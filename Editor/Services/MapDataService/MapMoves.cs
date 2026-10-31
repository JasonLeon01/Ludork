using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    public WorldMapMutationResult MoveMaps(IReadOnlyList<string> keys, string? targetWorldKey)
    {
        string? target = targetWorldKey is null ? null : WorldDataService.normalizeWorldKey(targetWorldKey);
        if (target is not null && store.Worlds.getWorldMap(target) is null)
            return WorldMapMutationResult.Failed(string.Format(LocaleService.Get("MAP_MOVE_FAILED"), target));
        foreach (string key in keys.Select(normaliseMapKey))
        {
            WorldMapMutationResult result = moveMap(key, target);
            if (!result.Success)
                return result;
        }
        return WorldMapMutationResult.Succeeded;
    }

    private WorldMapMutationResult moveMap(string key, string? target)
    {
        if (getMap(key) is not JsonObject map)
            return WorldMapMutationResult.Failed(string.Format(LocaleService.Get("MAP_MOVE_FAILED"), key));
        bool childMap = store.Worlds.TryGetWorldForMap(key, out string sourceWorld);
        if (string.Equals(childMap ? sourceWorld : null, target, StringComparison.Ordinal))
            return WorldMapMutationResult.Succeeded;
        string stem = Path.GetFileName(key);
        string newKey = target is null ? stem : target + "/" + stem;
        if (containsMapKey(newKey) || !store.canCreateDocument("Maps", newKey)
            || target is null && (store.Worlds.WorldMapData.ContainsKey(newKey)
                || Directory.Exists(Path.ChangeExtension(getMapDataPath(newKey), null))))
            return WorldMapMutationResult.Failed(string.Format(LocaleService.Get("MAP_MOVE_NAME_CONFLICT"), stem));
        if (target is not null && store.Worlds.hasDuplicateWorldActorTags(target, newKey, map))
            return WorldMapMutationResult.Failed(string.Format(LocaleService.Get("MAP_MOVE_TAG_CONFLICT"), stem, target));
        JsonObject? worldCandidate = null;
        if (childMap)
        {
            worldCandidate = prepareWorldWithoutChild(sourceWorld, key);
            if (worldCandidate is null)
                return WorldMapMutationResult.Failed(string.Format(LocaleService.Get("MAP_MOVE_FAILED"), key));
        }
        HistoryMarker marker = new(LocaleService.Get("DOCUMENT_HISTORY_MAP_MOVE_BARRIER"), IsBarrier: true);
        return store.commitResourceChange("Maps", key, newKey, map, worldCandidate, sourceMarker: marker)
            ? WorldMapMutationResult.Succeeded
            : WorldMapMutationResult.Failed(string.Format(LocaleService.Get("MAP_MOVE_FAILED"), key));
    }

    private JsonObject? prepareWorldWithoutChild(string worldKey, string childKey)
    {
        WorldDataService worlds = store.Worlds;
        WorldMapValidationResult validation = worlds.ValidateWorldMap(worldKey);
        if (worlds.getWorldMap(worldKey) is not JsonObject world || !validation.IsValidForEditing)
            return null;
        string childFile = Path.GetFileName(childKey) + ".json";
        WorldMapPlacement[] placements = validation.Placements
            .Where(item => !string.Equals(item.Map, childFile, StringComparison.Ordinal)).ToArray();
        Dictionary<string, MapCatalogEntry> children = worlds.getWorldChildCatalog(worldKey)
            .Where(pair => !string.Equals(pair.Key, childKey, StringComparison.Ordinal))
            .ToDictionary(pair => pair.Key, pair => pair.Value, StringComparer.Ordinal);
        if (worlds.worldMapValidation.TryMergeLayerOrder(worldKey, placements, children) is not IReadOnlyList<string> layerOrder)
            return null;
        JsonObject result = WorldDataService.replaceWorldComposition(world, layerOrder, placements);
        return worlds.worldMapValidation.Validate(worldKey, result, children).IsValidForEditing ? result : null;
    }
}
