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
    public Task<JsonObject?> ReadWorldChildMapSnapshotAsync(string key)
    {
        key = normaliseMapKey(key);
        if (!tryGetMapCatalogEntry(key, out MapCatalogEntry entry)
            || entry.Kind != MapCatalogEntryKind.WorldChildMap)
        {
            return Task.FromResult<JsonObject?>(null);
        }
        string path = getReadableMapDataPath(key);
        return Task.Run(() => readMapFile(path, true));
    }

    public JsonObject? InstallWorldChildMapSnapshot(string key, JsonObject snapshot)
    {
        key = normaliseMapKey(key);
        if (sections["Maps"].Data.TryGetValue(key, out JsonObject? loaded))
        {
            touchMap(key);
            return loaded;
        }
        if (!tryGetMapCatalogEntry(key, out MapCatalogEntry entry)
            || entry.Kind != MapCatalogEntryKind.WorldChildMap
            || !mapMatchesCatalogEntry(snapshot, entry))
        {
            return null;
        }
        string path = getReadableMapDataPath(key);
        if (!File.Exists(path))
            return null;
        sections["Maps"].Data[key] = snapshot;
        originData["Maps"][key] = (JsonObject)snapshot.DeepClone();
        mapLoadedBytes[key] = new FileInfo(path).Length;
        touchMap(key);
        return snapshot;
    }

    public int TrimWorldChildCache(
        IReadOnlyCollection<string>? pinnedMapKeys = null,
        int maximumLoadedChildren = 32,
        long maximumLoadedBytes = 256L * 1024L * 1024L)
    {
        HashSet<string> pinned = pinnedMapKeys is null
            ? new HashSet<string>(StringComparer.Ordinal)
            : new HashSet<string>(pinnedMapKeys.Select(normaliseMapKey), StringComparer.Ordinal);
        List<string> candidates = sections["Maps"].Data.Keys
            .Where(key => TryGetWorldForMap(key, out _)
                && !pinned.Contains(key)
                && !isMapHeldByHistory(key)
                && isCleanLoadedMap(key))
            .OrderBy(key => mapAccessOrder.GetValueOrDefault(key))
            .ToList();
        int loadedCount = sections["Maps"].Data.Keys.Count(key => TryGetWorldForMap(key, out _));
        long loadedBytes = sections["Maps"].Data.Keys
            .Where(key => TryGetWorldForMap(key, out _))
            .Sum(key => mapLoadedBytes.GetValueOrDefault(key));
        int removed = 0;
        foreach (string key in candidates)
        {
            if (loadedCount <= Math.Max(0, maximumLoadedChildren)
                && loadedBytes <= Math.Max(0L, maximumLoadedBytes))
            {
                break;
            }
            loadedCount -= 1;
            loadedBytes -= mapLoadedBytes.GetValueOrDefault(key);
            sections["Maps"].Data.Remove(key);
            originData["Maps"].Remove(key);
            mapAccessOrder.Remove(key);
            mapLoadedBytes.Remove(key);
            removed += 1;
        }
        return removed;
    }

    public JsonObject? getWorldMap(string worldKey)
    {
        worldKey = normalizeWorldKey(worldKey);
        return sections["WorldMaps"].Data.TryGetValue(worldKey, out JsonObject? value) ? value : null;
    }

    public WorldMapInfo? getWorldMapInfo(string worldKey)
    {
        worldKey = normalizeWorldKey(worldKey);
        if (getWorldMap(worldKey) is not JsonObject world)
            return null;
        WorldMapValidationResult validation = ValidateWorldMap(worldKey);
        return new WorldMapInfo
        {
            DirectoryName = worldKey,
            Width = world["width"]?.GetValue<int?>() ?? 13,
            Height = world["height"]?.GetValue<int?>() ?? 13,
            Fog = world["fog"]?.GetValue<string>() ?? string.Empty,
            FogPower = world["fogPower"]?.GetValue<int?>() ?? 0,
            FogOx = world["fogOx"]?.GetValue<double?>() ?? 0.0,
            FogOy = world["fogOy"]?.GetValue<double?>() ?? 0.0,
            FogDistort = world["fogDistort"]?.GetValue<int?>() ?? 0,
            LayerOrder = validation.LayerOrder,
            Placements = validation.Placements,
        };
    }

    public IReadOnlyList<string> getWorldChildren(string worldKey)
    {
        worldKey = normalizeWorldKey(worldKey);
        return getMapCatalogEntries()
            .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap
                && string.Equals(entry.WorldKey, worldKey, StringComparison.Ordinal))
            .Select(entry => entry.Key)
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public IReadOnlyDictionary<string, JsonObject> GetWorldChildMaps(string worldKey)
    {
        Dictionary<string, JsonObject> result = new(StringComparer.Ordinal);
        foreach (string key in getWorldChildren(worldKey))
        {
            if (getMap(key) is JsonObject map)
                result[key] = map;
        }
        return result;
    }

    public bool TryGetWorldForMap(string mapKey, out string worldKey)
    {
        mapKey = normaliseMapKey(mapKey);
        if (tryGetMapCatalogEntry(mapKey, out MapCatalogEntry entry)
            && entry.Kind == MapCatalogEntryKind.WorldChildMap
            && entry.WorldKey is not null)
        {
            worldKey = entry.WorldKey;
            return true;
        }
        worldKey = string.Empty;
        return false;
    }

    public bool WorldActorTagExists(
        string worldKey,
        string tag,
        string? ignoredMapKey = null,
        string? ignoredLayerName = null,
        int ignoredActorIndex = -1)
    {
        worldKey = normalizeWorldKey(worldKey);
        if (string.IsNullOrWhiteSpace(tag))
            return false;
        string normalizedIgnoredMap = ignoredMapKey is null
            ? string.Empty
            : normaliseMapKey(ignoredMapKey);
        foreach (string childKey in getWorldChildren(worldKey))
        {
            bool liveMap = string.Equals(childKey, normalizedIgnoredMap, StringComparison.Ordinal);
            if (!liveMap)
            {
                if (tryGetMapCatalogEntry(childKey, out MapCatalogEntry child)
                    && child.ActorTags.Contains(tag, StringComparer.Ordinal))
                {
                    return true;
                }
                continue;
            }
            Dictionary<string, List<MapActorTagLocation>> index = getMapActorTagIndex(childKey, true);
            if (index.TryGetValue(tag, out List<MapActorTagLocation>? locations)
                && locations.Any(location =>
                    !string.Equals(location.LayerName, ignoredLayerName, StringComparison.Ordinal)
                    || location.ActorIndex != ignoredActorIndex))
            {
                return true;
            }
        }
        return false;
    }

    public string GetWorldManifestRuntimePath(string worldKey)
    {
        worldKey = normalizeWorldKey(worldKey);
        return worldKey.Length == 0 ? string.Empty : worldKey + "/_world.json";
    }

    public bool TryResolveWorldTarget(string mapPath, out WorldMapTarget target)
    {
        string normalized = normalizeMapRuntimePath(mapPath);
        if (normalized.EndsWith("/_world.json", StringComparison.OrdinalIgnoreCase))
        {
            string worldKey = normalized[..^"/_world.json".Length];
            if (getWorldMap(worldKey) is not null)
            {
                target = new WorldMapTarget(worldKey, GetWorldManifestRuntimePath(worldKey), null, 0, 0);
                return true;
            }
        }
        string mapKey = normaliseMapKey(normalized);
        if (TryGetWorldForMap(mapKey, out string childWorld)
            && tryGetWorldPlacement(childWorld, mapKey, out WorldMapPlacement placement))
        {
            target = new WorldMapTarget(
                childWorld,
                GetWorldManifestRuntimePath(childWorld),
                mapKey,
                placement.Rect.X,
                placement.Rect.Y);
            return true;
        }
        target = null!;
        return false;
    }

    public WorldMapValidationResult ValidateWorldMap(string worldKey)
    {
        worldKey = normalizeWorldKey(worldKey);
        if (getWorldMap(worldKey) is not JsonObject world)
        {
            return new WorldMapValidationResult(
                [new WorldMapValidationIssue("missingWorld", "The world map does not exist.")],
                [],
                []);
        }
        return worldMapValidation.Validate(worldKey, world, getWorldChildCatalog(worldKey));
    }

    public WorldMapMutationResult ValidateAllWorldMaps()
    {
        List<string> failures = [];
        foreach (string worldKey in sections["WorldMaps"].Data.Keys.OrderBy(value => value, StringComparer.Ordinal))
        {
            WorldMapValidationResult validation = ValidateWorldMap(worldKey);
            if (!validation.IsValid)
                failures.Add(worldKey + ": " + formatWorldMapValidation(validation));
            Dictionary<string, int> tagCounts = new(StringComparer.Ordinal);
            foreach (MapCatalogEntry child in getWorldChildCatalog(worldKey).Values)
            {
                foreach (string tag in child.ActorTags)
                    tagCounts[tag] = tagCounts.GetValueOrDefault(tag) + 1;
            }
            foreach (KeyValuePair<string, int> pair in tagCounts)
            {
                if (pair.Value > 1)
                    failures.Add(worldKey + ": duplicate actor tag " + pair.Key + ".");
            }
        }
        return failures.Count == 0
            ? WorldMapMutationResult.Succeeded
            : WorldMapMutationResult.Failed(string.Join(Environment.NewLine, failures));
    }

    public bool CreateWorldMap(string worldKey, WorldMapInfo info)
    {
        worldKey = normalizeWorldKey(worldKey);
        if (info is null
            || !isValidWorldKey(worldKey)
            || !isValidMapSize(info.Width, info.Height)
            || sections["WorldMaps"].Data.ContainsKey(worldKey)
            || containsMapKey(worldKey))
        {
            return false;
        }
        string worldDirectory = Path.Combine(ProjectPath, "Data", "Maps", worldKey);
        if (Directory.Exists(worldDirectory) || File.Exists(worldDirectory + ".json"))
            return false;
        JsonObject world = createWorldMapData(info, [], []);
        WorldMapValidationResult validation = worldMapValidation.Validate(
            worldKey,
            world,
            new Dictionary<string, MapCatalogEntry>(StringComparer.Ordinal));
        if (!validation.IsValid)
            return false;
        RecordSnapshot();
        sections["WorldMaps"].Data[worldKey] = world;
        setMapCatalogEntry(new MapCatalogEntry(
            worldKey,
            worldKey,
            MapCatalogEntryKind.WorldMap,
            null,
            info.Width,
            info.Height,
            [],
            []));
        refreshModifiedState();
        return true;
    }

    public WorldMapMutationResult UpdateWorldMap(string worldKey, WorldMapInfo info)
    {
        worldKey = normalizeWorldKey(worldKey);
        if (info is null || getWorldMap(worldKey) is not JsonObject current)
            return WorldMapMutationResult.Failed("The world map does not exist.");
        if (!isValidMapSize(info.Width, info.Height))
            return WorldMapMutationResult.Failed("The world size must be from 1 to 32768 cells.");
        WorldMapValidationResult currentValidation = ValidateWorldMap(worldKey);
        if (!currentValidation.IsValid)
            return WorldMapMutationResult.Failed(formatWorldMapValidation(currentValidation));
        JsonObject candidate = createWorldMapData(
            info,
            currentValidation.LayerOrder,
            currentValidation.Placements);
        WorldMapValidationResult validation = worldMapValidation.Validate(
            worldKey,
            candidate,
            getWorldChildCatalog(worldKey));
        if (!validation.IsValid)
            return WorldMapMutationResult.Failed(formatWorldMapValidation(validation));
        if (nodesEqual(current, candidate))
            return WorldMapMutationResult.Succeeded;
        RecordWorldSnapshot(worldKey);
        sections["WorldMaps"].Data[worldKey] = candidate;
        setMapCatalogEntry(new MapCatalogEntry(
            worldKey,
            worldKey,
            MapCatalogEntryKind.WorldMap,
            null,
            info.Width,
            info.Height,
            validation.LayerOrder,
            []));
        refreshModifiedState();
        return WorldMapMutationResult.Succeeded;
    }

    public bool RenameWorldMap(string currentKey, string newKey)
    {
        currentKey = normalizeWorldKey(currentKey);
        newKey = normalizeWorldKey(newKey);
        if (!isValidWorldKey(newKey)
            || !sections["WorldMaps"].Data.TryGetValue(currentKey, out JsonObject? world)
            || sections["WorldMaps"].Data.ContainsKey(newKey)
            || containsMapKey(newKey))
        {
            return false;
        }
        string destinationDirectory = getWorldDirectory(newKey);
        bool returnsToSource = pendingWorldDirectoryMoves.TryGetValue(currentKey, out string? pendingSource)
            && pathsEqual(pendingSource, destinationDirectory);
        if ((!returnsToSource && Directory.Exists(destinationDirectory))
            || File.Exists(destinationDirectory + ".json"))
            return false;
        MapCatalogEntry[] childEntries = getWorldChildCatalog(currentKey).Values.ToArray();
        string[] children = childEntries.Select(entry => entry.Key).ToArray();
        RecordSnapshot();
        string? sourceDirectory = pendingWorldDirectoryMoves.Remove(currentKey, out string? source)
            ? source
            : originData["WorldMaps"].ContainsKey(currentKey) ? getWorldDirectory(currentKey) : null;
        if (sourceDirectory is not null && !pathsEqual(sourceDirectory, destinationDirectory))
            pendingWorldDirectoryMoves[newKey] = sourceDirectory;
        sections["WorldMaps"].Data.Remove(currentKey);
        sections["WorldMaps"].Data[newKey] = world;
        Dictionary<string, JsonObject> maps = sections["Maps"].Data;
        foreach (string child in children.Where(maps.ContainsKey))
        {
            JsonObject childMap = maps[child];
            maps.Remove(child);
            string childName = child[(currentKey.Length + 1)..];
            string renamedKey = newKey + "/" + childName;
            maps[renamedKey] = childMap;
            rekeyLoadedMapMetadata(child, renamedKey);
        }
        removeMapCatalogEntry(MapCatalogEntryKind.WorldMap, currentKey);
        foreach (string child in children)
            removeMapCatalogEntry(MapCatalogEntryKind.WorldChildMap, child);
        setMapCatalogEntry(new MapCatalogEntry(
            newKey,
            newKey,
            MapCatalogEntryKind.WorldMap,
            null,
            world["width"]?.GetValue<int?>() ?? 0,
            world["height"]?.GetValue<int?>() ?? 0,
            readStringArray(world["layerOrder"]),
            []));
        foreach (MapCatalogEntry child in childEntries)
        {
            string childName = child.Key[(currentKey.Length + 1)..];
            string renamedKey = newKey + "/" + childName;
            setMapCatalogEntry(child with { Key = renamedKey, WorldKey = newKey });
        }
        refreshModifiedState();
        return true;
    }

    public bool DeleteWorldMap(string worldKey)
    {
        worldKey = normalizeWorldKey(worldKey);
        if (!sections["WorldMaps"].Data.ContainsKey(worldKey))
            return false;
        string[] children = getWorldChildren(worldKey).ToArray();
        RecordSnapshot();
        pendingWorldDirectoryMoves.Remove(worldKey);
        sections["WorldMaps"].Data.Remove(worldKey);
        removeMapCatalogEntry(MapCatalogEntryKind.WorldMap, worldKey);
        foreach (string child in children)
        {
            sections["Maps"].Data.Remove(child);
            removeLoadedMapMetadata(child);
            removeMapCatalogEntry(MapCatalogEntryKind.WorldChildMap, child);
        }
        refreshModifiedState();
        return true;
    }

    public bool CreateWorldChildMap(string worldKey, MapInfo info)
    {
        worldKey = normalizeWorldKey(worldKey);
        if (info is null || getWorldMap(worldKey) is null)
            return false;
        string childName = normaliseMapKey(info.FileName);
        if (!isValidMapChildName(childName)
            || containsMapKey(worldKey + "/" + childName)
            || !isValidMapSize(info.Width, info.Height)
            || TilesetData.Keys.FirstOrDefault() is not { } tilesetKey)
        {
            return false;
        }
        if (File.Exists(getMapDataPath(worldKey + "/" + childName)))
            return false;
        RecordSnapshot();
        string key = worldKey + "/" + childName;
        JsonObject map = createMapData(info, tilesetKey);
        sections["Maps"].Data[key] = map;
        updateLoadedMapMetadata(key, map);
        setMapCatalogEntry(createMapCatalogEntry(
            key,
            MapCatalogEntryKind.WorldChildMap,
            worldKey,
            map));
        refreshModifiedState();
        return true;
    }

    public string? CopyWorldChildMap(string childMapKey)
    {
        childMapKey = normaliseMapKey(childMapKey);
        if (!TryGetWorldForMap(childMapKey, out string worldKey)
            || getMap(childMapKey) is not JsonObject source)
        {
            return null;
        }
        string oldStem = Path.GetFileName(childMapKey);
        string newStem = getCopyMapStem(worldKey, oldStem);
        string newKey = worldKey + "/" + newStem;
        JsonObject copy = (JsonObject)source.DeepClone();
        copy["mapName"] = (copy["mapName"]?.GetValue<string>() ?? oldStem) + " (copy)";
        rewriteCopiedActorTags(copy, worldKey, oldStem, newStem);
        if (hasDuplicateWorldActorTags(worldKey, newKey, copy))
            return null;
        RecordSnapshot();
        sections["Maps"].Data[newKey] = copy;
        updateLoadedMapMetadata(newKey, copy);
        setMapCatalogEntry(createMapCatalogEntry(
            newKey,
            MapCatalogEntryKind.WorldChildMap,
            worldKey,
            copy));
        refreshModifiedState();
        return newKey;
    }

    public WorldMapMutationResult UpdateWorldPlacement(
        string worldKey,
        string childMapKey,
        int x,
        int y)
    {
        worldKey = normalizeWorldKey(worldKey);
        childMapKey = normaliseMapKey(childMapKey);
        if (getWorldMap(worldKey) is not JsonObject current
            || !TryGetWorldForMap(childMapKey, out string childWorld)
            || !string.Equals(childWorld, worldKey, StringComparison.Ordinal)
            || !tryGetMapCatalogEntry(childMapKey, out MapCatalogEntry child))
        {
            return WorldMapMutationResult.Failed("The world or child map does not exist.");
        }
        WorldMapValidationResult currentValidation = ValidateWorldMap(worldKey);
        if (!currentValidation.IsValid)
            return WorldMapMutationResult.Failed(formatWorldMapValidation(currentValidation));
        string childFile = Path.GetFileName(childMapKey) + ".json";
        List<WorldMapPlacement> placements = currentValidation.Placements.ToList();
        WorldMapPlacement replacement = new(
            childFile,
            new WorldMapRect(x, y, child.Width, child.Height));
        int placementIndex = placements.FindIndex(item =>
            string.Equals(item.Map, childFile, StringComparison.Ordinal));
        if (placementIndex < 0)
            placements.Add(replacement);
        else
            placements[placementIndex] = replacement;
        IReadOnlyList<string>? layerOrder = worldMapValidation.TryMergeLayerOrder(
            worldKey,
            placements,
            getWorldChildCatalog(worldKey));
        if (layerOrder is null)
            return WorldMapMutationResult.Failed("Placed child maps contain conflicting layer orders.");
        JsonObject candidate = replaceWorldComposition(current, layerOrder, placements);
        WorldMapValidationResult validation = worldMapValidation.Validate(
            worldKey,
            candidate,
            getWorldChildCatalog(worldKey));
        if (!validation.IsValid)
            return WorldMapMutationResult.Failed(formatWorldMapValidation(validation));
        if (nodesEqual(current, candidate))
            return WorldMapMutationResult.Succeeded;
        RecordWorldSnapshot(worldKey);
        sections["WorldMaps"].Data[worldKey] = candidate;
        setWorldCatalogLayerOrder(worldKey, validation.LayerOrder);
        refreshModifiedState();
        return WorldMapMutationResult.Succeeded;
    }

    public WorldMapMutationResult RemoveWorldPlacement(string worldKey, string childMapKey)
    {
        worldKey = normalizeWorldKey(worldKey);
        childMapKey = normaliseMapKey(childMapKey);
        if (getWorldMap(worldKey) is not JsonObject current
            || !TryGetWorldForMap(childMapKey, out string childWorld)
            || !string.Equals(childWorld, worldKey, StringComparison.Ordinal))
        {
            return WorldMapMutationResult.Failed("The world or child map does not exist.");
        }
        WorldMapValidationResult validation = ValidateWorldMap(worldKey);
        if (!validation.IsValid)
            return WorldMapMutationResult.Failed(formatWorldMapValidation(validation));
        string childFile = Path.GetFileName(childMapKey) + ".json";
        List<WorldMapPlacement> placements = validation.Placements
            .Where(item => !string.Equals(item.Map, childFile, StringComparison.Ordinal))
            .ToList();
        if (placements.Count == validation.Placements.Count)
            return WorldMapMutationResult.Succeeded;
        IReadOnlyList<string>? layerOrder = worldMapValidation.TryMergeLayerOrder(
            worldKey,
            placements,
            getWorldChildCatalog(worldKey));
        if (layerOrder is null)
            return WorldMapMutationResult.Failed("Placed child maps contain conflicting layer orders.");
        RecordWorldSnapshot(worldKey);
        sections["WorldMaps"].Data[worldKey] = replaceWorldComposition(current, layerOrder, placements);
        setWorldCatalogLayerOrder(worldKey, layerOrder);
        refreshModifiedState();
        return WorldMapMutationResult.Succeeded;
    }
}
