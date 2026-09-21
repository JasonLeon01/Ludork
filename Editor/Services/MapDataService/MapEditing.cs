using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    internal JsonObject? getMap(string key)
    {
        key = normaliseMapKey(key);
        if (mapDocuments.TryGetValue(key, out JsonObject? value))
        {
            touchMap(key);
            return value;
        }
        if (!tryGetMapCatalogEntry(key, out MapCatalogEntry entry)
            || entry.Kind != MapCatalogEntryKind.WorldChildMap)
        {
            return null;
        }
        string path = getReadableMapDataPath(key);
        JsonObject? loaded = readMapFile(path, true);
        if (loaded is null || !mapMatchesCatalogEntry(loaded, entry))
        {
            store.addInvalidLoadPath(path);
            return null;
        }
        store.AcceptLoadedDocument("Maps", key, loaded);
        mapLoadedBytes[key] = new FileInfo(path).Length;
        touchMap(key);
        return loaded;
    }

    public MapInfo? getMapInfo(string key)
    {
        if (getMap(key) is not JsonObject map)
            return null;
        JsonArray ambientLight = map["ambientLight"] is JsonArray values && values.Count >= 4
            ? (JsonArray)values.DeepClone()
            : new JsonArray(255, 255, 255, 255);
        return new MapInfo
        {
            FileName = key + ".json",
            MapName = map["mapName"]?.GetValue<string>() ?? key,
            Width = Math.Max(1, map["width"]?.GetValue<int?>() ?? 13),
            Height = Math.Max(1, map["height"]?.GetValue<int?>() ?? 13),
            AmbientLight = ambientLight,
            Bgm = map["bgm"]?.GetValue<string>() ?? string.Empty,
            BgmFilter = ProjectDataStore.cloneObject(map["bgmFilter"]),
            Bgs = map["bgs"]?.GetValue<string>() ?? string.Empty,
            BgsFilter = ProjectDataStore.cloneObject(map["bgsFilter"]),
            Fog = map["fog"]?.GetValue<string>() ?? string.Empty,
            FogPower = map["fogPower"]?.GetValue<int?>() ?? 0,
            FogOx = map["fogOx"]?.GetValue<double?>() ?? 0.0,
            FogOy = map["fogOy"]?.GetValue<double?>() ?? 0.0,
            FogDistort = map["fogDistort"]?.GetValue<int?>() ?? 0,
            Panorama = map["panorama"]?.GetValue<string>() ?? string.Empty,
        };
    }

    public string getMapDisplayName(string key)
    {
        key = normaliseMapKey(key);
        if (tryGetMapCatalogEntry(key, out MapCatalogEntry entry))
            return entry.DisplayName;
        string? mapName = getMap(key)?["mapName"]?.GetValue<string>();
        return string.IsNullOrWhiteSpace(mapName) ? key : mapName;
    }

    public string getNewMapFileName()
    {
        for (int index = 1; ; index += 1)
        {
            string key = $"Map_{index:D2}";
            if (!MapData.ContainsKey(key))
                return key + ".json";
        }
    }

    public bool CreateMap(string key, string mapName, int width, int height)
    {
        return CreateMap(new MapInfo
        {
            FileName = key,
            MapName = mapName,
            Width = width,
            Height = height,
        });
    }

    public bool CreateMap(MapInfo info)
    {
        if (info is null)
            return false;
        string key = normaliseMapKey(info.FileName);
        if (!WorldDataService.isValidMapChildName(key) || !store.canCreateDocument("Maps", key) || store.Worlds.WorldMapData.ContainsKey(key)
            || !isValidMapSize(info.Width, info.Height)
            || store.Assets.TilesetData.Keys.FirstOrDefault() is not { } tilesetKey)
            return false;
        string mapPath = getMapDataPath(key);
        if (Directory.Exists(Path.ChangeExtension(mapPath, null)))
            return false;
        mapDocuments.RecordChange(key);
        JsonObject map = createMapData(info, tilesetKey);
        mapDocuments[key] = map;
        updateLoadedMapMetadata(key, map);
        setMapCatalogEntry(createMapCatalogEntry(
            key,
            MapCatalogEntryKind.StandaloneMap,
            null,
            map));
        store.refreshModifiedState();
        return true;
    }

    public bool UpdateMap(string currentKey, MapInfo info)
    {
        currentKey = normaliseMapKey(currentKey);
        if (info is null || getMap(currentKey) is not JsonObject current)
            return false;
        string requestedKey = normaliseMapKey(info.FileName);
        bool childMap = store.Worlds.TryGetWorldForMap(currentKey, out string worldKey);
        string newKey;
        if (childMap)
        {
            string childName = requestedKey.Contains('/') ? Path.GetFileName(requestedKey) : requestedKey;
            newKey = worldKey + "/" + childName;
            if (!WorldDataService.isValidMapChildName(childName)
                || requestedKey.Contains('/')
                    && !string.Equals(requestedKey, newKey, StringComparison.Ordinal))
            {
                return false;
            }
        }
        else
        {
            if (!WorldDataService.isValidMapChildName(requestedKey))
                return false;
            newKey = requestedKey;
        }
        if (!isValidMapSize(info.Width, info.Height)
            || newKey != currentKey
                && (containsMapKey(newKey)
                    || store.Worlds.WorldMapData.ContainsKey(newKey)
                    || File.Exists(getMapDataPath(newKey))))
            return false;
        MapInfo baseline = getMapInfo(currentKey)!;
        JsonObject candidate = (JsonObject)current.DeepClone();
        bool sizeChanged = baseline.Width != info.Width || baseline.Height != info.Height;
        if (sizeChanged)
            resizeMapLayers(candidate, info.Width, info.Height);
        if (!string.IsNullOrWhiteSpace(info.MapName) && info.MapName.Trim() != baseline.MapName.Trim())
            candidate["mapName"] = info.MapName.Trim();
        if (info.Width != baseline.Width)
            candidate["width"] = info.Width;
        if (info.Height != baseline.Height)
            candidate["height"] = info.Height;
        JsonArray ambientLight = normaliseAmbientLight(info.AmbientLight);
        if (!JsonNode.DeepEquals(ambientLight, normaliseAmbientLight(baseline.AmbientLight)))
            candidate["ambientLight"] = ambientLight;
        if (info.Bgm.Trim() != baseline.Bgm.Trim())
            candidate["bgm"] = info.Bgm.Trim();
        if (!JsonNode.DeepEquals(info.BgmFilter, baseline.BgmFilter))
            candidate["bgmFilter"] = ProjectDataStore.cloneObject(info.BgmFilter);
        if (info.Bgs.Trim() != baseline.Bgs.Trim())
            candidate["bgs"] = info.Bgs.Trim();
        if (!JsonNode.DeepEquals(info.BgsFilter, baseline.BgsFilter))
            candidate["bgsFilter"] = ProjectDataStore.cloneObject(info.BgsFilter);
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
        JsonObject? worldCandidate = null;
        if (childMap && (sizeChanged || currentKey != newKey) && store.Worlds.getWorldMap(worldKey) is JsonObject world)
        {
            WorldMapValidationResult worldValidation = store.Worlds.ValidateWorldMap(worldKey);
            if (!worldValidation.IsValid)
                return false;
            string oldFile = Path.GetFileName(currentKey) + ".json";
            string newFile = Path.GetFileName(newKey) + ".json";
            List<WorldMapPlacement> placements = worldValidation.Placements.Select(placement =>
            {
                if (!string.Equals(placement.Map, oldFile, StringComparison.Ordinal))
                    return placement;
                return new WorldMapPlacement(
                    newFile,
                    new WorldMapRect(
                        placement.Rect.X,
                        placement.Rect.Y,
                        info.Width,
                        info.Height));
            }).ToList();
            Dictionary<string, MapCatalogEntry> children = store.Worlds.getWorldChildCatalog(worldKey)
                .Where(item => !string.Equals(item.Key, currentKey, StringComparison.Ordinal))
                .ToDictionary(item => item.Key, item => item.Value, StringComparer.Ordinal);
            children[newKey] = createMapCatalogEntry(
                newKey,
                MapCatalogEntryKind.WorldChildMap,
                worldKey,
                candidate);
            IReadOnlyList<string>? merged = store.Worlds.worldMapValidation.TryMergeLayerOrder(worldKey, placements, children);
            if (merged is null)
                return false;
            worldCandidate = WorldDataService.replaceWorldComposition(world, merged, placements);
            if (!store.Worlds.worldMapValidation.Validate(worldKey, worldCandidate, children).IsValid)
                return false;
        }
        if (ProjectDataStore.nodesEqual(current, candidate)
            && string.Equals(currentKey, newKey, StringComparison.Ordinal)
            && worldCandidate is null)
        {
            return true;
        }
        return store.commitResourceChange("Maps", currentKey, newKey, candidate, worldCandidate);
    }

    public string? CopyMap(string key)
    {
        if (store.Worlds.TryGetWorldForMap(key, out _))
            return store.Worlds.CopyWorldChildMap(key);
        if (getMap(key) is not JsonObject source)
            return null;
        return PasteMap(source, key);
    }

    public string? PasteMap(JsonObject source, string sourceKey)
    {
        if (source is null || string.IsNullOrWhiteSpace(sourceKey))
            return null;
        sourceKey = Path.GetFileName(normaliseMapKey(sourceKey));
        string baseKey = sourceKey + " (copy)";
        string copyKey = baseKey;
        for (int index = 1; !store.canCreateDocument("Maps", copyKey); index += 1)
            copyKey = $"{baseKey} ({index})";
        JsonObject copy = (JsonObject)source.DeepClone();
        string mapName = copy["mapName"]?.GetValue<string>() ?? sourceKey;
        copy["mapName"] = mapName + " (copy)";
        mapDocuments.RecordChange(copyKey);
        mapDocuments[copyKey] = copy;
        updateLoadedMapMetadata(copyKey, copy);
        setMapCatalogEntry(createMapCatalogEntry(
            copyKey,
            MapCatalogEntryKind.StandaloneMap,
            null,
            copy));
        store.refreshModifiedState();
        return copyKey;
    }

    public bool DeleteMap(string key)
    {
        return store.DeleteDocumentResource("Maps", normaliseMapKey(key));
    }

    public IReadOnlyList<string> GetMapsReferencingTileset(string key)
    {
        key = ProjectDataStore.normalizeDataKey(key);
        return MapCatalog
            .Where(entry => entry.Kind != MapCatalogEntryKind.WorldMap)
            .Where(entry => ReadMapSnapshotWithoutCaching(entry.Key)?["layers"] is JsonObject layers
                && layers.Any(layer => string.Equals(
                    ProjectDataStore.getString(layer.Value?["layerTileset"]),
                    key,
                    StringComparison.Ordinal)))
            .Select(entry => entry.Key)
            .OrderBy(mapKey => mapKey, StringComparer.Ordinal)
            .ToArray();
    }

    public IReadOnlyList<string> getLayerNames(string mapKey)
    {
        if (getMap(mapKey) is not JsonObject map)
            return Array.Empty<string>();
        return getLayerOrder(map).Select(name => name!.GetValue<string>()).ToArray();
    }

    public string? getLayerTilesetKey(string mapKey, string layerName)
    {
        return getMap(mapKey)?["layers"]?[layerName]?["layerTileset"]?.GetValue<string>();
    }

    public bool setLayerTilesetKey(string mapKey, string layerName, string tilesetKey)
    {
        JsonObject? layer = getMap(mapKey)?["layers"]?[layerName] as JsonObject;
        if (layer is null || !store.Assets.TilesetData.ContainsKey(tilesetKey))
            return false;
        if (string.Equals(layer["layerTileset"]?.GetValue<string>(), tilesetKey, StringComparison.Ordinal))
            return false;
        store.RecordMapSnapshot(mapKey);
        layer["layerTileset"] = tilesetKey;
        store.refreshModifiedState();
        NotifyMapContentChanged(mapKey);
        return true;
    }

    public string getLayerShaderPath(string mapKey, string layerName)
    {
        return getMap(mapKey)?["layers"]?[layerName]?["shaderPath"]?.GetValue<string>() ?? string.Empty;
    }

    public bool setLayerShaderPath(string mapKey, string layerName, string shaderPath)
    {
        JsonObject? layer = getMap(mapKey)?["layers"]?[layerName] as JsonObject;
        if (layer is null)
            return false;
        string normalizedPath = shaderPath ?? string.Empty;
        if (normalizedPath.Length != 0 && !GameAssetPath.IsCanonical(normalizedPath))
            return false;
        if (string.Equals(layer["shaderPath"]?.GetValue<string>() ?? string.Empty, normalizedPath, StringComparison.Ordinal))
            return false;
        store.RecordMapSnapshot(mapKey);
        layer["shaderPath"] = normalizedPath;
        store.refreshModifiedState();
        return true;
    }

    public bool SetLayerVisible(string mapKey, string layerName, bool visible)
    {
        JsonObject? layer = getMap(mapKey)?["layers"]?[layerName] as JsonObject;
        if (layer is null)
            return false;
        bool current = layer["visible"]?.GetValue<bool?>() ?? true;
        if (current == visible)
            return false;
        store.RecordMapSnapshot(mapKey);
        if (visible)
            layer.Remove("visible");
        else
            layer["visible"] = false;
        store.refreshModifiedState();
        NotifyMapContentChanged(mapKey);
        return true;
    }

    public JsonObject? copyLayer(string mapKey, string layerName)
    {
        return getMap(mapKey)?["layers"]?[layerName] is JsonObject layer
            ? (JsonObject)layer.DeepClone()
            : null;
    }

    public bool addEmptyLayer(string mapKey, string layerName, string? insertAfterLayer = null)
    {
        JsonObject? map = getMap(mapKey);
        JsonObject? layers = map?["layers"] as JsonObject;
        if (layers is null || string.IsNullOrWhiteSpace(layerName)
            || layers.ContainsKey(layerName)
            || store.Assets.TilesetData.Keys.FirstOrDefault() is not { } tilesetKey)
            return false;
        int width = map?["width"]?.GetValue<int?>() ?? 0;
        int height = map?["height"]?.GetValue<int?>() ?? 0;
        if (width <= 0 || height <= 0)
            return false;
        return insertLayer(mapKey, layerName, createEmptyLayer(layerName, tilesetKey, width, height), insertAfterLayer);
    }

    public bool pasteLayer(string mapKey, string layerName, JsonObject layer, string? insertAfterLayer)
    {
        if (string.IsNullOrWhiteSpace(layerName))
            return false;
        JsonObject copy = (JsonObject)layer.DeepClone();
        copy["layerName"] = layerName;
        return insertLayer(mapKey, layerName, copy, insertAfterLayer);
    }

    public bool renameLayer(string mapKey, string oldName, string newName)
    {
        JsonObject? map = getMap(mapKey);
        JsonObject? layers = map?["layers"] as JsonObject;
        JsonObject? actorGroups = map?["actors"] as JsonObject;
        if (map is null || layers is null || string.IsNullOrWhiteSpace(newName)
            || !layers.ContainsKey(oldName)
            || layers.ContainsKey(newName)
            || actorGroups?.ContainsKey(newName) == true)
            return false;
        JsonObject candidate = (JsonObject)map.DeepClone();
        JsonObject candidateLayers = (JsonObject)candidate["layers"]!;
        JsonObject? candidateActorGroups = candidate["actors"] as JsonObject;
        JsonArray layerOrder = getLayerOrder(candidate);
        int layerOrderIndex = findLayerOrderIndex(layerOrder, oldName);
        if (layerOrderIndex < 0)
            throw new InvalidDataException($"Map layerOrder does not contain '{oldName}'.");
        layerOrder[layerOrderIndex] = newName;
        List<KeyValuePair<string, JsonNode?>> entries = candidateLayers
            .Select(entry => new KeyValuePair<string, JsonNode?>(entry.Key, entry.Value))
            .ToList();
        candidateLayers.Clear();
        foreach (KeyValuePair<string, JsonNode?> entry in entries)
        {
            if (entry.Key == oldName && entry.Value is JsonObject layer)
            {
                layer["layerName"] = newName;
                candidateLayers.Add(newName, layer);
            }
            else
                candidateLayers.Add(entry.Key, entry.Value);
        }
        if (candidateActorGroups is not null && candidateActorGroups.ContainsKey(oldName))
        {
            List<KeyValuePair<string, JsonNode?>> actorEntries = candidateActorGroups
                .Select(entry => new KeyValuePair<string, JsonNode?>(entry.Key, entry.Value))
                .ToList();
            candidateActorGroups.Clear();
            foreach (KeyValuePair<string, JsonNode?> entry in actorEntries)
                candidateActorGroups.Add(entry.Key == oldName ? newName : entry.Key, entry.Value);
        }
        return commitMapStructureChange(mapKey, map, candidate);
    }

    public bool removeLayer(string mapKey, string layerName)
    {
        if (getMap(mapKey) is not JsonObject map || map["layers"] is not JsonObject layers || !layers.ContainsKey(layerName))
            return false;
        JsonObject candidate = (JsonObject)map.DeepClone();
        if (candidate["layers"] is not JsonObject candidateLayers)
            return false;
        JsonArray layerOrder = getLayerOrder(candidate);
        int layerOrderIndex = findLayerOrderIndex(layerOrder, layerName);
        if (layerOrderIndex < 0)
            throw new InvalidDataException($"Map layerOrder does not contain '{layerName}'.");
        if (layerOrder.Count == 1)
            return false;
        candidateLayers.Remove(layerName);
        if (candidate["actors"] is JsonObject actorGroups)
            actorGroups.Remove(layerName);
        layerOrder.RemoveAt(layerOrderIndex);
        return commitMapStructureChange(mapKey, map, candidate);
    }

    public bool reorderLayers(string mapKey, string movingLayer, string targetLayer)
    {
        if (movingLayer == targetLayer || getMap(mapKey) is not JsonObject map || map["layers"] is not JsonObject layers)
            return false;
        JsonArray layerOrder = getLayerOrder(map);
        int movingIndex = findLayerOrderIndex(layerOrder, movingLayer);
        int targetIndex = findLayerOrderIndex(layerOrder, targetLayer);
        if (movingIndex < 0 || targetIndex < 0)
            return false;
        JsonObject candidate = (JsonObject)map.DeepClone();
        JsonArray candidateOrder = getLayerOrder(candidate);
        JsonObject candidateLayers = (JsonObject)candidate["layers"]!;
        JsonNode moving = candidateOrder[movingIndex]!.DeepClone();
        candidateOrder.RemoveAt(movingIndex);
        candidateOrder.Insert(Math.Min(targetIndex, candidateOrder.Count), moving);
        List<KeyValuePair<string, JsonNode?>> entries = candidateOrder
            .Select(name => new KeyValuePair<string, JsonNode?>(
                name!.GetValue<string>(),
                candidateLayers[name.GetValue<string>()]))
            .ToList();
        candidateLayers.Clear();
        foreach (KeyValuePair<string, JsonNode?> entry in entries)
            candidateLayers.Add(entry.Key, entry.Value);
        return commitMapStructureChange(mapKey, map, candidate);
    }

}
