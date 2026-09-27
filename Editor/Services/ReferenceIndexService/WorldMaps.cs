using Ludork.Models;
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
        using IDisposable metadataRead = metadataService.BeginRead();
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
            .OrderBy(record => nodes.GetValueOrDefault(record.Source)?.Type, StringComparer.Ordinal)
            .ThenBy(record => nodes.GetValueOrDefault(record.Source)?.Key, StringComparer.Ordinal)
            .ThenBy(record => record.Path, StringComparer.Ordinal)
            .ToArray();
    }

    public bool RewriteMapReferences(IReadOnlyDictionary<string, string> replacements)
    {
        return gameData.ApplyReferenceRewrites(PrepareMapReferenceRewrites(replacements));
    }

    public IReadOnlyList<ReferenceRewrite> PrepareMapReferenceRewrites(IReadOnlyDictionary<string, string> replacements)
    {
        Dictionary<string, string> normalized = replacements.ToDictionary(
            item => normalizeMapRuntimePath(item.Key, true),
            item => item.Value.Replace('\\', '/').Trim('/'),
            StringComparer.Ordinal);
        if (normalized.Count == 0)
            return [];
        ensureBuilt();
        ensureAllWorldChildMapReferences();
        List<ReferenceRewrite> result = prepareMapReferenceRewrites(normalized);
        appendReferenceRewrites(result, "Configs", gameData.GetReferenceSection("Configs"), normalized, true);
        appendReferenceRewrites(result, "CommonFunctions", gameData.GetReferenceSection("CommonFunctions"), normalized, false);
        appendReferenceRewrites(result, "Blueprints", gameData.GetReferenceSection("Blueprints"), normalized, false);
        appendReferenceRewrites(result, "General", gameData.GetReferenceSection("General"), normalized, false);
        return result;
    }

    private List<ReferenceRewrite> prepareMapReferenceRewrites(
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
        mapKeys.UnionWith(targetIds
            .Select(tryGetMapKeyFromNodeId)
            .Where(key => key is not null && gameData.Maps.MapData.ContainsKey(key))
            .Select(key => key!));
        List<ReferenceRewrite> result = [];
        foreach (string mapKey in mapKeys.OrderBy(item => item, StringComparer.Ordinal))
        {
            JsonObject? original = gameData.Maps.ReadMapSnapshotWithoutCaching(mapKey);
            if (original is null)
                throw new InvalidDataException($"The indexed map could not be read: {mapKey}.");
            JsonObject candidate = (JsonObject)original.DeepClone();
            if (rewriteKnownMapNodeReferences(candidate, replacements))
                result.Add(new ReferenceRewrite("Maps", mapKey, original, candidate));
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
        string mapsRoot = gameData.Worlds.MapPathPolicy.MapsRoot;
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

    private static void appendReferenceRewrites(
        List<ReferenceRewrite> rewrites,
        string section,
        IReadOnlyDictionary<string, JsonObject> data,
        IReadOnlyDictionary<string, string> replacements,
        bool config)
    {
        foreach (KeyValuePair<string, JsonObject> entry in data)
        {
            JsonObject candidate = (JsonObject)entry.Value.DeepClone();
            bool changed = config
                ? rewriteConfigMapReferences(candidate, replacements)
                : rewriteKnownMapNodeReferences(candidate, replacements);
            if (changed)
                rewrites.Add(new ReferenceRewrite(section, entry.Key, entry.Value, candidate));
        }
    }

    private static bool rewriteConfigMapReferences(
        JsonObject config,
        IReadOnlyDictionary<string, string> replacements)
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
                    values[index] = replacement;
                }
            }
            else if (tryGetMapReplacement(setting["value"], replacements, out string replacement))
            {
                changed = true;
                setting["value"] = replacement;
            }
        }
        return changed;
    }

    private static bool rewriteKnownMapNodeReferences(
        JsonNode node,
        IReadOnlyDictionary<string, string> replacements)
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
                parameters[0] = replacement;
            }
            foreach (JsonNode? child in objectValue.Select(item => item.Value).ToArray())
            {
                if (child is not null)
                    changed |= rewriteKnownMapNodeReferences(child, replacements);
            }
        }
        else if (node is JsonArray arrayValue)
        {
            foreach (JsonNode? child in arrayValue.ToArray())
            {
                if (child is not null)
                    changed |= rewriteKnownMapNodeReferences(child, replacements);
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
        foreach (MapCatalogEntry entry in gameData.Maps.MapCatalog
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
        cancellationToken.ThrowIfCancellationRequested();
        string sourceId = nodeId("map", entry.Key);
        JsonObject? map = gameData.Maps.ReadMapSnapshotWithoutCaching(entry.Key);
        if (map is null)
        {
            if (entry.Kind == MapCatalogEntryKind.WorldChildMap)
                throw new InvalidDataException($"The indexed world child map could not be read: {entry.Key}.");
            return;
        }
        scanDocumentReferences("Maps", entry.Key, map);
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

    private static bool isKnownMapNodeReference(string nodeFunction)
    {
        return nodeFunction.EndsWith(".GotoMap", StringComparison.Ordinal)
            || nodeFunction.EndsWith(".RecordTelepoint", StringComparison.Ordinal);
    }
}
