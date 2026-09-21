using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    internal bool insertLayer(string mapKey, string layerName, JsonObject layer, string? insertAfterLayer)
    {
        if (getMap(mapKey) is not JsonObject map || map["layers"] is not JsonObject layers || layers.ContainsKey(layerName))
            return false;
        JsonObject candidate = (JsonObject)map.DeepClone();
        JsonObject candidateLayers = (JsonObject)candidate["layers"]!;
        JsonArray layerOrder = getLayerOrder(candidate);
        int insertIndex = string.IsNullOrWhiteSpace(insertAfterLayer)
            ? layerOrder.Count
            : findLayerOrderIndex(layerOrder, insertAfterLayer) + 1;
        if (insertIndex <= 0)
            insertIndex = layerOrder.Count;
        layerOrder.Insert(insertIndex, layerName);
        candidateLayers.Add(layerName, layer.DeepClone());
        List<KeyValuePair<string, JsonNode?>> entries = layerOrder
            .Select(name => new KeyValuePair<string, JsonNode?>(
                name!.GetValue<string>(),
                candidateLayers[name.GetValue<string>()]))
            .ToList();
        candidateLayers.Clear();
        foreach (KeyValuePair<string, JsonNode?> entry in entries)
            candidateLayers.Add(entry.Key, entry.Value);
        return commitMapStructureChange(mapKey, map, candidate);
    }

    internal static JsonArray getLayerOrder(JsonObject map)
    {
        return map["layerOrder"] as JsonArray
            ?? throw new InvalidDataException("Map data must contain a layerOrder array.");
    }

    internal static int findLayerOrderIndex(JsonArray layerOrder, string layerName)
    {
        for (int index = 0; index < layerOrder.Count; index += 1)
        {
            if (string.Equals(layerOrder[index]?.GetValue<string>(), layerName, StringComparison.Ordinal))
                return index;
        }
        return -1;
    }

    internal static JsonObject createEmptyLayer(string layerName, string tilesetKey, int width, int height)
    {
        JsonArray tiles = new JsonArray();
        JsonArray autoTiles = new JsonArray();
        for (int y = 0; y < height; y++)
        {
            JsonArray tileRow = new JsonArray();
            JsonArray autoTileRow = new JsonArray();
            for (int x = 0; x < width; x++)
            {
                tileRow.Add(null);
                autoTileRow.Add(null);
            }
            tiles.Add(tileRow);
            autoTiles.Add(autoTileRow);
        }
        return new JsonObject
        {
            ["layerName"] = layerName,
            ["layerTileset"] = tilesetKey,
            ["tiles"] = tiles,
            ["autoTiles"] = autoTiles,
            ["shaderPath"] = string.Empty,
            ["actors"] = new JsonArray(),
        };
    }

    internal static string normaliseMapKey(string fileName)
    {
        string key = ProjectDataStore.normalizeDataKey(fileName);
        return key.EndsWith(".json", StringComparison.OrdinalIgnoreCase) ? key[..^5] : key;
    }

    internal static bool isValidMapSize(int width, int height)
    {
        return width is >= 1 and <= 32768 && height is >= 1 and <= 32768;
    }

    internal static JsonArray normaliseAmbientLight(JsonArray? values)
    {
        JsonArray result = new JsonArray();
        for (int index = 0; index < 4; index += 1)
        {
            JsonNode? value = values?[index];
            int component = value is JsonValue jsonValue
                && jsonValue.TryGetValue<byte>(out byte byteValue)
                    ? byteValue
                    : value?.GetValue<int?>() ?? 255;
            result.Add(Math.Clamp(component, 0, 255));
        }
        return result;
    }

    internal static void resizeMapLayers(JsonObject map, int width, int height)
    {
        if (map["layers"] is not JsonObject layers)
            return;
        foreach (JsonNode? value in layers.Select(entry => entry.Value))
        {
            if (value is not JsonObject layer)
                continue;
            layer["tiles"] = resizeGrid(layer["tiles"] as JsonArray, width, height);
            layer["autoTiles"] = resizeGrid(layer["autoTiles"] as JsonArray, width, height);
        }
    }

    internal static JsonArray resizeGrid(JsonArray? source, int width, int height)
    {
        JsonArray result = new JsonArray();
        for (int y = 0; y < height; y += 1)
        {
            JsonArray row = new JsonArray();
            JsonArray? sourceRow = source is not null && y < source.Count ? source[y] as JsonArray : null;
            for (int x = 0; x < width; x += 1)
                row.Add(sourceRow is not null && x < sourceRow.Count ? sourceRow[x]?.DeepClone() : null);
            result.Add(row);
        }
        return result;
    }

    internal void renameMapKey(string currentKey, string newKey, JsonObject map)
    {
        EditorDocumentCollection maps = mapDocuments;
        List<KeyValuePair<string, JsonObject>> entries = maps
            .Select(entry => new KeyValuePair<string, JsonObject>(entry.Key == currentKey ? newKey : entry.Key, entry.Key == currentKey ? map : entry.Value))
            .ToList();
        maps.Clear();
        foreach (KeyValuePair<string, JsonObject> entry in entries)
            maps.Add(entry.Key, entry.Value);
    }

    internal sealed class LazyMapDataDictionary : IReadOnlyDictionary<string, MapDocumentSnapshot>
    {
        private readonly ProjectDataStore owner;

        public LazyMapDataDictionary(ProjectDataStore owner)
        {
            this.owner = owner;
        }

        public MapDocumentSnapshot this[string key] => owner.Maps.ReadMapDocument(key)
            ?? throw new KeyNotFoundException(key);
        public IEnumerable<string> Keys => owner.Maps.getAllMapKeys();
        public IEnumerable<MapDocumentSnapshot> Values => Keys.Select(key => this[key]);
        public int Count => owner.Maps.getAllMapKeys().Count;

        public bool ContainsKey(string key)
        {
            return owner.Maps.containsMapKey(normaliseMapKey(key));
        }

        public bool TryGetValue(string key, out MapDocumentSnapshot value)
        {
            MapDocumentSnapshot? result = owner.Maps.ReadMapDocument(key);
            value = result!;
            return result is not null;
        }

        public IEnumerator<KeyValuePair<string, MapDocumentSnapshot>> GetEnumerator()
        {
            foreach (string key in Keys)
                yield return new KeyValuePair<string, MapDocumentSnapshot>(key, this[key]);
        }

        System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }
    }

    internal sealed record MapActorTagLocation(string LayerName, int ActorIndex);

    internal sealed record MapActorCollection(string LocationKey, JsonArray Actors);

    internal sealed class MapCatalogLayerShape
    {
        public string? ActiveGrid { get; set; }
        public IReadOnlyList<int>? TilesRows { get; set; }
        public IReadOnlyList<int>? AutoTileRows { get; set; }
    }

}
