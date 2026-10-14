using Avalonia;
using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapPreviewSceneBuilder(ProjectDataStore gameData, BlueprintPreviewService preview)
{
    public JsonObject Build(string key, JsonObject map)
    {
        if (map["renderOrigin"] is not JsonArray { Count: 2 } origin
            || map["renderWorldSize"] is not JsonArray { Count: 2 } size)
            return Build(map, [(key, 0, 0, 0, map)], false);
        JsonObject settings = (JsonObject)map.DeepClone();
        settings["width"] = size[0]?.DeepClone();
        settings["height"] = size[1]?.DeepClone();
        JsonObject region = (JsonObject)map.DeepClone();
        region["fog"] = map["regionFog"]?.DeepClone();
        return Build(settings, [(key, JsonScalar.IntegerFromText(origin[0]), JsonScalar.IntegerFromText(origin[1]), 0, region)]);
    }

    public JsonObject Build(JsonObject settings, IEnumerable<(string Key, int X, int Y, int Priority, JsonObject Map)> maps, bool world = true)
    {
        JsonArray regions = [];
        JsonArray sprites = [];
        JsonArray lights = [];
        SceneActors cached = new();
        using IDisposable batch = preview.BeginResolutionBatch();
        foreach ((string key, int x, int y, int priority, JsonObject map) in maps)
        {
            JsonObject terrain = new();
            foreach (string field in new[] { "width", "height", "layers", "fog", "ambientLight" })
                terrain[field] = map[field]?.DeepClone();
            regions.Add(new JsonObject { ["id"] = key, ["x"] = x, ["y"] = y, ["priority"] = priority, ["map"] = terrain });
            if (map["actors"] is not JsonObject groups)
                continue;
            foreach (KeyValuePair<string, JsonNode?> group in groups)
            {
                if (group.Value is not JsonArray actors || map["layers"]?[group.Key]?["visible"]?.GetValue<bool>() == false)
                    continue;
                foreach (JsonObject actor in actors.OfType<JsonObject>())
                {
                    appendActor(key, x, y, group.Key, map, actor, sprites, lights);
                    cacheActor(cached, key, group.Key, map, actor, sprites, lights);
                }
            }
        }
        JsonObject tilesets = [];
        foreach (KeyValuePair<string, JsonObject> entry in SnapshotJson.ToDictionary(gameData.Assets.TilesetData))
            tilesets[entry.Key] = entry.Value.DeepClone();
        JsonObject autoTiles = [];
        foreach (KeyValuePair<string, JsonObject> entry in SnapshotJson.ToDictionary(gameData.Assets.AutoTileData))
            autoTiles[entry.Key] = entry.Value.DeepClone();
        JsonObject scene = new()
        {
            ["world"] = world, ["width"] = settings["width"]?.DeepClone(), ["height"] = settings["height"]?.DeepClone(),
            ["cellSize"] = gameData.Configs.getCellSize(), ["layerOrder"] = settings["layerOrder"]?.DeepClone(),
            ["regions"] = regions, ["sprites"] = sprites, ["lights"] = lights,
            ["tilesets"] = tilesets, ["autoTiles"] = autoTiles,
            ["materials"] = gameData.Assets.CaptureMaterials(),
            ["ambientLight"] = settings["ambientLight"]?.DeepClone() ?? new JsonArray(255, 255, 255, 255),
            ["fog"] = settings["fog"]?.DeepClone(), ["panorama"] = settings["panorama"]?.DeepClone() ?? JsonValue.Create(string.Empty),
        };
        sceneActors.Add(scene, cached);
        return scene;
    }

    private void appendActor(string region, int offsetX, int offsetY, string layer, JsonObject map, JsonObject actor, JsonArray sprites, JsonArray lights)
    {
        if (actor["position"] is not JsonArray { Count: >= 2 } position)
            return;
        double cellSize = gameData.Configs.getCellSize();
        double x = (number(position[0]) + offsetX) * cellSize;
        double y = (number(position[1]) + offsetY) * cellSize;
        string id = region + "/" + layer + "/" + (actor["runtimeId"] ?? actor["tag"])?.ToString();
        JsonObject? runtimeVisual = actor["visual"] as JsonObject;
        ResolvedBlueprintClass? resolved = runtimeVisual is null ? preview.tryResolveMapActorClass(map, actor) : null;
        ActorVisualDescriptor? visual = resolved is null ? null
            : preview.tryResolveActorVisual(resolved, actor["bp"]?.GetValue<string>() ?? string.Empty);
        JsonObject sprite = new()
        {
            ["id"] = id, ["regionId"] = region, ["layer"] = layer, ["position"] = new JsonArray(x, y),
            ["texturePath"] = visual?.TexturePath ?? string.Empty,
            ["textureRect"] = visual is null ? new JsonArray(0, 0, (int)cellSize, (int)cellSize)
                : new JsonArray(visual.BaseTextureRect.X, visual.BaseTextureRect.Y, visual.BaseTextureRect.Width, visual.BaseTextureRect.Height),
            ["translation"] = vector(visual?.Translation ?? default), ["scale"] = vector(visual?.Scale ?? new Vector(1, 1)),
            ["origin"] = vector(visual?.Origin ?? default), ["rotation"] = visual?.Rotation ?? 0,
            ["visible"] = visual?.Visible ?? true, ["opacity"] = visual?.MapPreviewOpacity ?? 1,
            ["material"] = visual?.Material ?? string.Empty,
            ["materialParameters"] = visual?.MaterialParameters.DeepClone() ?? new JsonObject(),
            ["animated"] = visual?.Animated ?? false, ["switchInterval"] = visual?.SwitchInterval ?? 0.2,
            ["frameCount"] = visual?.FrameCount ?? 1,
        };
        if (resolved?.GetField("material")?.Value is JsonNode material)
            sprite["material"] = material.DeepClone();
        if (visual is null && resolved is not null)
        {
            foreach ((string source, string target) in new[] { ("defaultTranslation", "translation"), ("defaultScale", "scale"),
                ("defaultOrigin", "origin"), ("defaultRotation", "rotation"), ("visible", "visible") })
                if (resolved.GetField(source)?.Value is JsonNode value)
                    sprite[target] = value.DeepClone();
            sprite["opacity"] = sprite["visible"]?.GetValue<bool>() == false ? 0.25 : 1.0;
        }
        if (runtimeVisual is not null)
        {
            foreach (string name in new[] { "texturePath", "textureRect", "translation", "scale", "origin", "rotation", "visible", "material", "materialParameters", "colour" })
                if (runtimeVisual[name] is JsonNode value)
                    sprite[name] = value.DeepClone();
            sprite["opacity"] = runtimeVisual["visible"]?.GetValue<bool>() == false ? 0.25 : 1.0;
            sprite["animated"] = false;
        }
        sprites.Add(sprite);
        if (runtimeVisual is not null)
        {
            if (runtimeVisual["light"] is JsonObject runtimeLight && runtimeLight["position"] is JsonArray { Count: 2 } lightPosition)
            {
                JsonObject lightRecord = (JsonObject)runtimeLight.DeepClone();
                lightRecord["id"] = id + "/light";
                lightRecord["ownerId"] = id;
                lightRecord["position"] = new JsonArray(number(lightPosition[0]) + offsetX * cellSize, number(lightPosition[1]) + offsetY * cellSize);
                lights.Add(lightRecord);
            }
            return;
        }
        ActorLightDescriptor? light = resolved is null ? null : preview.tryResolveActorLight(resolved);
        if (light is null || map["layers"]?[layer]?["visible"]?.GetValue<bool>() == false)
            return;
        Point emitterPosition = light.GetWorldPosition(new Point(x, y));
        lights.Add(new JsonObject
        {
            ["id"] = id + "/light", ["ownerId"] = id,
            ["position"] = new JsonArray(emitterPosition.X, emitterPosition.Y),
            ["colour"] = new JsonArray(light.Colour.R, light.Colour.G, light.Colour.B, light.Colour.A),
            ["radius"] = light.Radius, ["intensity"] = 1.0,
        });
    }

    private static JsonArray vector(Vector value) => new(value.X, value.Y);
    private static double number(JsonNode? value) => JsonScalar.FiniteNumber(value);
}
