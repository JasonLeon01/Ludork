using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal static class TilesetMetadata
{
    public static JsonObject CreateDefaultMaterial() => new()
    {
        ["lightBlock"] = 0.0,
        ["mirror"] = false,
        ["reflectionStrength"] = 0.5,
        ["opacity"] = 1.0,
        ["speedRate"] = 1.0,
    };

    public static bool IsValidValue(string property, JsonNode value, bool isAutoTile)
    {
        return property switch
        {
            "passable" => value is JsonValue boolean && boolean.TryGetValue(out bool _),
            "material" => isAutoTile && value is JsonObject,
            "materials" => !isAutoTile && value is JsonObject,
            "dir4" => !isAutoTile && value is JsonArray directions && directions.Count == 4
                && directions[0] is JsonValue down && down.TryGetValue(out bool _)
                && directions[1] is JsonValue left && left.TryGetValue(out bool _)
                && directions[2] is JsonValue right && right.TryGetValue(out bool _)
                && directions[3] is JsonValue up && up.TryGetValue(out bool _),
            _ => false,
        };
    }

    public static JsonNode Read(JsonObject source, string property, int index, bool isAutoTile)
    {
        JsonNode? value = isAutoTile
            ? source[property]
            : source[property] is JsonArray values && index >= 0 && index < values.Count ? values[index] : null;
        return value?.DeepClone() ?? CreateDefaultValue(property, isAutoTile);
    }

    public static void Apply(JsonObject target, string property, JsonNode value, IReadOnlyList<int> indices, int count, bool isAutoTile)
    {
        if (isAutoTile)
        {
            target[property] = value.DeepClone();
            return;
        }
        JsonArray values = Resize(target, property, count, () => CreateDefaultValue(property, false));
        foreach (int index in indices)
            values[index] = value.DeepClone();
    }

    public static void ResizeForImage(JsonObject target, int count)
    {
        Resize(target, "passable", count, () => JsonValue.Create(true)!);
        Resize(target, "materials", count, CreateDefaultMaterial);
        Resize(target, "dir4", count, () => new JsonArray(true, true, true, true));
    }

    public static JsonObject MergeMaterial(JsonObject current, JsonObject initial, JsonObject edited)
    {
        JsonObject result = (JsonObject)current.DeepClone();
        foreach (KeyValuePair<string, JsonNode?> field in edited)
            if (!JsonNode.DeepEquals(initial[field.Key], field.Value))
                result[field.Key] = field.Value?.DeepClone();
        foreach (KeyValuePair<string, JsonNode?> field in initial)
            if (!edited.ContainsKey(field.Key))
                result.Remove(field.Key);
        return result;
    }

    private static JsonNode CreateDefaultValue(string property, bool isAutoTile) => property switch
    {
        "passable" => JsonValue.Create(isAutoTile)!,
        "material" or "materials" => CreateDefaultMaterial(),
        "dir4" => new JsonArray(true, true, true, true),
        _ => throw new ArgumentException(nameof(property)),
    };

    private static JsonArray Resize(JsonObject target, string property, int count, Func<JsonNode> createValue)
    {
        JsonArray values = target[property] as JsonArray ?? new JsonArray();
        while (values.Count < count)
            values.Add(createValue());
        while (values.Count > count)
            values.RemoveAt(values.Count - 1);
        target[property] = values;
        return values;
    }
}
