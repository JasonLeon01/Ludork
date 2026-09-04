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
    public JsonObject? getMap(string key)
    {
        key = normaliseMapKey(key);
        if (sections["Maps"].Data.TryGetValue(key, out JsonObject? value))
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
            addInvalidLoadPath(path);
            return null;
        }
        sections["Maps"].Data[key] = loaded;
        originData["Maps"][key] = (JsonObject)loaded.DeepClone();
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
            BgmFilter = cloneObject(map["bgmFilter"]),
            Bgs = map["bgs"]?.GetValue<string>() ?? string.Empty,
            BgsFilter = cloneObject(map["bgsFilter"]),
            Fog = map["fog"]?.GetValue<string>() ?? string.Empty,
            FogPower = map["fogPower"]?.GetValue<int?>() ?? 0,
            FogOx = map["fogOx"]?.GetValue<double?>() ?? 0.0,
            FogOy = map["fogOy"]?.GetValue<double?>() ?? 0.0,
            FogDistort = map["fogDistort"]?.GetValue<int?>() ?? 0,
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
        if (!isValidMapChildName(key) || containsMapKey(key) || sections["WorldMaps"].Data.ContainsKey(key)
            || !isValidMapSize(info.Width, info.Height)
            || TilesetData.Keys.FirstOrDefault() is not { } tilesetKey)
            return false;
        string mapPath = getMapDataPath(key);
        if (File.Exists(mapPath) || Directory.Exists(Path.ChangeExtension(mapPath, null)))
            return false;
        RecordSnapshot();
        JsonObject map = createMapData(info, tilesetKey);
        sections["Maps"].Data[key] = map;
        updateLoadedMapMetadata(key, map);
        setMapCatalogEntry(createMapCatalogEntry(
            key,
            MapCatalogEntryKind.StandaloneMap,
            null,
            map));
        refreshModifiedState();
        return true;
    }

    public bool UpdateMap(string currentKey, MapInfo info)
    {
        currentKey = normaliseMapKey(currentKey);
        if (info is null || getMap(currentKey) is not JsonObject current)
            return false;
        string requestedKey = normaliseMapKey(info.FileName);
        bool childMap = TryGetWorldForMap(currentKey, out string worldKey);
        string newKey;
        if (childMap)
        {
            string childName = requestedKey.Contains('/') ? Path.GetFileName(requestedKey) : requestedKey;
            newKey = worldKey + "/" + childName;
            if (!isValidMapChildName(childName)
                || requestedKey.Contains('/')
                    && !string.Equals(requestedKey, newKey, StringComparison.Ordinal))
            {
                return false;
            }
        }
        else
        {
            if (!isValidMapChildName(requestedKey))
                return false;
            newKey = requestedKey;
        }
        if (!isValidMapSize(info.Width, info.Height)
            || newKey != currentKey
                && (containsMapKey(newKey)
                    || sections["WorldMaps"].Data.ContainsKey(newKey)
                    || File.Exists(getMapDataPath(newKey))))
            return false;
        JsonObject candidate = (JsonObject)current.DeepClone();
        int oldWidth = candidate["width"]?.GetValue<int?>() ?? 0;
        int oldHeight = candidate["height"]?.GetValue<int?>() ?? 0;
        if (oldWidth != info.Width || oldHeight != info.Height)
            resizeMapLayers(candidate, info.Width, info.Height);
        if (!string.IsNullOrWhiteSpace(info.MapName))
            candidate["mapName"] = info.MapName.Trim();
        candidate["width"] = info.Width;
        candidate["height"] = info.Height;
        candidate["ambientLight"] = normaliseAmbientLight(info.AmbientLight);
        candidate["bgm"] = info.Bgm.Trim();
        candidate["bgmFilter"] = cloneObject(info.BgmFilter);
        candidate["bgs"] = info.Bgs.Trim();
        candidate["bgsFilter"] = cloneObject(info.BgsFilter);
        candidate["fog"] = info.Fog.Trim();
        candidate["fogPower"] = string.IsNullOrWhiteSpace(info.Fog) ? 0 : info.FogPower;
        candidate["fogOx"] = string.IsNullOrWhiteSpace(info.Fog) ? 0.0 : info.FogOx;
        candidate["fogOy"] = string.IsNullOrWhiteSpace(info.Fog) ? 0.0 : info.FogOy;
        candidate["fogDistort"] = string.IsNullOrWhiteSpace(info.Fog) ? 0 : info.FogDistort;
        JsonObject? worldCandidate = null;
        if (childMap && getWorldMap(worldKey) is JsonObject world)
        {
            WorldMapValidationResult worldValidation = ValidateWorldMap(worldKey);
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
            Dictionary<string, MapCatalogEntry> children = getWorldChildCatalog(worldKey)
                .Where(item => !string.Equals(item.Key, currentKey, StringComparison.Ordinal))
                .ToDictionary(item => item.Key, item => item.Value, StringComparer.Ordinal);
            children[newKey] = createMapCatalogEntry(
                newKey,
                MapCatalogEntryKind.WorldChildMap,
                worldKey,
                candidate);
            IReadOnlyList<string>? merged = worldMapValidation.TryMergeLayerOrder(worldKey, placements, children);
            if (merged is null)
                return false;
            worldCandidate = replaceWorldComposition(world, merged, placements);
            if (!worldMapValidation.Validate(worldKey, worldCandidate, children).IsValid)
                return false;
        }
        if (nodesEqual(current, candidate)
            && string.Equals(currentKey, newKey, StringComparison.Ordinal)
            && worldCandidate is null)
        {
            return true;
        }
        recordSnapshot(new HashSet<string>([currentKey], StringComparer.Ordinal));
        sections["Maps"].Data.Remove(currentKey);
        sections["Maps"].Data[newKey] = candidate;
        if (!string.Equals(currentKey, newKey, StringComparison.Ordinal))
            rekeyLoadedMapMetadata(currentKey, newKey);
        updateLoadedMapMetadata(newKey, candidate);
        MapCatalogEntryKind kind = childMap
            ? MapCatalogEntryKind.WorldChildMap
            : MapCatalogEntryKind.StandaloneMap;
        removeMapCatalogEntry(kind, currentKey);
        setMapCatalogEntry(createMapCatalogEntry(newKey, kind, childMap ? worldKey : null, candidate));
        if (worldCandidate is not null)
        {
            sections["WorldMaps"].Data[worldKey] = worldCandidate;
            setWorldCatalogLayerOrder(worldKey, readStringArray(worldCandidate["layerOrder"]));
        }
        NotifyMapContentChanged(currentKey);
        if (!string.Equals(currentKey, newKey, StringComparison.Ordinal))
            NotifyMapContentChanged(newKey);
        refreshModifiedState();
        return true;
    }

    public string? CopyMap(string key)
    {
        if (TryGetWorldForMap(key, out _))
            return CopyWorldChildMap(key);
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
        for (int index = 1; MapData.ContainsKey(copyKey); index += 1)
            copyKey = $"{baseKey} ({index})";
        JsonObject copy = (JsonObject)source.DeepClone();
        string mapName = copy["mapName"]?.GetValue<string>() ?? sourceKey;
        copy["mapName"] = mapName + " (copy)";
        RecordSnapshot();
        sections["Maps"].Data[copyKey] = copy;
        updateLoadedMapMetadata(copyKey, copy);
        setMapCatalogEntry(createMapCatalogEntry(
            copyKey,
            MapCatalogEntryKind.StandaloneMap,
            null,
            copy));
        refreshModifiedState();
        return copyKey;
    }

    public bool DeleteMap(string key)
    {
        key = normaliseMapKey(key);
        if (!tryGetMapCatalogEntry(key, out MapCatalogEntry entry) || getMap(key) is null)
        {
            return false;
        }
        recordSnapshot(new HashSet<string>([key], StringComparer.Ordinal));
        sections["Maps"].Data.Remove(key);
        removeLoadedMapMetadata(key);
        removeMapCatalogEntry(entry.Kind, key);
        if (entry.Kind == MapCatalogEntryKind.WorldChildMap
            && entry.WorldKey is not null
            && getWorldMap(entry.WorldKey) is JsonObject world)
        {
            WorldMapValidationResult validation = ValidateWorldMap(entry.WorldKey);
            string childFile = Path.GetFileName(key) + ".json";
            List<WorldMapPlacement> placements = validation.Placements
                .Where(placement => !string.Equals(placement.Map, childFile, StringComparison.Ordinal))
                .ToList();
            Dictionary<string, MapCatalogEntry> children = getWorldChildCatalog(entry.WorldKey)
                .Where(item => !string.Equals(item.Key, key, StringComparison.Ordinal))
                .ToDictionary(item => item.Key, item => item.Value, StringComparer.Ordinal);
            IReadOnlyList<string> layerOrder = worldMapValidation.TryMergeLayerOrder(
                entry.WorldKey,
                placements,
                children) ?? [];
            sections["WorldMaps"].Data[entry.WorldKey] = replaceWorldComposition(
                world,
                layerOrder,
                placements);
            setWorldCatalogLayerOrder(entry.WorldKey, layerOrder);
        }
        NotifyMapContentChanged(key);
        refreshModifiedState();
        return true;
    }

    public bool CreateAnimation(string key, string name)
    {
        key = normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key) || key.EndsWith(".anim", StringComparison.OrdinalIgnoreCase)
            || sections["Animations"].Data.ContainsKey(key))
            return false;
        RecordSnapshot();
        sections["Animations"].Data[key] = new JsonObject
        {
            ["name"] = name,
            ["frameRate"] = 30,
            ["assets"] = new JsonArray(),
            ["timeLines"] = new JsonArray(),
            ["timeTags"] = new JsonArray(),
        };
        refreshModifiedState();
        return true;
    }

    public bool UpdateAnimation(string key, JsonObject animation)
    {
        key = normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key) || !sections["Animations"].Data.ContainsKey(key))
            return false;
        JsonObject copy = (JsonObject)animation.DeepClone();
        copy.Remove("type");
        if (nodesEqual(sections["Animations"].Data[key], copy))
            return false;
        RecordSnapshot();
        sections["Animations"].Data[key] = copy;
        refreshModifiedState();
        return true;
    }

    public bool CreateCurve(string key, string name, string type = "curve")
    {
        key = normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key)
            || !isCurveType(type)
            || sections["Curves"].Data.ContainsKey(key))
        {
            return false;
        }
        int componentCount = curveComponentCount(type);
        RecordSnapshot();
        sections["Curves"].Data[key] = new JsonObject
        {
            ["type"] = type,
            ["name"] = name,
            ["defaultValue"] = createCurveValue(componentCount, 0.0),
            ["preInfinity"] = "constant",
            ["postInfinity"] = "constant",
            ["keys"] = new JsonArray
            {
                createCurveKey(0.0, createCurveValue(componentCount, 0.0)),
                createCurveKey(1.0, createCurveValue(componentCount, 1.0)),
            },
        };
        refreshModifiedState();
        return true;
    }

    public bool CreateTextConfig(string key, string type, string name)
    {
        key = normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key)
            || !isTextConfigType(type)
            || sections["TextConfigs"].Data.ContainsKey(key))
        {
            return false;
        }
        RecordSnapshot();
        sections["TextConfigs"].Data[key] = type == "plainTextConfig"
            ? createPlainTextConfig(name)
            : createRichTextConfig(name);
        refreshModifiedState();
        return true;
    }

    public bool UpdateTextConfig(string key, JsonObject textConfig)
    {
        key = normalizeDataKey(key);
        string? type = textConfig["type"]?.GetValue<string>();
        if (string.IsNullOrWhiteSpace(key)
            || !isTextConfigType(type)
            || !sections["TextConfigs"].Data.ContainsKey(key))
        {
            return false;
        }
        JsonObject copy = (JsonObject)textConfig.DeepClone();
        if (nodesEqual(sections["TextConfigs"].Data[key], copy))
            return false;
        RecordSnapshot();
        sections["TextConfigs"].Data[key] = copy;
        refreshModifiedState();
        return true;
    }

    public bool DeleteTextConfig(string key)
    {
        return DeleteTextConfigs([key]);
    }

    public bool DeleteTextConfigs(IEnumerable<string> keys)
    {
        string[] normalizedKeys = keys
            .Select(normalizeJsonKey)
            .Where(key => sections["TextConfigs"].Data.ContainsKey(key))
            .Distinct(StringComparer.Ordinal)
            .ToArray();
        if (normalizedKeys.Length == 0)
            return false;
        RecordSnapshot();
        foreach (string key in normalizedKeys)
            sections["TextConfigs"].Data.Remove(key);
        refreshModifiedState();
        return true;
    }

    public IReadOnlyList<string> GetMapsReferencingTileset(string key)
    {
        key = normalizeDataKey(key);
        return MapCatalog
            .Where(entry => entry.Kind != MapCatalogEntryKind.WorldMap)
            .Where(entry => ReadMapSnapshotWithoutCaching(entry.Key)?["layers"] is JsonObject layers
                && layers.Any(layer => string.Equals(
                    getString(layer.Value?["layerTileset"]),
                    key,
                    StringComparison.Ordinal)))
            .Select(entry => entry.Key)
            .OrderBy(mapKey => mapKey, StringComparer.Ordinal)
            .ToArray();
    }

    public bool RenameTileset(string oldKey, string newKey, bool updateReferences)
    {
        oldKey = normalizeDataKey(oldKey);
        newKey = normalizeDataKey(newKey);
        Dictionary<string, JsonObject> tilesets = sections["Tilesets"].Data;
        if (newKey.Length == 0
            || !tilesets.ContainsKey(oldKey)
            || tilesets.ContainsKey(newKey))
        {
            return false;
        }
        IReadOnlyList<string> referencingMapKeys = GetMapsReferencingTileset(oldKey);
        if (referencingMapKeys.Count != 0 && !updateReferences)
            return false;
        List<(string Key, JsonObject Data)> referencingMaps = [];
        foreach (string mapKey in referencingMapKeys)
        {
            if (getMap(mapKey) is not JsonObject map)
                return false;
            referencingMaps.Add((mapKey, map));
        }

        RecordSnapshot();
        List<KeyValuePair<string, JsonObject>> entries = tilesets.ToList();
        tilesets.Clear();
        foreach (KeyValuePair<string, JsonObject> entry in entries)
            tilesets.Add(entry.Key == oldKey ? newKey : entry.Key, entry.Value);
        foreach ((string mapKey, JsonObject map) in referencingMaps)
        {
            if (map["layers"] is not JsonObject layers)
                continue;
            foreach (JsonObject layer in layers.Select(entry => entry.Value).OfType<JsonObject>())
            {
                if (string.Equals(getString(layer["layerTileset"]), oldKey, StringComparison.Ordinal))
                    layer["layerTileset"] = newKey;
            }
            NotifyMapContentChanged(mapKey);
        }
        NotifyAllMapPreviewsChanged();
        refreshModifiedState();
        return true;
    }

    public bool CreateTileset(string key)
    {
        key = normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key) || sections["Tilesets"].Data.ContainsKey(key))
            return false;
        RecordSnapshot();
        sections["Tilesets"].Data[key] = new JsonObject
        {
            ["name"] = key,
            ["fileName"] = string.Empty,
            ["passable"] = new JsonArray(),
            ["materials"] = new JsonArray(),
            ["dir4"] = new JsonArray(),
        };
        NotifyAllMapPreviewsChanged();
        refreshModifiedState();
        return true;
    }

    public bool CreateAutoTile(string key)
    {
        key = normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key) || sections["AutoTiles"].Data.ContainsKey(key))
            return false;
        RecordSnapshot();
        sections["AutoTiles"].Data[key] = new JsonObject
        {
            ["name"] = key,
            ["fileName"] = string.Empty,
            ["passable"] = true,
            ["material"] = createDefaultMaterial(),
        };
        NotifyAllMapPreviewsChanged();
        refreshModifiedState();
        return true;
    }

    public bool UpdateCurve(string key, JsonObject curve)
    {
        key = normalizeDataKey(key);
        string? type = curve["type"]?.GetValue<string>();
        if (string.IsNullOrWhiteSpace(key)
            || !isCurveType(type)
            || !sections["Curves"].Data.ContainsKey(key))
        {
            return false;
        }
        JsonObject copy = (JsonObject)curve.DeepClone();
        if (nodesEqual(sections["Curves"].Data[key], copy))
            return false;
        RecordSnapshot();
        sections["Curves"].Data[key] = copy;
        refreshModifiedState();
        return true;
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
        if (layer is null || !TilesetData.ContainsKey(tilesetKey))
            return false;
        if (string.Equals(layer["layerTileset"]?.GetValue<string>(), tilesetKey, StringComparison.Ordinal))
            return false;
        RecordMapSnapshot(mapKey);
        layer["layerTileset"] = tilesetKey;
        NotifyMapContentChanged(mapKey);
        refreshModifiedState();
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
        RecordMapSnapshot(mapKey);
        layer["shaderPath"] = normalizedPath;
        refreshModifiedState();
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
        RecordMapSnapshot(mapKey);
        if (visible)
            layer.Remove("visible");
        else
            layer["visible"] = false;
        NotifyMapContentChanged(mapKey);
        refreshModifiedState();
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
            || TilesetData.Keys.FirstOrDefault() is not { } tilesetKey)
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

    public int getCellSize()
    {
        int? value = SystemConfigData.TryGetValue("System", out JsonObject? system)
            ? system["cellSize"]?["value"]?.GetValue<int?>()
            : null;
        return value is > 0 ? value.Value : 32;
    }

    public (int Width, int Height) getGameSize()
    {
        if (!SystemConfigData.TryGetValue("System", out JsonObject? system)
            || system["gameSize"]?["value"] is not JsonArray values
            || values.Count < 2)
        {
            return (640, 480);
        }
        int width = values[0]?.GetValue<int?>() ?? 640;
        int height = values[1]?.GetValue<int?>() ?? 480;
        return (width > 0 ? width : 640, height > 0 ? height : 480);
    }

    public string getGameTitle()
    {
        if (!SystemConfigData.TryGetValue("System", out JsonObject? system))
            return "Ludork";
        string? title = system["title"]?["value"]?.GetValue<string>();
        return string.IsNullOrWhiteSpace(title) ? "Ludork" : $"Ludork - {title.Trim()}";
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
