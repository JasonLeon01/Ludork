using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapPreviewSceneBuilder
{
    private readonly ConditionalWeakTable<JsonObject, SceneActors> sceneActors = new();

    public static bool IsActorEdit(MapDataEditedEventArgs edit)
        => edit.Edits.All(value => value.Path[0] is "actors" or "BPClassVarChanged" or "runtimeInfo");

    public bool UpdateActors(JsonObject scene, string key, JsonObject map, MapDataEditedEventArgs edit)
    {
        if (!IsActorEdit(edit) || !sceneActors.TryGetValue(scene, out SceneActors? cache)
            || (scene["regions"] as JsonArray)?.OfType<JsonObject>().FirstOrDefault(region => region["id"]?.GetValue<string>() == key) is not JsonObject region)
            return false;
        int offsetX = JsonScalar.IntegerFromText(region["x"]);
        int offsetY = JsonScalar.IntegerFromText(region["y"]);
        List<string> order = [];
        HashSet<string>? affected = affectedActors(key, map, edit);
        using IDisposable batch = preview.BeginResolutionBatch();
        foreach ((string layer, JsonNode? group) in map["actors"] as JsonObject ?? [])
        {
            if (group is not JsonArray actors || map["layers"]?[layer]?["visible"]?.GetValue<bool>() == false)
                continue;
            foreach (JsonObject actor in actors.OfType<JsonObject>())
            {
                if (actor["position"] is not JsonArray { Count: >= 2 } position)
                    continue;
                string id = actorId(key, layer, actor);
                order.Add(id);
                if (affected is not null && !affected.Contains(id) && cache.Actors.ContainsKey(id))
                    continue;
                JsonNode? overrides = actorOverrides(map, actor);
                if (cache.Actors.TryGetValue(id, out CachedActor? old)
                    && JsonNode.DeepEquals(old.Overrides, overrides))
                {
                    if (JsonNode.DeepEquals(old.Source, actor))
                        continue;
                    if (old.Source.Count == actor.Count && actor.All(field => field.Key == "position"
                        || JsonNode.DeepEquals(old.Source[field.Key], field.Value)))
                    {
                        double x = (number(position[0]) + offsetX) * gameData.Configs.getCellSize();
                        double y = (number(position[1]) + offsetY) * gameData.Configs.getCellSize();
                        JsonArray previous = (JsonArray)old.Sprite["position"]!;
                        JsonObject sprite = (JsonObject)old.Sprite.DeepClone();
                        sprite["position"] = new JsonArray(x, y);
                        JsonObject? light = old.Light?.DeepClone() as JsonObject;
                        if (actor["visual"] is null && light?["position"] is JsonArray emitter)
                            light["position"] = new JsonArray(number(emitter[0]) + x - number(previous[0]), number(emitter[1]) + y - number(previous[1]));
                        cache.Actors[id] = new((JsonObject)actor.DeepClone(), overrides?.DeepClone(), sprite, light);
                        continue;
                    }
                }
                JsonArray sprites = [];
                JsonArray lights = [];
                appendActor(key, offsetX, offsetY, layer, map, actor, sprites, lights);
                cacheActor(cache, key, layer, map, actor, sprites, lights, false);
                sprites.Clear();
                lights.Clear();
            }
        }
        HashSet<string> retained = order.ToHashSet(StringComparer.Ordinal);
        if (cache.Order.TryGetValue(key, out List<string>? previousOrder))
            foreach (string id in previousOrder)
                if (!retained.Contains(id))
                    cache.Actors.Remove(id);
        cache.Order[key] = order;
        JsonArray outputSprites = (JsonArray)scene["sprites"]!;
        JsonArray outputLights = (JsonArray)scene["lights"]!;
        outputSprites.Clear();
        outputLights.Clear();
        foreach (JsonObject item in ((JsonArray)scene["regions"]!).OfType<JsonObject>())
            if (cache.Order.TryGetValue(item["id"]!.GetValue<string>(), out List<string>? ids))
                foreach (string id in ids)
                {
                    CachedActor actor = cache.Actors[id];
                    outputSprites.Add(actor.Sprite);
                    if (actor.Light is not null)
                        outputLights.Add(actor.Light);
                }
        return true;
    }

    private static HashSet<string>? affectedActors(string key, JsonObject map, MapDataEditedEventArgs edit)
    {
        HashSet<string> affected = new(StringComparer.Ordinal);
        foreach (JsonDataEdit change in edit.Edits)
        {
            if (change.Path[0] is "runtimeInfo")
                continue;
            if (change.Path[0] is "actors" && change.Kind == JsonDataEdit.Operation.Set && change.Path.Count >= 4
                && change.Path[1] is string layer && change.Path[2] is int index && change.Path[3] is not "tag"
                && map["actors"]?[layer] is JsonArray actors && index >= 0 && index < actors.Count && actors[index] is JsonObject actor)
            {
                affected.Add(actorId(key, layer, actor));
                continue;
            }
            if (change.Path[0] is "BPClassVarChanged" && change.Path.Count >= 2 && change.Path[1] is string tag)
            {
                foreach ((string group, JsonNode? values) in map["actors"] as JsonObject ?? [])
                    foreach (JsonObject item in (values as JsonArray ?? []).OfType<JsonObject>())
                        if (item["tag"]?.GetValue<string>() == tag)
                            affected.Add(actorId(key, group, item));
                continue;
            }
            return null;
        }
        return affected;
    }

    private static string actorId(string region, string layer, JsonObject actor)
        => region + "/" + layer + "/" + (actor["runtimeId"] ?? actor["tag"])?.ToString();

    private static JsonNode? actorOverrides(JsonObject map, JsonObject actor)
        => map["BPClassVarChanged"]?[actor["tag"]?.GetValue<string>() ?? string.Empty];

    private static void cacheActor(SceneActors cache, string key, string layer, JsonObject map, JsonObject actor,
        JsonArray sprites, JsonArray lights, bool addOrder = true)
    {
        string id = actorId(key, layer, actor);
        if (sprites.LastOrDefault() is not JsonObject sprite || sprite["id"]?.GetValue<string>() != id)
            return;
        JsonObject? light = lights.LastOrDefault() as JsonObject;
        if (light?["ownerId"]?.GetValue<string>() != id)
            light = null;
        cache.Actors[id] = new((JsonObject)actor.DeepClone(), actorOverrides(map, actor)?.DeepClone(), sprite, light);
        if (addOrder)
        {
            if (!cache.Order.TryGetValue(key, out List<string>? order))
                cache.Order.Add(key, order = []);
            order.Add(id);
        }
    }

    private sealed class SceneActors
    {
        public Dictionary<string, CachedActor> Actors { get; } = new(StringComparer.Ordinal);
        public Dictionary<string, List<string>> Order { get; } = new(StringComparer.Ordinal);
    }

    private sealed record CachedActor(JsonObject Source, JsonNode? Overrides, JsonObject Sprite, JsonObject? Light);
}
