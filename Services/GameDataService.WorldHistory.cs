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
    private Dictionary<string, Dictionary<string, JsonObject>> cloneWorldHistory(string worldKey)
    {
        Dictionary<string, Dictionary<string, JsonObject>> snapshot = sections.Keys.ToDictionary(
            key => key,
            _ => new Dictionary<string, JsonObject>(StringComparer.Ordinal),
            StringComparer.Ordinal);
        if (sections["WorldMaps"].Data.TryGetValue(worldKey, out JsonObject? world))
            snapshot["WorldMaps"][worldKey] = (JsonObject)world.DeepClone();
        string catalogKey = getMapCatalogDataKey(MapCatalogEntryKind.WorldMap, worldKey);
        if (sections["MapCatalog"].Data.TryGetValue(catalogKey, out JsonObject? catalog))
            snapshot["MapCatalog"][catalogKey] = (JsonObject)catalog.DeepClone();
        snapshot[WorldHistoryScope] = new Dictionary<string, JsonObject>(StringComparer.Ordinal)
        {
            [worldKey] = new JsonObject(),
        };
        if (pendingWorldDirectoryMoves.TryGetValue(worldKey, out string? sourceDirectory))
        {
            snapshot[WorldFileMovesScope] = new Dictionary<string, JsonObject>(StringComparer.Ordinal)
            {
                [worldKey] = new JsonObject { ["source"] = sourceDirectory },
            };
        }
        return snapshot;
    }

    private bool tryApplyExternalMapMove(
        string oldPath,
        string newPath,
        bool directory,
        out bool changed)
    {
        changed = false;
        if (!tryGetMapsRelativePath(Path.GetFullPath(oldPath), out string oldRelativePath)
            || !tryGetMapsRelativePath(Path.GetFullPath(newPath), out string newRelativePath))
        {
            return false;
        }
        string oldRelative = normalizeDataKey(oldRelativePath);
        string newRelative = normalizeDataKey(newRelativePath);
        if (directory)
        {
            if (oldRelative.Contains('/') || newRelative.Contains('/'))
                return true;
            changed |= moveDataKey("WorldMaps", oldRelative, "WorldMaps", newRelative);
            changed |= moveDataPrefix("Maps", oldRelative, "Maps", newRelative);
            rekeyLoadedMapMetadataPrefix(oldRelative, newRelative);
            moveMapCatalogPrefixAcrossHistory(oldRelative, newRelative);
            changed = true;
            mapActorTagIndexes.Clear();
            return true;
        }
        if (!string.Equals(Path.GetExtension(oldRelative), ".json", StringComparison.OrdinalIgnoreCase)
            || !string.Equals(Path.GetExtension(newRelative), ".json", StringComparison.OrdinalIgnoreCase)
            || string.Equals(Path.GetFileName(oldRelative), "_world.json", StringComparison.OrdinalIgnoreCase)
            || string.Equals(Path.GetFileName(newRelative), "_world.json", StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }
        string oldKey = normaliseMapKey(oldRelative);
        string newKey = normaliseMapKey(newRelative);
        changed |= moveDataKey("Maps", oldKey, "Maps", newKey);
        rekeyLoadedMapMetadata(oldKey, newKey);
        moveMapCatalogEntryAcrossHistory(oldKey, newKey);
        changed = true;
        int oldSeparator = oldKey.IndexOf('/');
        int newSeparator = newKey.IndexOf('/');
        if (oldSeparator > 0
            && newSeparator > 0
            && string.Equals(
                oldKey[..oldSeparator],
                newKey[..newSeparator],
                StringComparison.Ordinal))
        {
            string worldKey = oldKey[..oldSeparator];
            rewriteWorldPlacementMap(
                sections["WorldMaps"].Data,
                worldKey,
                Path.GetFileName(oldKey) + ".json",
                Path.GetFileName(newKey) + ".json");
            foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            {
                rewriteWorldPlacementMap(
                    snapshot["WorldMaps"],
                    worldKey,
                    Path.GetFileName(oldKey) + ".json",
                    Path.GetFileName(newKey) + ".json");
            }
            foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            {
                rewriteWorldPlacementMap(
                    snapshot["WorldMaps"],
                    worldKey,
                    Path.GetFileName(oldKey) + ".json",
                    Path.GetFileName(newKey) + ".json");
            }
            changed = true;
        }
        mapActorTagIndexes.Remove(oldKey);
        mapActorTagIndexes.Remove(newKey);
        return true;
    }

    private void moveMapCatalogEntryAcrossHistory(string oldKey, string newKey)
    {
        moveMapCatalogEntry(sections["MapCatalog"].Data, oldKey, newKey);
        moveMapCatalogEntry(originData["MapCatalog"], oldKey, newKey);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            moveMapCatalogEntry(snapshot["MapCatalog"], oldKey, newKey);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            moveMapCatalogEntry(snapshot["MapCatalog"], oldKey, newKey);
    }

    private static void moveMapCatalogEntry(
        Dictionary<string, JsonObject> catalog,
        string oldKey,
        string newKey)
    {
        string? catalogKey = catalog.FirstOrDefault(item =>
            string.Equals(getString(item.Value["key"]), oldKey, StringComparison.Ordinal)).Key;
        if (catalogKey is null || !catalog.Remove(catalogKey, out JsonObject? entry))
            return;
        string? kind = getString(entry["kind"]);
        entry["key"] = newKey;
        int separator = newKey.IndexOf('/');
        if (string.Equals(kind, MapCatalogEntryKind.WorldChildMap.ToString(), StringComparison.Ordinal)
            && separator > 0)
        {
            entry["worldKey"] = newKey[..separator];
        }
        catalog[kind + ":" + newKey] = entry;
    }

    private void moveMapCatalogPrefixAcrossHistory(string oldWorldKey, string newWorldKey)
    {
        moveMapCatalogPrefix(sections["MapCatalog"].Data, oldWorldKey, newWorldKey);
        moveMapCatalogPrefix(originData["MapCatalog"], oldWorldKey, newWorldKey);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
        {
            moveMapCatalogPrefix(snapshot["MapCatalog"], oldWorldKey, newWorldKey);
            moveWorldHistoryScope(snapshot, oldWorldKey, newWorldKey);
        }
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
        {
            moveMapCatalogPrefix(snapshot["MapCatalog"], oldWorldKey, newWorldKey);
            moveWorldHistoryScope(snapshot, oldWorldKey, newWorldKey);
        }
    }

    private static void moveWorldHistoryScope(
        Dictionary<string, Dictionary<string, JsonObject>> snapshot,
        string oldWorldKey,
        string newWorldKey)
    {
        if (snapshot.TryGetValue(WorldHistoryScope, out Dictionary<string, JsonObject>? scope)
            && scope.Remove(oldWorldKey, out JsonObject? value))
        {
            scope[newWorldKey] = value;
        }
    }

    private static void moveMapCatalogPrefix(
        Dictionary<string, JsonObject> catalog,
        string oldWorldKey,
        string newWorldKey)
    {
        JsonObject[] entries = catalog.Values
            .Where(entry =>
            {
                string? key = getString(entry["key"]);
                return key is not null && keyMatchesPrefix(key, oldWorldKey);
            })
            .ToArray();
        foreach (JsonObject entry in entries)
        {
            string oldKey = getString(entry["key"])!;
            string suffix = oldKey.Length == oldWorldKey.Length
                ? string.Empty
                : oldKey[oldWorldKey.Length..];
            moveMapCatalogEntry(catalog, oldKey, newWorldKey + suffix);
        }
    }

    private static void rewriteWorldPlacementMap(
        Dictionary<string, JsonObject> worlds,
        string worldKey,
        string oldMap,
        string newMap)
    {
        if (!worlds.TryGetValue(worldKey, out JsonObject? world)
            || world["placements"] is not JsonArray placements)
        {
            return;
        }
        foreach (JsonObject placement in placements.OfType<JsonObject>())
        {
            if (string.Equals(getString(placement["map"]), oldMap, StringComparison.Ordinal))
                placement["map"] = newMap;
        }
    }

    private bool canArchiveRemovedWorldDirectory(string worldKey, out string failure)
    {
        string sourceDirectory = getWorldDirectory(worldKey);
        if (archivedOriginWorldDirectories.TryGetValue(worldKey, out string? archivedDirectory)
            && isWorldHistoryBackupDirectory(archivedDirectory)
            && Directory.Exists(archivedDirectory))
        {
            if (Directory.Exists(sourceDirectory))
            {
                failure = "the original world path is occupied while its history source is retained";
                return false;
            }
            failure = string.Empty;
            return true;
        }
        try
        {
            if (!MapPathPolicy.IsWorldDirectory(sourceDirectory)
                || !worldDirectoryMatchesOriginCatalog(worldKey, sourceDirectory))
            {
                failure = "the original world directory contains missing, nested, or unmanaged entries";
                return false;
            }
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or System.Security.SecurityException)
        {
            failure = exception.Message;
            return false;
        }
        failure = string.Empty;
        return true;
    }

    private bool tryArchiveRemovedWorldDirectory(
        string worldKey,
        out string sourceDirectory,
        out string backupDirectory,
        out bool created,
        out string failure)
    {
        sourceDirectory = getWorldDirectory(worldKey);
        if (archivedOriginWorldDirectories.TryGetValue(worldKey, out string? archivedDirectory)
            && isWorldHistoryBackupDirectory(archivedDirectory)
            && Directory.Exists(archivedDirectory))
        {
            if (Directory.Exists(sourceDirectory))
            {
                backupDirectory = archivedDirectory;
                created = false;
                failure = "the original world path is occupied while its history source is retained";
                return false;
            }
            backupDirectory = archivedDirectory;
            created = false;
            failure = string.Empty;
            return true;
        }
        archivedOriginWorldDirectories.Remove(worldKey);
        backupDirectory = Path.Combine(worldHistorySessionRoot, Guid.NewGuid().ToString("N"));
        created = false;
        if (!canArchiveRemovedWorldDirectory(worldKey, out failure))
            return false;
        try
        {
            Directory.CreateDirectory(worldHistorySessionRoot);
            Directory.Move(sourceDirectory, backupDirectory);
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or System.Security.SecurityException)
        {
            if (!Directory.Exists(sourceDirectory) && Directory.Exists(backupDirectory))
            {
                archivedOriginWorldDirectories[worldKey] = backupDirectory;
                created = true;
                failure = string.Empty;
                return true;
            }
            failure = exception.Message;
            return false;
        }
        archivedOriginWorldDirectories[worldKey] = backupDirectory;
        created = true;
        failure = string.Empty;
        return true;
    }

    private bool tryRestoreArchivedOriginWorldDirectory(
        string worldKey,
        string sourceDirectory,
        string backupDirectory,
        out string failure)
    {
        try
        {
            if (!Directory.Exists(backupDirectory))
            {
                if (Directory.Exists(sourceDirectory))
                {
                    archivedOriginWorldDirectories.Remove(worldKey);
                    failure = string.Empty;
                    return true;
                }
                failure = "both the original and history directories are missing";
                return false;
            }
            if (Directory.Exists(sourceDirectory))
            {
                failure = "the original world directory is already occupied";
                return false;
            }
            Directory.CreateDirectory(Path.GetDirectoryName(sourceDirectory)!);
            Directory.Move(backupDirectory, sourceDirectory);
            archivedOriginWorldDirectories.Remove(worldKey);
            failure = string.Empty;
            return true;
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or System.Security.SecurityException)
        {
            failure = exception.Message;
            return false;
        }
    }

    private bool isRemovedWorldFilePath(string path, IReadOnlyCollection<string> removedWorldKeys)
    {
        string? directory = Path.GetDirectoryName(Path.GetFullPath(path));
        return directory is not null
            && removedWorldKeys.Any(worldKey => pathsEqual(directory, getWorldDirectory(worldKey)));
    }

    private bool worldDirectoryMatchesOriginCatalog(string worldKey, string directory)
    {
        if ((File.GetAttributes(directory) & FileAttributes.ReparsePoint) != 0)
            return false;
        HashSet<string> expectedPaths = new(getPathComparer())
        {
            Path.GetFullPath(Path.Combine(directory, "_world.json")),
        };
        foreach (MapCatalogEntry child in originData["MapCatalog"].Values
                     .Select(readMapCatalogEntry)
                     .Where(entry => entry is
                     {
                         Kind: MapCatalogEntryKind.WorldChildMap,
                         WorldKey: not null,
                     } && string.Equals(entry.WorldKey, worldKey, StringComparison.Ordinal))
                     .Select(entry => entry!))
        {
            string suffix = child.Key[(worldKey.Length + 1)..];
            if (!isValidMapChildName(suffix))
                return false;
            expectedPaths.Add(Path.GetFullPath(Path.Combine(directory, suffix + ".json")));
        }
        if (Directory.EnumerateDirectories(directory, "*", SearchOption.TopDirectoryOnly).Any())
            return false;
        string[] actualPaths = Directory.EnumerateFiles(directory, "*", SearchOption.TopDirectoryOnly)
            .Where(path => string.Equals(
                Path.GetExtension(path),
                ".json",
                StringComparison.OrdinalIgnoreCase))
            .Select(Path.GetFullPath)
            .ToArray();
        if (actualPaths.Length != expectedPaths.Count
            || actualPaths.Any(path => !expectedPaths.Contains(path)))
        {
            return false;
        }
        foreach (string path in actualPaths)
        {
            if (!File.Exists(path)
                || (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
            {
                return false;
            }
        }
        return true;
    }

    private void retargetWorldHistorySources(
        string worldKey,
        string sourceDirectory,
        string backupDirectory)
    {
        foreach (string key in pendingWorldDirectoryMoves.Keys.ToArray())
        {
            if (pathsEqual(pendingWorldDirectoryMoves[key], sourceDirectory))
                pendingWorldDirectoryMoves[key] = backupDirectory;
        }
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
            retargetWorldHistorySource(snapshot, worldKey, sourceDirectory, backupDirectory);
        foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
            retargetWorldHistorySource(snapshot, worldKey, sourceDirectory, backupDirectory);
    }

    private static void retargetWorldHistorySource(
        Dictionary<string, Dictionary<string, JsonObject>> snapshot,
        string worldKey,
        string sourceDirectory,
        string backupDirectory)
    {
        if (!snapshot.TryGetValue(WorldFileMovesScope, out Dictionary<string, JsonObject>? sources))
        {
            sources = new Dictionary<string, JsonObject>(StringComparer.Ordinal);
            snapshot[WorldFileMovesScope] = sources;
        }
        foreach (KeyValuePair<string, JsonObject> source in sources.ToArray())
        {
            string? path = getString(source.Value["source"]);
            if (path is not null && pathsEqual(path, sourceDirectory))
                sources[source.Key] = new JsonObject { ["source"] = backupDirectory };
        }
        if (snapshot.TryGetValue("WorldMaps", out Dictionary<string, JsonObject>? worlds)
            && worlds.ContainsKey(worldKey)
            && !sources.ContainsKey(worldKey))
        {
            sources[worldKey] = new JsonObject { ["source"] = backupDirectory };
        }
    }

    private void cleanupUnreferencedWorldHistoryDirectories()
    {
        try
        {
            if (!Directory.Exists(worldHistorySessionRoot))
                return;
            HashSet<string> referenced = new(getPathComparer());
            foreach (string source in pendingWorldDirectoryMoves.Values)
                addWorldHistoryReference(referenced, source);
            foreach (string source in archivedOriginWorldDirectories.Values)
                addWorldHistoryReference(referenced, source);
            foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in undoStack)
                addWorldHistoryReferences(referenced, snapshot);
            foreach (Dictionary<string, Dictionary<string, JsonObject>> snapshot in redoStack)
                addWorldHistoryReferences(referenced, snapshot);
            foreach (string directory in Directory.EnumerateDirectories(
                         worldHistorySessionRoot,
                         "*",
                         SearchOption.TopDirectoryOnly))
            {
                string fullPath = Path.GetFullPath(directory);
                if (!referenced.Contains(fullPath))
                    deleteWorldHistoryDirectory(fullPath);
            }
            if (Directory.Exists(worldHistorySessionRoot)
                && !Directory.EnumerateFileSystemEntries(worldHistorySessionRoot).Any())
            {
                deleteWorldHistoryDirectory(worldHistorySessionRoot);
            }
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or System.Security.SecurityException)
        {
        }
    }

    private void addWorldHistoryReference(HashSet<string> referenced, string source)
    {
        if (isWorldHistoryBackupDirectory(source))
            referenced.Add(Path.GetFullPath(source));
    }

    private void addWorldHistoryReferences(
        HashSet<string> referenced,
        IReadOnlyDictionary<string, Dictionary<string, JsonObject>> snapshot)
    {
        if (!snapshot.TryGetValue(WorldFileMovesScope, out Dictionary<string, JsonObject>? sources))
            return;
        foreach (JsonObject source in sources.Values)
        {
            string? path = getString(source["source"]);
            if (path is not null)
                addWorldHistoryReference(referenced, path);
        }
    }

    private bool isWorldHistoryBackupDirectory(string path)
    {
        string fullPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
        string? parent = Path.GetDirectoryName(fullPath);
        return parent is not null && pathsEqual(parent, worldHistorySessionRoot);
    }

    private void deleteWorldHistoryDirectory(string path)
    {
        string fullPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
        if (!pathsEqual(fullPath, worldHistorySessionRoot)
            && !isWorldHistoryBackupDirectory(fullPath))
        {
            return;
        }
        try
        {
            if (Directory.Exists(fullPath))
                Directory.Delete(fullPath, true);
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or System.Security.SecurityException)
        {
        }
    }

    private static bool tryGetWorldHistoryKey(
        IReadOnlyDictionary<string, Dictionary<string, JsonObject>> snapshot,
        out string worldKey)
    {
        if (snapshot.TryGetValue(WorldHistoryScope, out Dictionary<string, JsonObject>? scope)
            && scope.Count == 1)
        {
            worldKey = scope.Keys.First();
            return true;
        }
        worldKey = string.Empty;
        return false;
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        foreach (KeyValuePair<string, string> archive in archivedOriginWorldDirectories.ToArray())
        {
            tryRestoreArchivedOriginWorldDirectory(
                archive.Key,
                getWorldDirectory(archive.Key),
                archive.Value,
                out _);
        }
        undoStack.Clear();
        redoStack.Clear();
        pendingWorldDirectoryMoves.Clear();
        cleanupUnreferencedWorldHistoryDirectories();
    }

    public void RecordMapSnapshot(string mapKey)
    {
        recordSnapshot(new HashSet<string>([normaliseMapKey(mapKey)], StringComparer.Ordinal));
    }

    public void RecordWorldSnapshot(string worldKey)
    {
        worldKey = normalizeWorldKey(worldKey);
        pushHistorySnapshot(cloneWorldHistory(worldKey));
    }

    private void recordSnapshot(IReadOnlySet<string>? retainedMapKeys)
    {
        pushHistorySnapshot(cloneAllData(retainedMapKeys));
    }

    private void pushHistorySnapshot(
        Dictionary<string, Dictionary<string, JsonObject>> snapshot)
    {
        if (activeHistoryGestureId != 0 && activeHistoryGestureHasSnapshot)
            return;
        pushHistory(undoStack, snapshot);
        if (activeHistoryGestureId != 0)
            activeHistoryGestureHasSnapshot = true;
        redoStack.Clear();
        cleanupUnreferencedWorldHistoryDirectories();
        UndoRedoStateChanged?.Invoke(this, EventArgs.Empty);
    }

    private static void copyFileAtomically(string sourcePath, string destinationPath)
    {
        string directory = Path.GetDirectoryName(destinationPath)!;
        string temporaryPath = Path.Combine(
            directory,
            $".{Path.GetFileName(destinationPath)}.{Guid.NewGuid():N}.tmp");
        try
        {
            File.Copy(sourcePath, temporaryPath);
            File.Move(temporaryPath, destinationPath, true);
        }
        catch
        {
            deleteTemporaryFile(temporaryPath);
            throw;
        }
    }
}
