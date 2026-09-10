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
    private bool insertLayer(string mapKey, string layerName, JsonObject layer, string? insertAfterLayer)
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

    private static JsonArray getLayerOrder(JsonObject map)
    {
        return map["layerOrder"] as JsonArray
            ?? throw new InvalidDataException("Map data must contain a layerOrder array.");
    }

    private static int findLayerOrderIndex(JsonArray layerOrder, string layerName)
    {
        for (int index = 0; index < layerOrder.Count; index += 1)
        {
            if (string.Equals(layerOrder[index]?.GetValue<string>(), layerName, StringComparison.Ordinal))
                return index;
        }
        return -1;
    }

    private static JsonObject createEmptyLayer(string layerName, string tilesetKey, int width, int height)
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

    private static string normalizeDataKey(string key)
    {
        return key.Replace('\\', '/').Trim().Trim('/');
    }

    private static string normalizeJsonKey(string key)
    {
        string normalized = normalizeDataKey(key);
        return normalized.EndsWith(".json", StringComparison.OrdinalIgnoreCase)
            ? normalized[..^5]
            : normalized;
    }

    private bool renameDataEntry(string sectionName, string oldKey, string newKey)
    {
        return RenameDocumentResource(sectionName, oldKey, newKey);
    }

    private bool deleteDataEntry(string sectionName, string key)
    {
        return DeleteDocumentResource(sectionName, key);
    }
    private static string normaliseMapKey(string fileName)
    {
        string key = normalizeDataKey(fileName);
        return key.EndsWith(".json", StringComparison.OrdinalIgnoreCase) ? key[..^5] : key;
    }

    private static bool isValidMapSize(int width, int height)
    {
        return width is >= 1 and <= 32768 && height is >= 1 and <= 32768;
    }

    private static JsonObject cloneObject(JsonNode? value)
    {
        return value is JsonObject objectValue ? (JsonObject)objectValue.DeepClone() : new JsonObject();
    }

    private static JsonArray normaliseAmbientLight(JsonArray? values)
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

    private static void resizeMapLayers(JsonObject map, int width, int height)
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

    private static JsonArray resizeGrid(JsonArray? source, int width, int height)
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

    private void renameMapKey(string currentKey, string newKey, JsonObject map)
    {
        Dictionary<string, JsonObject> maps = sections["Maps"].Data;
        List<KeyValuePair<string, JsonObject>> entries = maps
            .Select(entry => new KeyValuePair<string, JsonObject>(entry.Key == currentKey ? newKey : entry.Key, entry.Key == currentKey ? map : entry.Value))
            .ToList();
        maps.Clear();
        foreach (KeyValuePair<string, JsonObject> entry in entries)
            maps.Add(entry.Key, entry.Value);
    }

    private static JsonObject createDefaultMaterial()
    {
        return TilesetMetadata.CreateDefaultMaterial();
    }

    private static JsonObject createCurveKey(double time, JsonNode value)
    {
        int componentCount = value is JsonArray values ? values.Count : 1;
        return new JsonObject
        {
            ["time"] = time,
            ["value"] = value,
            ["interpolation"] = "linear",
            ["arriveTangent"] = createCurveValue(componentCount, 0.0),
            ["leaveTangent"] = createCurveValue(componentCount, 0.0),
        };
    }

    private static JsonNode createCurveValue(int componentCount, double value)
    {
        if (componentCount == 1)
            return JsonValue.Create(value);
        JsonArray result = new();
        for (int index = 0; index < componentCount; index += 1)
            result.Add(value);
        return result;
    }

    private static JsonObject createPlainTextConfig(string name)
    {
        return new JsonObject
        {
            ["type"] = "plainTextConfig",
            ["name"] = name,
            ["font"] = string.Empty,
            ["characterSize"] = 22,
            ["style"] = createTextStyleFlags(),
            ["slantAngle"] = 0.0,
            ["fillColor"] = createColour(255, 255, 255, 255),
            ["letterSpacing"] = 1.0,
            ["lineSpacing"] = 1.0,
            ["lineAlignment"] = "default",
            ["outline"] = createOutline(),
            ["glow"] = createGlow(),
            ["gradient"] = createGradient(),
        };
    }

    private static JsonObject createRichTextConfig(string name)
    {
        return new JsonObject
        {
            ["type"] = "richTextConfig",
            ["name"] = name,
            ["font"] = string.Empty,
            ["lineAlignment"] = "default",
            ["defaultStyle"] = new JsonObject
            {
                ["characterSize"] = 22,
                ["style"] = createTextStyleFlags(),
                ["fillColor"] = createColour(255, 255, 255, 255),
                ["letterSpacing"] = 1.0,
                ["lineSpacing"] = 1.0,
                ["outline"] = createOutline(),
            },
            ["styleOrder"] = new JsonArray(),
            ["styles"] = new JsonObject(),
            ["glow"] = createGlow(),
            ["gradient"] = createGradient(),
        };
    }

    private static JsonObject createTextStyleFlags()
    {
        return new JsonObject
        {
            ["bold"] = false,
            ["italic"] = false,
            ["underlined"] = false,
            ["strikeThrough"] = false,
        };
    }

    private static JsonArray createColour(int red, int green, int blue, int alpha)
    {
        return new JsonArray(red, green, blue, alpha);
    }

    private static JsonObject createOutline()
    {
        return new JsonObject
        {
            ["color"] = createColour(0, 0, 0, 255),
            ["thickness"] = 0.0,
        };
    }

    private static JsonObject createGlow()
    {
        return new JsonObject
        {
            ["enabled"] = false,
            ["color"] = createColour(255, 255, 255, 0),
            ["radius"] = 0.0,
            ["intensity"] = 0.0,
        };
    }

    private static JsonObject createGradient()
    {
        return new JsonObject
        {
            ["enabled"] = false,
            ["direction"] = "vertical",
            ["curve"] = string.Empty,
        };
    }

    private static bool isCurveType(string? type)
    {
        return type is "curve" or "vector2Curve" or "vector3Curve" or "vector4Curve";
    }

    private static int curveComponentCount(string type)
    {
        return type switch
        {
            "vector2Curve" => 2,
            "vector3Curve" => 3,
            "vector4Curve" => 4,
            _ => 1,
        };
    }

    private static bool isTextConfigType(string? type)
    {
        return type is "plainTextConfig" or "richTextConfig";
    }

    private Dictionary<string, Dictionary<string, JsonObject>> cloneAllData()
    {
        return sections.ToDictionary(
            section => section.Key,
            section => section.Value.Data.ToDictionary(item => item.Key,
                item => (JsonObject)item.Value.DeepClone(), StringComparer.Ordinal),
            StringComparer.Ordinal);
    }

    private static bool hasDataFileExtension(string sectionName, string path)
    {
        StringComparison comparison = sectionName == "UI"
            ? StringComparison.Ordinal
            : StringComparison.OrdinalIgnoreCase;
        return string.Equals(
            Path.GetExtension(path),
            DataConfig.DataFileExtension,
            comparison);
    }

    private static string formatSaveDetails(
        IReadOnlyList<string> added,
        IReadOnlyList<string> updated,
        IReadOnlyList<string> deleted,
        IReadOnlyList<string> failed
    )
    {
        List<string> lines = [];
        if (added.Count != 0)
            lines.Add($"A [{string.Join(", ", added)}]");
        if (updated.Count != 0)
            lines.Add($"U [{string.Join(", ", updated)}]");
        if (deleted.Count != 0)
            lines.Add($"D [{string.Join(", ", deleted)}]");
        if (failed.Count != 0)
            lines.Add($"Failed [{string.Join(", ", failed)}]");
        return "\n" + string.Join("\n", lines);
    }

    private static bool nodesEqual(JsonNode current, JsonNode origin)
    {
        return string.Equals(current.ToJsonString(), origin.ToJsonString(), StringComparison.Ordinal);
    }

    private static void deleteTemporaryFile(string path)
    {
        try
        {
            if (File.Exists(path))
                File.Delete(path);
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or System.Security.SecurityException)
        {
        }
    }

    private static bool isUiDataType(JsonObject data, string expectedType)
    {
        return string.Equals(getString(data["type"]), expectedType, StringComparison.Ordinal);
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    private sealed class LazyMapDataDictionary : IReadOnlyDictionary<string, JsonObject>
    {
        private readonly GameDataService owner;

        public LazyMapDataDictionary(GameDataService owner)
        {
            this.owner = owner;
        }

        public JsonObject this[string key] => owner.ReadMapSnapshot(key)
            ?? throw new KeyNotFoundException(key);
        public IEnumerable<string> Keys => owner.getAllMapKeys();
        public IEnumerable<JsonObject> Values => Keys.Select(key => this[key]);
        public int Count => owner.getAllMapKeys().Count;

        public bool ContainsKey(string key)
        {
            return owner.containsMapKey(normaliseMapKey(key));
        }

        public bool TryGetValue(string key, out JsonObject value)
        {
            JsonObject? result = owner.ReadMapSnapshot(key);
            value = result!;
            return result is not null;
        }

        public IEnumerator<KeyValuePair<string, JsonObject>> GetEnumerator()
        {
            foreach (string key in Keys)
                yield return new KeyValuePair<string, JsonObject>(key, this[key]);
        }

        System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }
    }

    private sealed record MapActorTagLocation(string LayerName, int ActorIndex);
    private sealed record MapActorCollection(string LocationKey, JsonArray Actors);

    private sealed class MapCatalogLayerShape
    {
        public string? ActiveGrid { get; set; }
        public IReadOnlyList<int>? TilesRows { get; set; }
        public IReadOnlyList<int>? AutoTileRows { get; set; }
    }

    private sealed class DataSection
    {
        private readonly HashSet<string>? acceptedTypes;

        public DataSection(string? expectedType, bool writeType)
            : this(expectedType, writeType, true)
        {
        }

        public DataSection(string? expectedType, bool writeType, bool persist)
        {
            ExpectedType = expectedType;
            WriteType = writeType;
            Persist = persist;
        }

        public DataSection(IEnumerable<string> acceptedTypes)
        {
            this.acceptedTypes = new HashSet<string>(acceptedTypes, StringComparer.Ordinal);
            PreserveType = true;
        }

        public string? ExpectedType { get; }
        public bool WriteType { get; }
        public bool PreserveType { get; }
        public bool Persist { get; } = true;
        public Dictionary<string, JsonObject> Data { get; } = new(StringComparer.Ordinal);
        private JsonSnapshotDictionary? snapshots;
        public JsonSnapshotDictionary Snapshots => snapshots ??= new JsonSnapshotDictionary(Data);

        public bool AcceptsType(string? type)
        {
            if (acceptedTypes is not null)
                return type is not null && acceptedTypes.Contains(type);
            return ExpectedType is null
                || type is null
                || string.Equals(type, ExpectedType, StringComparison.Ordinal);
        }
    }
}
