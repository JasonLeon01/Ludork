using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    internal bool commitMapStructureChange(string mapKey, JsonObject current, JsonObject candidate)
    {
        mapKey = normaliseMapKey(mapKey);
        JsonObject? worldCandidate = null;
        IReadOnlyList<string>? worldLayerOrder = null;
        if (store.Worlds.TryGetWorldForMap(mapKey, out string worldKey)
            && store.Worlds.getWorldMap(worldKey) is JsonObject world)
        {
            WorldMapValidationResult validation = store.Worlds.ValidateWorldMap(worldKey);
            if (!validation.IsValid)
                return false;
            Dictionary<string, MapCatalogEntry> children = store.Worlds.getWorldChildCatalog(worldKey)
                .ToDictionary(item => item.Key, item => item.Value, StringComparer.Ordinal);
            children[mapKey] = createMapCatalogEntry(
                mapKey,
                MapCatalogEntryKind.WorldChildMap,
                worldKey,
                candidate);
            worldLayerOrder = store.Worlds.worldMapValidation.TryMergeLayerOrder(
                worldKey,
                validation.Placements,
                children);
            if (worldLayerOrder is null)
                return false;
            worldCandidate = WorldDataService.replaceWorldComposition(world, worldLayerOrder, validation.Placements);
            if (!store.Worlds.worldMapValidation.Validate(worldKey, worldCandidate, children).IsValid)
                return false;
        }
        return store.commitResourceChange("Maps", mapKey, mapKey, candidate, worldCandidate);
    }

    internal static void replaceJsonObject(JsonObject target, JsonObject source)
    {
        target.Clear();
        foreach (KeyValuePair<string, JsonNode?> item in source)
            target[item.Key] = item.Value?.DeepClone();
    }

    internal static JsonObject createMapData(MapInfo info, string tilesetKey)
    {
        return new JsonObject
        {
            ["mapName"] = string.IsNullOrWhiteSpace(info.MapName)
                ? LocaleService.Get("NEW_MAP_DEFAULT_NAME")
                : info.MapName.Trim(),
            ["width"] = info.Width,
            ["height"] = info.Height,
            ["ambientLight"] = normaliseAmbientLight(info.AmbientLight),
            ["bgm"] = info.Bgm.Trim(),
            ["bgmFilter"] = ProjectDataStore.cloneObject(info.BgmFilter),
            ["bgs"] = info.Bgs.Trim(),
            ["bgsFilter"] = ProjectDataStore.cloneObject(info.BgsFilter),
            ["fog"] = info.Fog.Trim(),
            ["fogPower"] = string.IsNullOrWhiteSpace(info.Fog) ? 0 : info.FogPower,
            ["fogOx"] = string.IsNullOrWhiteSpace(info.Fog) ? 0.0 : info.FogOx,
            ["fogOy"] = string.IsNullOrWhiteSpace(info.Fog) ? 0.0 : info.FogOy,
            ["fogDistort"] = string.IsNullOrWhiteSpace(info.Fog) ? 0 : info.FogDistort,
            ["panorama"] = info.Panorama.Trim(),
            ["layerOrder"] = new JsonArray("floor", "default"),
            ["layers"] = new JsonObject
            {
                ["floor"] = createEmptyLayer("floor", tilesetKey, info.Width, info.Height),
                ["default"] = createEmptyLayer("default", tilesetKey, info.Width, info.Height),
            },
            ["actors"] = new JsonObject
            {
                ["floor"] = new JsonArray(),
                ["default"] = new JsonArray(),
            },
        };
    }

}
