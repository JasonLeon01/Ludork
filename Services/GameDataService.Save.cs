using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using Ludork.Models;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    public bool CreateGeneralType(string key, string? linkedType = null)
    {
        if (string.IsNullOrWhiteSpace(key) || sections["General"].Data.ContainsKey(key))
            return false;
        RecordSnapshot();
        JsonObject entry = new()
        {
            ["params"] = new JsonObject(),
            ["members"] = new JsonObject(),
        };
        if (!string.IsNullOrWhiteSpace(linkedType))
            entry["linkedType"] = linkedType;
        sections["General"].Data[key] = entry;
        refreshModifiedState();
        return true;
    }

    public bool RenameGeneralType(string oldKey, string newKey)
    {
        Dictionary<string, JsonObject> data = sections["General"].Data;
        if (string.IsNullOrWhiteSpace(newKey) || !data.ContainsKey(oldKey) || data.ContainsKey(newKey))
            return false;
        RecordSnapshot();
        List<KeyValuePair<string, JsonObject>> entries = data.Select(e => e).ToList();
        data.Clear();
        foreach (KeyValuePair<string, JsonObject> entry in entries)
            data.Add(entry.Key == oldKey ? newKey : entry.Key, entry.Value);
        refreshModifiedState();
        return true;
    }

    public bool DeleteGeneralType(string key)
    {
        Dictionary<string, JsonObject> data = sections["General"].Data;
        if (!data.ContainsKey(key))
            return false;
        RecordSnapshot();
        data.Remove(key);
        refreshModifiedState();
        return true;
    }

    public SaveResult SaveAllModified()
    {
        BreakHistoryGesture();
        WorldMapMutationResult worldValidation = ValidateAllWorldMaps();
        if (!worldValidation.Success)
            return new SaveResult(false, worldValidation.Details);
        List<string> added = [];
        List<string> updated = [];
        List<string> deleted = [];
        List<string> failed = [];
        List<(string Section, string Key, string Path)> pendingDeletions = [];
        List<(string WorldKey, string SourceDirectory, string BackupDirectory, bool Created)> worldArchives = [];
        string[] removedOriginWorldKeys = originData["WorldMaps"].Keys
            .Except(sections["WorldMaps"].Data.Keys, StringComparer.Ordinal)
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
        Dictionary<string, MapCatalogEntry> currentWorldChildren = getMapCatalogEntries()
            .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap)
            .ToDictionary(entry => entry.Key, entry => entry, StringComparer.Ordinal);
        Dictionary<string, MapCatalogEntry> originWorldChildren = originData["MapCatalog"].Values
            .Select(readMapCatalogEntry)
            .Where(entry => entry is { Kind: MapCatalogEntryKind.WorldChildMap })
            .Select(entry => entry!)
            .ToDictionary(entry => entry.Key, entry => entry, StringComparer.Ordinal);
        foreach (MapCatalogEntry child in currentWorldChildren.Values)
        {
            if (originWorldChildren.ContainsKey(child.Key)
                || child.WorldKey is null
                || !pendingWorldDirectoryMoves.TryGetValue(child.WorldKey, out string? sourceDirectory))
            {
                continue;
            }
            string suffix = child.Key[(child.WorldKey.Length + 1)..];
            string sourcePath = Path.Combine(sourceDirectory, suffix + ".json");
            string destinationPath = getMapDataPath(child.Key);
            try
            {
                if (sections["Maps"].Data.TryGetValue(child.Key, out JsonObject? loadedChild))
                {
                    JsonObject payload = (JsonObject)loadedChild.DeepClone();
                    payload["type"] = "map";
                    Directory.CreateDirectory(Path.GetDirectoryName(destinationPath)!);
                    File.WriteAllText(destinationPath, payload.ToJsonString(WriteOptions) + Environment.NewLine);
                    originData["Maps"][child.Key] = (JsonObject)loadedChild.DeepClone();
                    updateLoadedMapMetadata(child.Key, loadedChild, destinationPath);
                    added.Add(child.Key);
                    continue;
                }
                if (!File.Exists(sourcePath))
                    throw new FileNotFoundException("The source child map no longer exists.", sourcePath);
                Directory.CreateDirectory(Path.GetDirectoryName(destinationPath)!);
                copyFileAtomically(sourcePath, destinationPath);
                added.Add(child.Key);
            }
            catch (Exception)
            {
                failed.Add(child.Key);
            }
        }
        bool generalSaveFailed = false;
        foreach (KeyValuePair<string, DataSection> pair in sections)
        {
            if (!pair.Value.Persist)
                continue;
            Dictionary<string, JsonObject> originSection = originData[pair.Key];
            foreach (KeyValuePair<string, JsonObject> dataPair in pair.Value.Data)
            {
                if (originSection.TryGetValue(dataPair.Key, out JsonObject? origin) && nodesEqual(dataPair.Value, origin))
                    continue;
                try
                {
                    JsonObject payload = (JsonObject)dataPair.Value.DeepClone();
                    if (pair.Value.WriteType && pair.Value.ExpectedType is not null)
                        payload["type"] = pair.Value.ExpectedType;
                    string path = getSectionDataPath(pair.Key, dataPair.Key);
                    Directory.CreateDirectory(Path.GetDirectoryName(path)!);
                    File.WriteAllText(path, payload.ToJsonString(WriteOptions) + Environment.NewLine);
                    if (pair.Key == "Maps")
                        updateLoadedMapMetadata(dataPair.Key, dataPair.Value, path);
                    if (origin is null)
                        added.Add(dataPair.Key);
                    else
                        updated.Add(dataPair.Key);
                    originSection[dataPair.Key] = (JsonObject)dataPair.Value.DeepClone();
                }
                catch (Exception)
                {
                    failed.Add(dataPair.Key);
                    if (pair.Key == "General")
                        generalSaveFailed = true;
                }
            }
            foreach (string removedKey in originSection.Keys.Except(pair.Value.Data.Keys, StringComparer.Ordinal).ToArray())
            {
                pendingDeletions.Add((
                    pair.Key,
                    removedKey,
                    getSectionDataPath(pair.Key, removedKey)));
            }
        }
        foreach (MapCatalogEntry child in originWorldChildren.Values)
        {
            string path = getMapDataPath(child.Key);
            if (!currentWorldChildren.ContainsKey(child.Key)
                && !pendingDeletions.Any(item => string.Equals(item.Path, path, StringComparison.Ordinal)))
            {
                pendingDeletions.Add(("Maps", child.Key, path));
            }
        }
        if (!generalSaveFailed)
        {
            GeneralEnumSaveResult generalEnumResult = generalEnums.Save(sections["General"].Data);
            generalEnumSavePending = !generalEnumResult.Success;
            if (!generalEnumResult.Success)
                failed.Add("GeneralEnum (" + generalEnumResult.Detail + ")");
            else if (generalEnumResult.Changed)
            {
                updated.Add(GeneralEnumService.RuntimeRelativePath);
                updated.Add(GeneralEnumService.StubRelativePath);
            }
        }
        if (failed.Count == 0)
        {
            foreach (string worldKey in removedOriginWorldKeys)
            {
                if (!canArchiveRemovedWorldDirectory(worldKey, out string? archiveFailure))
                    failed.Add(worldKey + " (" + archiveFailure + ")");
            }
        }
        if (failed.Count == 0)
        {
            foreach ((string section, string key, string path) in pendingDeletions)
            {
                if (isRemovedWorldFilePath(path, removedOriginWorldKeys))
                    continue;
                try
                {
                    if (File.Exists(path))
                        File.Delete(path);
                    deleted.Add(key);
                    originData[section].Remove(key);
                }
                catch (Exception)
                {
                    failed.Add(key);
                }
            }
        }
        if (failed.Count == 0)
        {
            foreach (string worldKey in removedOriginWorldKeys)
            {
                if (tryArchiveRemovedWorldDirectory(
                        worldKey,
                        out string sourceDirectory,
                        out string backupDirectory,
                        out bool created,
                        out string? archiveFailure))
                {
                    worldArchives.Add((worldKey, sourceDirectory, backupDirectory, created));
                    continue;
                }
                failed.Add(worldKey + " (" + archiveFailure + ")");
                break;
            }
            if (failed.Count != 0)
            {
                foreach ((string worldKey, string sourceDirectory, string backupDirectory, bool created)
                         in worldArchives.AsEnumerable().Reverse())
                {
                    if (!created)
                        continue;
                    if (!tryRestoreArchivedOriginWorldDirectory(
                            worldKey,
                            sourceDirectory,
                            backupDirectory,
                            out string? restoreFailure))
                    {
                        retargetWorldHistorySources(worldKey, sourceDirectory, backupDirectory);
                        failed.Add(worldKey + " restore (" + restoreFailure + ")");
                    }
                }
            }
        }
        if (failed.Count == 0)
        {
            foreach ((string worldKey, string sourceDirectory, string backupDirectory, bool _) in worldArchives)
                retargetWorldHistorySources(worldKey, sourceDirectory, backupDirectory);
            foreach ((string section, string key, string path) in pendingDeletions)
            {
                if (!isRemovedWorldFilePath(path, removedOriginWorldKeys))
                    continue;
                deleted.Add(key);
                originData[section].Remove(key);
            }
        }
        if (failed.Count == 0)
        {
            originData["MapCatalog"] = sections["MapCatalog"].Data.ToDictionary(
                item => item.Key,
                item => (JsonObject)item.Value.DeepClone(),
                StringComparer.Ordinal);
            pendingWorldDirectoryMoves.Clear();
            archivedOriginWorldDirectories.Clear();
        }
        cleanupUnreferencedWorldHistoryDirectories();
        refreshModifiedState();
        DataSaved?.Invoke(this, EventArgs.Empty);
        return new SaveResult(failed.Count == 0, formatSaveDetails(added, updated, deleted, failed));
    }

    public SaveResult ReplaceMapLayerAndSave(
        string mapKey,
        string layerName,
        JsonArray tiles,
        JsonArray autoTiles)
    {
        BreakHistoryGesture();
        Dictionary<string, JsonObject> maps = sections["Maps"].Data;
        if (!maps.TryGetValue(mapKey, out JsonObject? current)
            || current["layers"]?[layerName] is not JsonObject)
        {
            return new SaveResult(false, "The map or layer no longer exists.");
        }

        JsonObject candidate = (JsonObject)current.DeepClone();
        if (candidate["layers"]?[layerName] is not JsonObject candidateLayer)
            return new SaveResult(false, "The map layer could not be copied.");
        candidateLayer["tiles"] = tiles.DeepClone();
        candidateLayer["autoTiles"] = autoTiles.DeepClone();

        JsonObject payload = (JsonObject)candidate.DeepClone();
        payload["type"] = "map";
        string mapsDirectory = Path.GetFullPath(Path.Combine(
            ProjectPath,
            "Data",
            "Maps"));
        string path = Path.GetFullPath(Path.Combine(
            mapsDirectory,
            mapKey.Replace('/', Path.DirectorySeparatorChar) + ".json"));
        StringComparison pathComparison = OperatingSystem.IsWindows()
            ? StringComparison.OrdinalIgnoreCase
            : StringComparison.Ordinal;
        if (!path.StartsWith(
            Path.TrimEndingDirectorySeparator(mapsDirectory)
                + Path.DirectorySeparatorChar,
            pathComparison))
        {
            return new SaveResult(false, "The map path is outside the project Maps directory.");
        }
        string directory = Path.GetDirectoryName(path)!;
        string temporaryPath = Path.Combine(
            directory,
            $".{Path.GetFileName(path)}.{Guid.NewGuid():N}.tmp");
        try
        {
            Directory.CreateDirectory(directory);
            using (FileStream stream = new(
                temporaryPath,
                FileMode.CreateNew,
                FileAccess.Write,
                FileShare.None))
            {
                using StreamWriter writer = new(
                    stream,
                    new UTF8Encoding(false),
                    1024,
                    true);
                writer.Write(payload.ToJsonString(WriteOptions));
                writer.Write(Environment.NewLine);
                writer.Flush();
                stream.Flush(true);
            }
            File.Move(temporaryPath, path, true);
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or System.Security.SecurityException
            or JsonException
            or NotSupportedException)
        {
            deleteTemporaryFile(temporaryPath);
            return new SaveResult(false, exception.Message);
        }

        RecordMapSnapshot(mapKey);
        maps[mapKey] = candidate;
        updateLoadedMapMetadata(mapKey, candidate, path);
        originData["Maps"][mapKey] = (JsonObject)candidate.DeepClone();
        NotifyMapContentChanged(mapKey);
        refreshModifiedState();
        DataSaved?.Invoke(this, EventArgs.Empty);
        return new SaveResult(true, string.Empty);
    }

    public void refreshModifiedState()
    {
        DataChanged?.Invoke(this, EventArgs.Empty);
        bool modified = generalEnumSavePending
            || sections.Any(section => !sectionEqual(section.Value.Data, originData[section.Key]));
        if (modified == isModified)
            return;
        isModified = modified;
        ModifiedChanged?.Invoke(this, EventArgs.Empty);
    }

    public void RecordSnapshot()
    {
        recordSnapshot(null);
    }

    public long BeginHistoryGesture()
    {
        clearHistoryGesture();
        nextHistoryGestureId += 1;
        if (nextHistoryGestureId == 0)
            nextHistoryGestureId = 1;
        activeHistoryGestureId = nextHistoryGestureId;
        return activeHistoryGestureId;
    }

    public void EndHistoryGesture(long gestureId)
    {
        if (gestureId == 0 || gestureId != activeHistoryGestureId)
            return;
        clearHistoryGesture();
    }

    public void BreakHistoryGesture()
    {
        activeHistoryGestureHasSnapshot = false;
    }

    internal bool IsHistoryGestureActive(long gestureId)
    {
        return gestureId != 0 && gestureId == activeHistoryGestureId;
    }

    public IReadOnlyList<string> Undo()
    {
        BreakHistoryGesture();
        if (undoStack.Count == 0)
            return Array.Empty<string>();
        Dictionary<string, Dictionary<string, JsonObject>> snapshot = undoStack.Pop();
        Dictionary<string, Dictionary<string, JsonObject>> current =
            tryGetWorldHistoryKey(snapshot, out string worldKey)
                ? cloneWorldHistory(worldKey)
                : cloneAllData();
        pushHistory(redoStack, current);
        IReadOnlyList<string> differences = getDiff(current, snapshot);
        restoreSnapshot(snapshot);
        cleanupUnreferencedWorldHistoryDirectories();
        return differences;
    }

    public IReadOnlyList<string> Redo()
    {
        BreakHistoryGesture();
        if (redoStack.Count == 0)
            return Array.Empty<string>();
        Dictionary<string, Dictionary<string, JsonObject>> snapshot = redoStack.Pop();
        Dictionary<string, Dictionary<string, JsonObject>> current =
            tryGetWorldHistoryKey(snapshot, out string worldKey)
                ? cloneWorldHistory(worldKey)
                : cloneAllData();
        pushHistory(undoStack, current);
        IReadOnlyList<string> differences = getDiff(current, snapshot);
        restoreSnapshot(snapshot);
        cleanupUnreferencedWorldHistoryDirectories();
        return differences;
    }

}

