using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal static class MapPreviewSceneDelta
{
    internal sealed class Snapshot(JsonObject scene)
    {
        public Dictionary<string, JsonNode?> Fields { get; } = scene.Where(field => field.Key is not ("regions" or "sprites" or "lights"))
            .ToDictionary(field => field.Key, field => field.Value, StringComparer.Ordinal);
        public Dictionary<string, JsonObject> Regions { get; } = records(scene, "regions");
        public Dictionary<string, JsonObject> Sprites { get; } = records(scene, "sprites");
        public Dictionary<string, JsonObject> Lights { get; } = records(scene, "lights");

        public JsonObject ToJson()
        {
            JsonObject result = new();
            foreach ((string key, JsonNode? value) in Fields)
                result[key] = value?.DeepClone();
            result["regions"] = new JsonArray(Regions.Values.Select(value => value.DeepClone()).ToArray());
            result["sprites"] = new JsonArray(Sprites.Values.Select(value => value.DeepClone()).ToArray());
            result["lights"] = new JsonArray(Lights.Values.Select(value => value.DeepClone()).ToArray());
            return result;
        }
    }

    public static JsonObject Create(Snapshot previous, Snapshot current)
    {
        JsonObject fields = [];
        foreach (KeyValuePair<string, JsonNode?> field in current.Fields)
            if (!previous.Fields.TryGetValue(field.Key, out JsonNode? old) || !equal(old, field.Value))
                fields[field.Key] = field.Value?.DeepClone();
        Dictionary<string, JsonObject> oldRegions = previous.Regions;
        Dictionary<string, JsonObject> newRegions = current.Regions;
        JsonArray changed = [];
        foreach (KeyValuePair<string, JsonObject> region in newRegions)
            if (!oldRegions.TryGetValue(region.Key, out JsonObject? old) || !equal(old, region.Value))
                changed.Add(region.Value.DeepClone());
        JsonArray removed = [];
        foreach (string id in oldRegions.Keys)
            if (!newRegions.ContainsKey(id))
                removed.Add(id);
        JsonArray order = [];
        foreach (string id in newRegions.Keys)
            order.Add(id);
        return new JsonObject
        {
            ["fields"] = fields, ["regions"] = changed, ["removedRegions"] = removed, ["regionOrder"] = order,
            ["sprites"] = changes(previous.Sprites, current.Sprites),
            ["lights"] = changes(previous.Lights, current.Lights),
        };
    }

    private static bool equal(JsonNode? left, JsonNode? right) => ReferenceEquals(left, right) || JsonNode.DeepEquals(left, right);

    private static JsonObject changes(Dictionary<string, JsonObject> previous, Dictionary<string, JsonObject> current)
    {
        JsonArray upsert = [];
        JsonArray removed = [];
        JsonArray order = [];
        foreach ((string id, JsonObject record) in current)
        {
            order.Add(id);
            if (!previous.TryGetValue(id, out JsonObject? old) || !equal(old, record))
                upsert.Add(record.DeepClone());
        }
        foreach (string id in previous.Keys)
            if (!current.ContainsKey(id))
                removed.Add(id);
        return new JsonObject { ["upsert"] = upsert, ["removed"] = removed, ["order"] = order };
    }

    private static Dictionary<string, JsonObject> records(JsonObject scene, string name)
        => (scene[name] as JsonArray ?? []).OfType<JsonObject>()
            .ToDictionary(region => region["id"]!.GetValue<string>(), StringComparer.Ordinal);
}
