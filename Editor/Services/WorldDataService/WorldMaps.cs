using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class WorldDataService
{
    internal JsonObject? getWorldMap(string worldKey)
    {
        worldKey = normalizeWorldKey(worldKey);
        return worldDocuments.TryGetValue(worldKey, out JsonObject? value) ? value : null;
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
            WorldName = world["worldName"]!.GetValue<string>(),
            Width = world["width"]?.GetValue<int?>() ?? 13,
            Height = world["height"]?.GetValue<int?>() ?? 13,
            Fog = world["fog"]?.GetValue<string>() ?? string.Empty,
            FogPower = world["fogPower"]?.GetValue<int?>() ?? 0,
            FogOx = world["fogOx"]?.GetValue<double?>() ?? 0.0,
            FogOy = world["fogOy"]?.GetValue<double?>() ?? 0.0,
            FogDistort = world["fogDistort"]?.GetValue<int?>() ?? 0,
            Panorama = world["panorama"]?.GetValue<string>() ?? string.Empty,
            LayerOrder = validation.LayerOrder,
            Placements = validation.Placements,
        };
    }

    public IReadOnlyList<string> getWorldChildren(string worldKey)
    {
        worldKey = normalizeWorldKey(worldKey);
        return store.Maps.getMapCatalogEntries()
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
            if (store.Maps.getMap(key) is JsonObject map)
                result[key] = (JsonObject)map.DeepClone();
        }
        return result;
    }

    public bool TryGetWorldForMap(string mapKey, out string worldKey)
    {
        mapKey = MapDataService.normaliseMapKey(mapKey);
        if (store.Maps.tryGetMapCatalogEntry(mapKey, out MapCatalogEntry entry)
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
            : MapDataService.normaliseMapKey(ignoredMapKey);
        foreach (string childKey in getWorldChildren(worldKey))
        {
            bool liveMap = string.Equals(childKey, normalizedIgnoredMap, StringComparison.Ordinal);
            if (!liveMap)
            {
                if (store.Maps.tryGetMapCatalogEntry(childKey, out MapCatalogEntry child)
                    && child.ActorTags.Contains(tag, StringComparer.Ordinal))
                {
                    return true;
                }
                continue;
            }
            Dictionary<string, List<MapDataService.MapActorTagLocation>> index = store.Maps.getMapActorTagIndex(childKey, true);
            if (index.TryGetValue(tag, out List<MapDataService.MapActorTagLocation>? locations)
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
        string mapKey = MapDataService.normaliseMapKey(normalized);
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
        foreach (string worldKey in worldDocuments.Keys.OrderBy(value => value, StringComparer.Ordinal))
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
            || !MapDataService.isValidMapSize(info.Width, info.Height)
            || !store.canCreateDocument("WorldMaps", worldKey)
            || store.Maps.containsMapKey(worldKey))
        {
            return false;
        }
        string worldDirectory = Path.Combine(store.ProjectPath, "Data", "Maps", worldKey);
        if (Directory.Exists(worldDirectory) || File.Exists(worldDirectory + ".json"))
            return false;
        JsonObject world = createWorldMapData(info, [], []);
        WorldMapValidationResult validation = worldMapValidation.Validate(
            worldKey,
            world,
            new Dictionary<string, MapCatalogEntry>(StringComparer.Ordinal));
        if (!validation.IsValid)
            return false;
        worldDocuments.RecordChange(worldKey);
        worldDocuments[worldKey] = world;
        store.Maps.setMapCatalogEntry(new MapCatalogEntry(
            worldKey,
            info.WorldName.Trim(),
            MapCatalogEntryKind.WorldMap,
            null,
            info.Width,
            info.Height,
            [],
            []));
        store.refreshModifiedState();
        return true;
    }

    public WorldMapMutationResult UpdateWorldMap(string worldKey, WorldMapInfo info)
    {
        worldKey = normalizeWorldKey(worldKey);
        if (info is null || getWorldMap(worldKey) is not JsonObject current)
            return WorldMapMutationResult.Failed("The world map does not exist.");
        if (!MapDataService.isValidMapSize(info.Width, info.Height))
            return WorldMapMutationResult.Failed("The world size must be from 1 to 32768 cells.");
        WorldMapValidationResult currentValidation = ValidateWorldMap(worldKey);
        if (!currentValidation.IsValid)
            return WorldMapMutationResult.Failed(formatWorldMapValidation(currentValidation));
        WorldMapInfo baseline = getWorldMapInfo(worldKey)!;
        JsonObject candidate = (JsonObject)current.DeepClone();
        if (info.WorldName.Trim() != baseline.WorldName.Trim())
            candidate["worldName"] = info.WorldName.Trim();
        if (info.Width != baseline.Width)
            candidate["width"] = info.Width;
        if (info.Height != baseline.Height)
            candidate["height"] = info.Height;
        bool fogChanged = info.Fog.Trim() != baseline.Fog.Trim();
        bool clearFog = fogChanged && string.IsNullOrWhiteSpace(info.Fog);
        if (fogChanged)
            candidate["fog"] = info.Fog.Trim();
        if (clearFog || info.FogPower != baseline.FogPower)
            candidate["fogPower"] = clearFog ? 0 : info.FogPower;
        if (clearFog || info.FogOx != baseline.FogOx)
            candidate["fogOx"] = clearFog ? 0.0 : info.FogOx;
        if (clearFog || info.FogOy != baseline.FogOy)
            candidate["fogOy"] = clearFog ? 0.0 : info.FogOy;
        if (clearFog || info.FogDistort != baseline.FogDistort)
            candidate["fogDistort"] = clearFog ? 0 : info.FogDistort;
        if (info.Panorama.Trim() != baseline.Panorama.Trim())
            candidate["panorama"] = info.Panorama.Trim();
        WorldMapValidationResult validation = worldMapValidation.Validate(
            worldKey,
            candidate,
            getWorldChildCatalog(worldKey));
        if (!validation.IsValid)
            return WorldMapMutationResult.Failed(formatWorldMapValidation(validation));
        if (ProjectDataStore.nodesEqual(current, candidate))
            return WorldMapMutationResult.Succeeded;
        store.RecordWorldSnapshot(worldKey);
        worldDocuments[worldKey] = candidate;
        store.Maps.setMapCatalogEntry(new MapCatalogEntry(
            worldKey,
            info.WorldName.Trim(),
            MapCatalogEntryKind.WorldMap,
            null,
            info.Width,
            info.Height,
            validation.LayerOrder,
            []));
        store.refreshModifiedState();
        return WorldMapMutationResult.Succeeded;
    }

    public bool RenameWorldMap(string currentKey, string newKey)
    {
        return isValidMapChildName(newKey) && store.RenameDocumentResource("WorldMaps", normalizeWorldKey(currentKey), newKey);
    }

    public bool DeleteWorldMap(string worldKey)
    {
        return store.DeleteDocumentResource("WorldMaps", normalizeWorldKey(worldKey));
    }

    public bool CreateWorldChildMap(string worldKey, MapInfo info)
    {
        worldKey = normalizeWorldKey(worldKey);
        if (info is null || getWorldMap(worldKey) is null)
            return false;
        string childName = MapDataService.normaliseMapKey(info.FileName);
        if (!isValidMapChildName(childName)
            || !store.canCreateDocument("Maps", worldKey + "/" + childName)
            || !MapDataService.isValidMapSize(info.Width, info.Height)
            || store.Assets.TilesetData.Keys.FirstOrDefault() is not { } tilesetKey)
        {
            return false;
        }
        string key = worldKey + "/" + childName;
        JsonObject map = MapDataService.createMapData(info, tilesetKey);
        store.Maps.CreateChildMapDocument(key, worldKey, map);
        return true;
    }

    public string? CopyWorldChildMap(string childMapKey)
    {
        childMapKey = MapDataService.normaliseMapKey(childMapKey);
        if (!TryGetWorldForMap(childMapKey, out string worldKey)
            || store.Maps.getMap(childMapKey) is not JsonObject source)
        {
            return null;
        }
        string oldStem = Path.GetFileName(childMapKey);
        string newStem = getCopyMapStem(worldKey, oldStem);
        string newKey = worldKey + "/" + newStem;
        if (!store.canCreateDocument("Maps", newKey))
            return null;
        JsonObject copy = (JsonObject)source.DeepClone();
        copy["mapName"] = (copy["mapName"]?.GetValue<string>() ?? oldStem) + " (copy)";
        rewriteCopiedActorTags(copy, worldKey, oldStem, newStem);
        if (hasDuplicateWorldActorTags(worldKey, newKey, copy))
            return null;
        store.Maps.CreateChildMapDocument(newKey, worldKey, copy);
        return newKey;
    }

    public WorldMapMutationResult UpdateWorldPlacement(
        string worldKey,
        string childMapKey,
        int x,
        int y)
    {
        worldKey = normalizeWorldKey(worldKey);
        childMapKey = MapDataService.normaliseMapKey(childMapKey);
        if (getWorldMap(worldKey) is not JsonObject current
            || !TryGetWorldForMap(childMapKey, out string childWorld)
            || !string.Equals(childWorld, worldKey, StringComparison.Ordinal)
            || !store.Maps.tryGetMapCatalogEntry(childMapKey, out MapCatalogEntry child))
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
        if (ProjectDataStore.nodesEqual(current, candidate))
            return WorldMapMutationResult.Succeeded;
        store.RecordWorldSnapshot(worldKey);
        worldDocuments[worldKey] = candidate;
        store.Maps.setWorldCatalogLayerOrder(worldKey, validation.LayerOrder);
        store.refreshModifiedState();
        return WorldMapMutationResult.Succeeded;
    }

    public WorldMapMutationResult RemoveWorldPlacement(string worldKey, string childMapKey)
    {
        worldKey = normalizeWorldKey(worldKey);
        childMapKey = MapDataService.normaliseMapKey(childMapKey);
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
        store.RecordWorldSnapshot(worldKey);
        worldDocuments[worldKey] = replaceWorldComposition(current, layerOrder, placements);
        store.Maps.setWorldCatalogLayerOrder(worldKey, layerOrder);
        store.refreshModifiedState();
        return WorldMapMutationResult.Succeeded;
    }

}
