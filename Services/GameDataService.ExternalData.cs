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
    private void loadAll()
    {
        invalidLoadPaths.Clear();
        pendingWorldDirectoryMoves.Clear();
        archivedOriginWorldDirectories.Clear();
        mapActorTagIndexes.Clear();
        mapAccessOrder.Clear();
        mapLoadedBytes.Clear();
        nextMapAccessOrder = 0;
        foreach (KeyValuePair<string, DataSection> pair in sections)
        {
            pair.Value.Data.Clear();
            if (!pair.Value.Persist || pair.Key is "Maps" or "WorldMaps")
                continue;
            string root = Path.Combine(ProjectPath, "Data", pair.Key);
            if (!Directory.Exists(root))
                continue;
            foreach (string path in Directory.EnumerateFiles(root, "*.json", SearchOption.AllDirectories)
                         .Where(path => !DataConfig.isAnimationCache(path))
                         .OrderBy(path => path, StringComparer.Ordinal))
            {
                if (pair.Key == "UI" && !hasDataFileExtension(pair.Key, path))
                {
                    invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                    continue;
                }
                try
                {
                    if (JsonNode.Parse(File.ReadAllText(path)) is not JsonObject data)
                    {
                        if (pair.Key is "TextConfigs" or "UI")
                            invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                        continue;
                    }
                    string? type = data["type"] is JsonValue typeValue
                        && typeValue.TryGetValue<string>(out string? parsedType)
                            ? parsedType
                            : null;
                    if (!pair.Value.AcceptsType(type))
                    {
                        if (pair.Key is "TextConfigs" or "UI")
                            invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                        continue;
                    }
                    if (!pair.Value.PreserveType)
                        data.Remove("type");
                    string relativePath = Path.GetRelativePath(root, path);
                    string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
                    pair.Value.Data[key] = data;
                }
                catch (JsonException)
                {
                    invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                }
            }
        }
        loadMapsAndWorldMaps();
        originData = cloneAllData();
        isModified = false;
        clearHistoryGesture();
        undoStack.Clear();
        redoStack.Clear();
        cleanupUnreferencedWorldHistoryDirectories();
        UndoRedoStateChanged?.Invoke(this, EventArgs.Empty);
    }

    private string? getDataKey(string absolutePath)
    {
        string fullPath = Path.GetFullPath(absolutePath);
        if (tryGetMapsRelativePath(fullPath, out string mapsRelative))
        {
            if (string.Equals(Path.GetFileName(fullPath), "_world.json", StringComparison.OrdinalIgnoreCase))
                return Path.GetDirectoryName(mapsRelative)?.Replace('\\', '/');
            return Path.ChangeExtension(mapsRelative, null)?.Replace('\\', '/');
        }
        foreach (KeyValuePair<string, DataSection> pair in sections)
        {
            if (!pair.Value.Persist || pair.Key is "Maps" or "WorldMaps")
                continue;
            string root = Path.Combine(ProjectPath, "Data", pair.Key);
            if (fullPath.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                return Path.ChangeExtension(Path.GetRelativePath(root, fullPath), null)?.Replace('\\', '/');
        }
        return null;
    }

    private Dictionary<string, string> createUiAssetMoveMap(
        IReadOnlyList<(string OldPath, string NewPath)> movedPaths)
    {
        Dictionary<string, string> result = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach ((string oldPath, string newPath) in movedPaths)
        {
            if (!tryGetDataLocation(oldPath, out string oldSection, out string oldRelative)
                || !tryGetDataLocation(newPath, out string newSection, out string newRelative)
                || oldSection != "UI"
                || newSection != "UI")
            {
                continue;
            }
            if (Directory.Exists(newPath))
            {
                string oldPrefix = normalizeDataKey(oldRelative);
                string newPrefix = normalizeDataKey(newRelative);
                string[] keys = collectUiAssetKeysAcrossHistory()
                    .Where(key => keyMatchesPrefix(key, oldPrefix))
                    .ToArray();
                foreach (string key in keys)
                {
                    string suffix = oldPrefix.Length == 0
                        ? key
                        : key[oldPrefix.Length..].TrimStart('/');
                    string destinationKey = newPrefix.Length == 0
                        ? suffix
                        : suffix.Length == 0 ? newPrefix : newPrefix + "/" + suffix;
                    addUiAssetMove(result, key, destinationKey);
                }
                continue;
            }
            if (!string.Equals(
                    Path.GetExtension(oldPath),
                    DataConfig.DataFileExtension,
                    StringComparison.Ordinal)
                || !string.Equals(
                    Path.GetExtension(newPath),
                    DataConfig.DataFileExtension,
                    StringComparison.Ordinal))
            {
                continue;
            }
            string oldKey = Path.ChangeExtension(oldRelative, null)!.Replace('\\', '/');
            string newKey = Path.ChangeExtension(newRelative, null)!.Replace('\\', '/');
            addUiAssetMove(result, oldKey, newKey);
        }
        return result;
    }

    private static void addUiAssetMove(
        IDictionary<string, string> moves,
        string oldKey,
        string newKey)
    {
        string oldLogicalKey = UiAssetSchema.ToLogicalAssetKey(oldKey);
        string newLogicalKey = UiAssetSchema.ToLogicalAssetKey(newKey);
        if (oldLogicalKey.Length != 0 && newLogicalKey.Length != 0)
            moves[oldLogicalKey] = newLogicalKey;
    }

    private HashSet<string> collectUiAssetKeysAcrossHistory()
    {
        HashSet<string> result = new HashSet<string>(
            sections["UI"].Data.Keys,
            StringComparer.Ordinal);
        result.UnionWith(originData["UI"].Keys);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            result.UnionWith(snapshot["UI"].Keys);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            result.UnionWith(snapshot["UI"].Keys);
        return result;
    }

    private void remapUiAssetReferences(
        IReadOnlyDictionary<string, string> moves)
    {
        rewriteUiAssetReferences(originData["UI"], moves);
        rewriteUiAssetReferences(sections["UI"].Data, moves);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            rewriteUiAssetReferences(snapshot["UI"], moves);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            rewriteUiAssetReferences(snapshot["UI"], moves);
    }

    private static HashSet<string> rewriteUiAssetReferences(
        IReadOnlyDictionary<string, JsonObject> assets,
        IReadOnlyDictionary<string, string> moves)
    {
        HashSet<string> changed = new HashSet<string>(StringComparer.Ordinal);
        if (moves.Count == 0)
            return changed;
        foreach (KeyValuePair<string, JsonObject> pair in assets)
        {
            foreach (JsonObject node in UiAssetSchema.EnumerateNodes(pair.Value))
            {
                string? controlId = getString(node["controlId"]);
                if (controlId is null
                    || !controlId.StartsWith(UiAssetSchema.ProjectControlPrefix, StringComparison.Ordinal))
                {
                    continue;
                }
                string oldKey = controlId[UiAssetSchema.ProjectControlPrefix.Length..];
                if (!moves.TryGetValue(oldKey, out string? newKey))
                    continue;
                node["controlId"] = UiAssetSchema.ProjectControlPrefix + newKey;
                changed.Add(pair.Key);
            }
        }
        return changed;
    }

    private void writeUiDiskAssetReferences(
        IReadOnlyDictionary<string, string> moves,
        IDictionary<string, string> originals)
    {
        Dictionary<string, string> replacements = new Dictionary<string, string>(StringComparer.Ordinal);
        string assetsRoot = Path.Combine(ProjectPath, "Data", "UI", "Assets");
        if (!Directory.Exists(assetsRoot))
            throw new IOException($"UI assets directory was not found after move: {assetsRoot}");
        foreach (string path in Directory.EnumerateFiles(
                     assetsRoot,
                     "*",
                     SearchOption.AllDirectories)
                 .Where(path => string.Equals(
                     Path.GetExtension(path),
                     DataConfig.DataFileExtension,
                     StringComparison.Ordinal))
                 .OrderBy(path => path, StringComparer.Ordinal))
        {
            string original = File.ReadAllText(path);
            if (JsonNode.Parse(original) is not JsonObject diskAsset)
                throw new IOException($"UI asset file root must be an object: {path}");
            Dictionary<string, JsonObject> diskAssets = new Dictionary<string, JsonObject>(StringComparer.Ordinal)
            {
                [path] = diskAsset,
            };
            if (rewriteUiAssetReferences(diskAssets, moves).Count == 0)
                continue;
            originals[path] = original;
            replacements[path] = diskAsset.ToJsonString(WriteOptions) + Environment.NewLine;
        }
        foreach (KeyValuePair<string, string> pair in replacements)
            writeTextAtomically(pair.Key, pair.Value);
    }

    private static void restoreUiOriginAssets(
        IReadOnlyDictionary<string, string> originals)
    {
        List<string> failures = [];
        foreach (KeyValuePair<string, string> pair in originals)
        {
            try
            {
                writeTextAtomically(pair.Key, pair.Value);
            }
            catch (Exception exception) when (
                exception is IOException
                or UnauthorizedAccessException
                or System.Security.SecurityException
                or NotSupportedException)
            {
                failures.Add(pair.Key + ": " + exception.Message);
            }
        }
        if (failures.Count != 0)
            throw new IOException(string.Join(Environment.NewLine, failures));
    }

    private static void writeTextAtomically(string path, string content)
    {
        string directory = Path.GetDirectoryName(path)!;
        string temporaryPath = Path.Combine(
            directory,
            $".{Path.GetFileName(path)}.{Guid.NewGuid():N}.tmp");
        try
        {
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
                writer.Write(content);
                writer.Flush();
                stream.Flush(true);
            }
            File.Move(temporaryPath, path, true);
        }
        catch
        {
            deleteTemporaryFile(temporaryPath);
            throw;
        }
    }

    private bool applyExternalMove(string oldPath, string newPath)
    {
        bool directory = Directory.Exists(newPath);
        bool file = File.Exists(newPath);
        if (!directory && !file)
            return false;
        if (tryApplyExternalMapMove(oldPath, newPath, directory, out bool mapChanged))
            return mapChanged;
        bool hasOldLocation = tryGetDataLocation(oldPath, out string oldSection, out string oldRelative);
        bool hasNewLocation = tryGetDataLocation(newPath, out string newSection, out string newRelative);
        if (directory)
        {
            if (hasOldLocation && hasNewLocation)
            {
                if (oldSection == newSection)
                {
                    return moveDataPrefix(
                        oldSection,
                        normalizeDataKey(oldRelative),
                        newSection,
                        normalizeDataKey(newRelative));
                }
                bool removed = removeDataPrefix(oldSection, normalizeDataKey(oldRelative));
                return applyExternalAdd(newPath) || removed;
            }
            if (hasOldLocation)
                return removeDataPrefix(oldSection, normalizeDataKey(oldRelative));
            return hasNewLocation && applyExternalAdd(newPath);
        }
        bool oldJson = hasOldLocation
            && hasDataFileExtension(oldSection, oldPath);
        bool newJson = hasNewLocation
            && hasDataFileExtension(newSection, newPath);
        if (oldJson && newJson)
        {
            string oldKey = Path.ChangeExtension(oldRelative, null)!.Replace('\\', '/');
            string newKey = Path.ChangeExtension(newRelative, null)!.Replace('\\', '/');
            if (oldSection != newSection)
            {
                bool removed = removeDataKey(oldSection, oldKey);
                return applyExternalAdd(newPath) || removed;
            }
            return moveDataKey(oldSection, oldKey, newSection, newKey);
        }
        if (oldJson)
        {
            string oldKey = Path.ChangeExtension(oldRelative, null)!.Replace('\\', '/');
            return removeDataKey(oldSection, oldKey);
        }
        return newJson && applyExternalAdd(newPath);
    }

    private bool applyExternalDelete(string path)
    {
        if (!tryGetDataLocation(path, out string sectionName, out string relativePath))
            return false;
        if (hasDataFileExtension(sectionName, path))
        {
            string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
            bool removed = removeDataKey(sectionName, key);
            return removeDataPrefix(sectionName, normalizeDataKey(relativePath)) || removed;
        }
        return removeDataPrefix(sectionName, normalizeDataKey(relativePath));
    }

    private bool applyExternalAdd(string path)
    {
        if (Directory.Exists(path))
        {
            bool changed = false;
            foreach (string filePath in Directory.EnumerateFiles(path, "*.json", SearchOption.AllDirectories)
                         .Where(filePath => !DataConfig.isAnimationCache(filePath)))
            {
                changed |= applyExternalAdd(filePath);
            }
            return changed;
        }
        if (!File.Exists(path)
            || DataConfig.isAnimationCache(path)
            || !tryGetDataLocation(path, out string sectionName, out string relativePath)
            || !hasDataFileExtension(sectionName, path))
        {
            return false;
        }
        JsonObject? data = readExternalDataFile(path, sections[sectionName]);
        if (data is null)
            return false;
        string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
        setDataKeyAcrossHistory(sectionName, key, data);
        return true;
    }

    private JsonObject? readExternalDataFile(string path, DataSection section)
    {
        try
        {
            if (JsonNode.Parse(File.ReadAllText(path)) is not JsonObject data)
                return null;
            string? type = data["type"] is JsonValue typeValue
                && typeValue.TryGetValue<string>(out string? parsedType)
                    ? parsedType
                    : null;
            if (!section.AcceptsType(type))
                return null;
            if (!section.PreserveType)
                data.Remove("type");
            return data;
        }
        catch (JsonException)
        {
            return null;
        }
        catch (IOException)
        {
            return null;
        }
        catch (UnauthorizedAccessException)
        {
            return null;
        }
    }

    private bool moveDataKey(string oldSection, string oldKey, string newSection, string newKey)
    {
        bool changed = moveDataKey(sections[oldSection].Data, oldKey, sections[newSection].Data, newKey);
        changed |= moveDataKey(originData[oldSection], oldKey, originData[newSection], newKey);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            moveDataKey(snapshot[oldSection], oldKey, snapshot[newSection], newKey);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            moveDataKey(snapshot[oldSection], oldKey, snapshot[newSection], newKey);
        return changed;
    }

    private static bool moveDataKey(
        Dictionary<string, JsonObject> oldData,
        string oldKey,
        Dictionary<string, JsonObject> newData,
        string newKey)
    {
        if (!oldData.TryGetValue(oldKey, out JsonObject? value))
            return false;
        if ((!ReferenceEquals(oldData, newData)
                || !string.Equals(oldKey, newKey, StringComparison.Ordinal))
            && newData.ContainsKey(newKey))
        {
            throw new InvalidOperationException($"Data move target already exists: {newKey}");
        }
        oldData.Remove(oldKey);
        newData[newKey] = value;
        return true;
    }

    private bool moveDataPrefix(
        string oldSection,
        string oldPrefix,
        string newSection,
        string newPrefix)
    {
        bool changed = moveDataPrefix(
            sections[oldSection].Data,
            oldPrefix,
            sections[newSection].Data,
            newPrefix);
        changed |= moveDataPrefix(originData[oldSection], oldPrefix, originData[newSection], newPrefix);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            moveDataPrefix(snapshot[oldSection], oldPrefix, snapshot[newSection], newPrefix);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            moveDataPrefix(snapshot[oldSection], oldPrefix, snapshot[newSection], newPrefix);
        return changed;
    }

    private static bool moveDataPrefix(
        Dictionary<string, JsonObject> oldData,
        string oldPrefix,
        Dictionary<string, JsonObject> newData,
        string newPrefix)
    {
        string[] keys = oldData.Keys.Where(key => keyMatchesPrefix(key, oldPrefix)).ToArray();
        HashSet<string> sourceKeys = keys.ToHashSet(StringComparer.Ordinal);
        HashSet<string> destinationKeys = new HashSet<string>(StringComparer.Ordinal);
        List<(string DestinationKey, JsonObject Value)> moved = [];
        foreach (string key in keys)
        {
            string suffix = oldPrefix.Length == 0 ? key : key[oldPrefix.Length..].TrimStart('/');
            string destinationKey = newPrefix.Length == 0
                ? suffix
                : suffix.Length == 0 ? newPrefix : newPrefix + "/" + suffix;
            if (!destinationKeys.Add(destinationKey)
                || newData.ContainsKey(destinationKey)
                    && (!ReferenceEquals(oldData, newData)
                        || !sourceKeys.Contains(destinationKey)))
            {
                throw new InvalidOperationException($"Data move target already exists: {destinationKey}");
            }
            moved.Add((destinationKey, oldData[key]));
        }
        foreach (string key in keys)
            oldData.Remove(key);
        foreach ((string destinationKey, JsonObject value) in moved)
            newData[destinationKey] = value;
        return keys.Length != 0;
    }

    private bool removeDataKey(string sectionName, string key)
    {
        bool changed = sections[sectionName].Data.Remove(key);
        changed |= originData[sectionName].Remove(key);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            changed |= snapshot[sectionName].Remove(key);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            changed |= snapshot[sectionName].Remove(key);
        return changed;
    }

    private bool removeDataPrefix(string sectionName, string prefix)
    {
        bool changed = removeDataPrefix(sections[sectionName].Data, prefix);
        changed |= removeDataPrefix(originData[sectionName], prefix);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            changed |= removeDataPrefix(snapshot[sectionName], prefix);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            changed |= removeDataPrefix(snapshot[sectionName], prefix);
        return changed;
    }

    private static bool removeDataPrefix(Dictionary<string, JsonObject> data, string prefix)
    {
        string[] keys = data.Keys.Where(key => keyMatchesPrefix(key, prefix)).ToArray();
        foreach (string key in keys)
            data.Remove(key);
        return keys.Length != 0;
    }

    private void setDataKeyAcrossHistory(string sectionName, string key, JsonObject data)
    {
        sections[sectionName].Data[key] = (JsonObject)data.DeepClone();
        originData[sectionName][key] = (JsonObject)data.DeepClone();
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            snapshot[sectionName][key] = (JsonObject)data.DeepClone();
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            snapshot[sectionName][key] = (JsonObject)data.DeepClone();
    }

    private bool isDataPath(string path)
    {
        return tryGetDataLocation(path, out _, out _);
    }

    private bool tryGetDataLocation(string path, out string sectionName, out string relativePath)
    {
        string fullPath = Path.GetFullPath(path);
        if (tryGetMapsRelativePath(fullPath, out string mapsRelative))
        {
            if (string.Equals(Path.GetFileName(fullPath), "_world.json", StringComparison.OrdinalIgnoreCase))
            {
                sectionName = "WorldMaps";
                relativePath = Path.GetDirectoryName(mapsRelative) ?? string.Empty;
            }
            else
            {
                sectionName = "Maps";
                relativePath = mapsRelative;
            }
            return true;
        }
        foreach (KeyValuePair<string, DataSection> pair in sections)
        {
            if (!pair.Value.Persist || pair.Key is "Maps" or "WorldMaps")
                continue;
            string root = Path.GetFullPath(Path.Combine(ProjectPath, "Data", pair.Key));
            string relative = Path.GetRelativePath(root, fullPath);
            if (Path.IsPathRooted(relative)
                || relative == ".."
                || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal))
            {
                continue;
            }
            sectionName = pair.Key;
            relativePath = relative == "." ? string.Empty : relative;
            return true;
        }
        sectionName = string.Empty;
        relativePath = string.Empty;
        return false;
    }

    private static bool keyMatchesPrefix(string key, string prefix)
    {
        return prefix.Length == 0
            || string.Equals(key, prefix, StringComparison.Ordinal)
            || key.StartsWith(prefix + "/", StringComparison.Ordinal);
    }

    private bool tryGetSectionAndKey(string absolutePath, out DataSection section, out string key)
    {
        section = null!;
        key = string.Empty;
        string fullPath = Path.GetFullPath(absolutePath);
        if (tryGetMapsRelativePath(fullPath, out string mapsRelative))
        {
            bool manifest = string.Equals(
                Path.GetFileName(fullPath),
                "_world.json",
                StringComparison.OrdinalIgnoreCase);
            section = sections[manifest ? "WorldMaps" : "Maps"];
            key = manifest
                ? (Path.GetDirectoryName(mapsRelative) ?? string.Empty).Replace('\\', '/')
                : Path.ChangeExtension(mapsRelative, null)!.Replace('\\', '/');
            return true;
        }
        foreach (KeyValuePair<string, DataSection> pair in sections)
        {
            if (!pair.Value.Persist || pair.Key is "Maps" or "WorldMaps")
                continue;
            string root = Path.Combine(ProjectPath, "Data", pair.Key);
            if (!fullPath.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                continue;
            section = pair.Value;
            key = Path.ChangeExtension(Path.GetRelativePath(root, fullPath), null)!.Replace('\\', '/');
            return true;
        }
        return false;
    }

}

