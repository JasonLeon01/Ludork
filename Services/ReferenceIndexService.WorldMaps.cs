using Ludork.Models;
using Ludork.Views.Utils.BlueprintGraph;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ReferenceIndexService
{
    public IReadOnlyList<ReferenceRecord> GetExternalMapReferences(
        IEnumerable<string> runtimePaths,
        IReadOnlyCollection<string>? ignoredMapKeys = null)
    {
        ensureBuilt();
        ensureAllWorldChildMapReferences();
        HashSet<string> ignoredSources = ignoredMapKeys is null
            ? new HashSet<string>(StringComparer.Ordinal)
            : ignoredMapKeys
                .Select(key => nodeId("map", normalizeMapRuntimePath(key, false)))
                .ToHashSet(StringComparer.Ordinal);
        return runtimePaths
            .Select(mapNodeIdFromRuntimePath)
            .Where(id => id is not null)
            .SelectMany(id => referencedByTarget.GetValueOrDefault(id!) ?? [])
            .Where(record => record.Kind != "worldPlacement"
                && !ignoredSources.Contains(record.Source))
            .Distinct()
            .OrderBy(record => GetNode(record.Source)?.Type, StringComparer.Ordinal)
            .ThenBy(record => GetNode(record.Source)?.Key, StringComparer.Ordinal)
            .ThenBy(record => record.Path, StringComparer.Ordinal)
            .ToArray();
    }

    public bool RewriteMapReferences(
        IReadOnlyDictionary<string, string> replacements,
        bool recordSnapshot = true)
    {
        Dictionary<string, string> normalized = replacements.ToDictionary(
            item => normalizeMapRuntimePath(item.Key, true),
            item => item.Value.Replace('\\', '/').Trim('/'),
            StringComparer.Ordinal);
        if (normalized.Count == 0)
            return false;
        ensureBuilt();
        ensureAllWorldChildMapReferences();
        IReadOnlyList<MapReferenceRewrite> mapRewrites = prepareMapReferenceRewrites(normalized);
        bool nonMapChanged = rewriteRecognizedMapReferences(normalized, false);
        if (!nonMapChanged && mapRewrites.Count == 0)
            return false;
        foreach (MapReferenceRewrite rewrite in mapRewrites.Where(item => !item.WasLoaded))
        {
            if (gameData.InstallWorldChildMapSnapshot(rewrite.MapKey, rewrite.Original) is null)
                return false;
        }
        if (recordSnapshot)
            gameData.RecordSnapshot();
        if (nonMapChanged)
            rewriteRecognizedMapReferences(normalized, true);
        foreach (MapReferenceRewrite rewrite in mapRewrites)
        {
            if (!gameData.LoadedMapData.TryGetValue(rewrite.MapKey, out JsonObject? map))
                return false;
            rewriteKnownMapNodeReferences(map, normalized, true);
            gameData.NotifyMapContentChanged(rewrite.MapKey);
        }
        gameData.refreshModifiedState();
        MarkDirty();
        return true;
    }

    private IReadOnlyList<MapReferenceRewrite> prepareMapReferenceRewrites(
        IReadOnlyDictionary<string, string> replacements)
    {
        HashSet<string> targetIds = replacements.Keys
            .Select(mapNodeIdFromRuntimePath)
            .Where(item => item is not null)
            .Select(item => item!)
            .ToHashSet(StringComparer.Ordinal);
        HashSet<string> mapKeys = targetIds
            .SelectMany(targetId => referencedByTarget.GetValueOrDefault(targetId) ?? [])
            .Select(record => tryGetMapKeyFromNodeId(record.Source))
            .Where(item => item is not null)
            .Select(item => item!)
            .ToHashSet(StringComparer.Ordinal);
        List<MapReferenceRewrite> result = [];
        foreach (string mapKey in mapKeys.OrderBy(item => item, StringComparer.Ordinal))
        {
            bool wasLoaded = gameData.LoadedMapData.ContainsKey(mapKey);
            JsonObject? original = gameData.ReadMapSnapshotWithoutCaching(mapKey);
            if (original is null)
                throw new InvalidDataException($"The indexed map could not be read: {mapKey}.");
            JsonObject candidate = (JsonObject)original.DeepClone();
            if (rewriteKnownMapNodeReferences(candidate, replacements, false))
                result.Add(new MapReferenceRewrite(mapKey, original, wasLoaded));
        }
        return result;
    }

    private static string? tryGetMapKeyFromNodeId(string value)
    {
        const string prefix = "map:";
        return value.StartsWith(prefix, StringComparison.Ordinal)
            ? value[prefix.Length..]
            : null;
    }

    public IReadOnlyDictionary<string, string> CreateMapMoveReplacements(
        IReadOnlyList<(string OldPath, string NewPath)> moves)
    {
        Dictionary<string, string> result = new(StringComparer.Ordinal);
        string mapsRoot = gameData.MapPathPolicy.MapsRoot;
        foreach ((string oldPath, string newPath) in moves)
        {
            if (!tryGetMapsRelativePath(mapsRoot, oldPath, out string oldRelative)
                || !tryGetMapsRelativePath(mapsRoot, newPath, out string newRelative))
            {
                continue;
            }
            if (Directory.Exists(newPath))
            {
                if (oldRelative.Contains('/') || newRelative.Contains('/'))
                    continue;
                result[oldRelative + "/_world.json"] = newRelative + "/_world.json";
                foreach (string childPath in Directory.EnumerateFiles(newPath, "*.json", SearchOption.TopDirectoryOnly)
                             .Where(path => !string.Equals(
                                 Path.GetFileName(path),
                                 "_world.json",
                                 StringComparison.OrdinalIgnoreCase)))
                {
                    string fileName = Path.GetFileName(childPath);
                    result[oldRelative + "/" + fileName] = newRelative + "/" + fileName;
                }
                continue;
            }
            if (string.Equals(Path.GetExtension(oldRelative), ".json", StringComparison.OrdinalIgnoreCase)
                && string.Equals(Path.GetExtension(newRelative), ".json", StringComparison.OrdinalIgnoreCase)
                && !string.Equals(Path.GetFileName(oldRelative), "_world.json", StringComparison.OrdinalIgnoreCase)
                && !string.Equals(Path.GetFileName(newRelative), "_world.json", StringComparison.OrdinalIgnoreCase))
            {
                result[oldRelative] = newRelative;
            }
        }
        return result;
    }

    private bool rewriteRecognizedMapReferences(
        IReadOnlyDictionary<string, string> replacements,
        bool apply)
    {
        bool changed = false;
        foreach (JsonObject config in gameData.SystemConfigData.Values)
            changed |= rewriteConfigMapReferences(config, replacements, apply);
        foreach (JsonObject commonFunction in gameData.CommonFunctionsData.Values)
            changed |= rewriteKnownMapNodeReferences(commonFunction, replacements, apply);
        foreach (JsonObject blueprint in gameData.BlueprintsData.Values)
            changed |= rewriteKnownMapNodeReferences(blueprint, replacements, apply);
        foreach (JsonObject general in gameData.GeneralData.Values)
            changed |= rewriteKnownMapNodeReferences(general, replacements, apply);
        return changed;
    }

    private static bool rewriteConfigMapReferences(
        JsonObject config,
        IReadOnlyDictionary<string, string> replacements,
        bool apply)
    {
        bool changed = false;
        foreach (JsonObject setting in config.Select(item => item.Value).OfType<JsonObject>())
        {
            string? valueType = getString(setting["type"]);
            if (valueType is null
                || !valueType.StartsWith("file", StringComparison.Ordinal)
                || !string.Equals(getString(setting["root"]), "Data", StringComparison.OrdinalIgnoreCase)
                || !string.Equals(getString(setting["base"]), "Maps", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }
            if (setting["value"] is JsonArray values)
            {
                for (int index = 0; index < values.Count; index += 1)
                {
                    if (!tryGetMapReplacement(values[index], replacements, out string replacement))
                        continue;
                    changed = true;
                    if (apply)
                        values[index] = replacement;
                }
            }
            else if (tryGetMapReplacement(setting["value"], replacements, out string replacement))
            {
                changed = true;
                if (apply)
                    setting["value"] = replacement;
            }
        }
        return changed;
    }

    private static bool rewriteKnownMapNodeReferences(
        JsonNode node,
        IReadOnlyDictionary<string, string> replacements,
        bool apply)
    {
        bool changed = false;
        if (node is JsonObject objectValue)
        {
            string? nodeFunction = getString(objectValue["nodeFunction"]);
            if (nodeFunction is not null
                && isKnownMapNodeReference(nodeFunction)
                && objectValue["params"] is JsonArray { Count: > 0 } parameters
                && tryGetMapReplacement(parameters[0], replacements, out string replacement))
            {
                changed = true;
                if (apply)
                    parameters[0] = replacement;
            }
            foreach (JsonNode? child in objectValue.Select(item => item.Value).ToArray())
            {
                if (child is not null)
                    changed |= rewriteKnownMapNodeReferences(child, replacements, apply);
            }
        }
        else if (node is JsonArray arrayValue)
        {
            foreach (JsonNode? child in arrayValue.ToArray())
            {
                if (child is not null)
                    changed |= rewriteKnownMapNodeReferences(child, replacements, apply);
            }
        }
        return changed;
    }

    private static bool tryGetMapReplacement(
        JsonNode? value,
        IReadOnlyDictionary<string, string> replacements,
        out string replacement)
    {
        replacement = string.Empty;
        string? text = normalizeReferenceParam(value);
        if (text is null
            || !replacements.TryGetValue(normalizeMapRuntimePath(text, true), out string? result))
        {
            return false;
        }
        replacement = result;
        return true;
    }

    private static string? mapNodeIdFromRuntimePath(string runtimePath)
    {
        string key = normalizeMapRuntimePath(runtimePath, false);
        if (key.Length == 0)
            return null;
        if (key.EndsWith("/_world", StringComparison.OrdinalIgnoreCase))
            return nodeId("worldMap", key[..^"/_world".Length]);
        return nodeId("map", key);
    }

    private static string normalizeMapRuntimePath(string value, bool keepExtension)
    {
        string path = value.Replace('\\', '/').Trim().Trim('/');
        while (path.StartsWith("./", StringComparison.Ordinal))
            path = path[2..];
        const string marker = "Data/Maps/";
        int markerIndex = path.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
        if (markerIndex >= 0)
            path = path[(markerIndex + marker.Length)..];
        if (!keepExtension
            && path.EndsWith(DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase))
        {
            path = path[..^DataConfig.DataFileExtension.Length];
        }
        return path;
    }

    private static bool tryGetMapsRelativePath(
        string mapsRoot,
        string path,
        out string relativePath)
    {
        string relative = Path.GetRelativePath(mapsRoot, Path.GetFullPath(path));
        if (Path.IsPathRooted(relative)
            || relative == ".."
            || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal))
        {
            relativePath = string.Empty;
            return false;
        }
        relativePath = relative.Replace('\\', '/').Trim('/');
        return relativePath.Length != 0;
    }

    private void ensureAllWorldChildMapReferences()
    {
        if (allWorldChildMapReferencesBuilt)
            return;
        foreach (MapCatalogEntry entry in gameData.MapCatalog
                     .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap))
        {
            if (mapReferenceCache.TryGetValue(entry.Key, out IReadOnlyList<ReferenceRecord>? cached))
                replayMapReferences(cached);
            else
                scanAndCacheMapReferences(entry);
        }
        allWorldChildMapReferencesBuilt = true;
    }

    private void scanAndCacheMapReferences(MapCatalogEntry entry)
    {
        string sourceId = nodeId("map", entry.Key);
        JsonObject? map = gameData.ReadMapSnapshotWithoutCaching(entry.Key);
        if (map is null)
        {
            if (entry.Kind == MapCatalogEntryKind.WorldChildMap)
                throw new InvalidDataException($"The indexed world child map could not be read: {entry.Key}.");
            return;
        }
        scanMapReferences(sourceId, entry.Key, map);
        if (entry.Kind == MapCatalogEntryKind.WorldChildMap)
        {
            mapReferenceCache[entry.Key] = referencesBySource.TryGetValue(
                    sourceId,
                    out List<ReferenceRecord>? records)
                ? records.ToArray()
                : [];
        }
    }

    private void replayMapReferences(IEnumerable<ReferenceRecord> records)
    {
        foreach (ReferenceRecord record in records)
            addReference(record.Source, record.Target, record.Kind, record.Path);
    }

    private void scanMapReferences(string sourceId, string key, JsonObject data)
    {
        if (data["layers"] is JsonObject layers)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in layers)
            {
                if (pair.Value is not JsonObject layer)
                    continue;
                string? tilesetKey = getString(layer["layerTileset"]);
                if (!string.IsNullOrWhiteSpace(tilesetKey))
                {
                    addReference(
                        sourceId,
                        nodeId("tileset", tilesetKey),
                        "tileset",
                        $"Maps/{key}.layers.{pair.Key}.layerTileset");
                }
                addAssetReference(
                    sourceId,
                    layer["shaderPath"],
                    "Shaders",
                    "asset",
                    $"Maps/{key}.layers.{pair.Key}.shaderPath");
                scanAutoTileReferences(
                    sourceId,
                    layer["autoTiles"],
                    $"Maps/{key}.layers.{pair.Key}.autoTiles");
                scanMapActorReferences(
                    sourceId,
                    layer["actors"],
                    $"Maps/{key}.layers.{pair.Key}.actors");
                scanKnownMapNodeReferences(
                    sourceId,
                    layer["actors"],
                    $"Maps/{key}.layers.{pair.Key}.actors");
            }
        }
        if (data["actors"] is JsonObject actorsByLayer)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in actorsByLayer)
            {
                scanMapActorReferences(sourceId, pair.Value, $"Maps/{key}.actors.{pair.Key}");
                scanKnownMapNodeReferences(sourceId, pair.Value, $"Maps/{key}.actors.{pair.Key}");
            }
        }
        else
        {
            scanMapActorReferences(sourceId, data["actors"], $"Maps/{key}.actors");
            scanKnownMapNodeReferences(sourceId, data["actors"], $"Maps/{key}.actors");
        }
        addAssetReference(sourceId, data["bgm"], "Musics", "asset", $"Maps/{key}.bgm");
        addAssetReference(sourceId, data["bgs"], "Musics", "asset", $"Maps/{key}.bgs");
        addAssetReference(sourceId, data["fog"], "Fogs", "asset", $"Maps/{key}.fog");
        scanGenericReferences(sourceId, data["BPClassVarChanged"], $"Maps/{key}.BPClassVarChanged");
        scanKnownMapNodeReferences(
            sourceId,
            data["BPClassVarChanged"],
            $"Maps/{key}.BPClassVarChanged");
    }

    private void scanKnownMapNodeReferences(string sourceId, JsonNode? node, string path)
    {
        if (node is JsonObject objectValue)
        {
            string? nodeFunction = getString(objectValue["nodeFunction"]);
            if (nodeFunction is not null
                && isKnownMapNodeReference(nodeFunction)
                && objectValue["params"] is JsonArray { Count: > 0 } parameters)
            {
                addMapReference(sourceId, parameters[0], "nodeParam", $"{path}.params[0]");
            }
            foreach (KeyValuePair<string, JsonNode?> pair in objectValue)
            {
                if (pair.Value is not null)
                    scanKnownMapNodeReferences(sourceId, pair.Value, $"{path}.{pair.Key}");
            }
            return;
        }
        if (node is not JsonArray arrayValue)
            return;
        for (int index = 0; index < arrayValue.Count; index += 1)
        {
            if (arrayValue[index] is JsonNode child)
                scanKnownMapNodeReferences(sourceId, child, $"{path}[{index}]");
        }
    }

    private void scanWorldMapReferences(string sourceId, string key, JsonObject data)
    {
        addAssetReference(sourceId, data["fog"], "Fogs", "asset", $"Maps/{key}/_world.fog");
        if (data["placements"] is not JsonArray placements)
            return;
        for (int index = 0; index < placements.Count; index += 1)
        {
            if (placements[index] is not JsonObject placement
                || getString(placement["map"]) is not string fileName)
            {
                continue;
            }
            string childName = Path.ChangeExtension(fileName.Replace('\\', '/'), null) ?? string.Empty;
            if (childName.Length == 0 || childName.Contains('/'))
                continue;
            addReference(
                sourceId,
                nodeId("map", $"{key}/{childName}"),
                "worldPlacement",
                $"Maps/{key}/_world.placements[{index}].map");
        }
    }

    private static bool isKnownMapNodeReference(string nodeFunction)
    {
        return nodeFunction.EndsWith(".GotoMap", StringComparison.Ordinal)
            || nodeFunction.EndsWith(".RecordTelepoint", StringComparison.Ordinal);
    }

    private void addMapReference(string sourceId, JsonNode? value, string kind, string path)
    {
        string? key = normalizeReferenceParam(value);
        if (string.IsNullOrWhiteSpace(key))
            return;
        key = Path.ChangeExtension(key.Replace('\\', '/'), null) ?? key;
        if (key.EndsWith("/_world", StringComparison.OrdinalIgnoreCase))
        {
            addReference(sourceId, nodeId("worldMap", key[..^"/_world".Length]), kind, path);
            return;
        }
        addReference(sourceId, nodeId("map", key), kind, path);
    }
}

