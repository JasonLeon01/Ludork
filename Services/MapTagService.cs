using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public static class MapTagService
{
    public static string CreateDefault(
        GameDataService gameData,
        string mapKey,
        string blueprintReference,
        int x,
        int y)
    {
        string candidate = CreateDefaultBody(blueprintReference, x, y);
        if (gameData.TryGetWorldForMap(mapKey, out _))
            candidate = $"{getMapStem(mapKey)}.{candidate}";
        return MakeUnique(gameData, mapKey, candidate);
    }

    internal static string CreateDefaultBody(
        string blueprintReference,
        int x,
        int y)
    {
        string prefix = blueprintReference
            .Replace("Data.Blueprints.", string.Empty, StringComparison.Ordinal)
            .Replace('.', '_');
        return $"{prefix}_default_{x}_{y}";
    }

    public static string MakeUnique(
        GameDataService gameData,
        string mapKey,
        string tag,
        string? ignoredLayerName = null,
        int? ignoredActorIndex = null)
    {
        return MakeUnique(
            tag,
            candidate => Exists(
                gameData,
                mapKey,
                candidate,
                ignoredLayerName,
                ignoredActorIndex));
    }

    internal static string MakeUnique(string tag, Func<string, bool> exists)
    {
        string baseTag = tag.Trim();
        if (baseTag.Length == 0)
            return string.Empty;
        string candidate = baseTag;
        int suffix = 2;
        while (exists(candidate))
        {
            candidate = $"{baseTag}_{suffix}";
            suffix += 1;
        }
        return candidate;
    }

    public static bool Exists(
        GameDataService gameData,
        string mapKey,
        string tag,
        string? ignoredLayerName = null,
        int? ignoredActorIndex = null)
    {
        if (gameData.TryGetWorldForMap(mapKey, out string worldKey))
        {
            return gameData.WorldActorTagExists(
                worldKey,
                tag,
                mapKey,
                ignoredLayerName,
                ignoredActorIndex ?? -1);
        }
        return gameData.getMap(mapKey) is JsonObject map
            && containsTag(map, tag, ignoredLayerName, ignoredActorIndex);
    }

    private static bool containsTag(
        JsonObject map,
        string tag,
        string? ignoredLayerName,
        int? ignoredActorIndex)
    {
        JsonObject groups = map["actors"] as JsonObject ?? new JsonObject();
        foreach (KeyValuePair<string, JsonNode?> entry in groups)
        {
            if (entry.Value is not JsonArray actors)
                continue;
            for (int index = 0; index < actors.Count; index += 1)
            {
                if (string.Equals(entry.Key, ignoredLayerName, StringComparison.Ordinal)
                    && index == ignoredActorIndex)
                {
                    continue;
                }
                if (actors[index] is JsonObject actor
                    && string.Equals(
                        actor["tag"]?.GetValue<string>(),
                        tag,
                        StringComparison.Ordinal))
                {
                    return true;
                }
            }
        }
        if (map["layers"] is not JsonObject layers)
            return false;
        foreach (JsonObject layer in layers.Select(entry => entry.Value).OfType<JsonObject>())
        {
            if (layer["actors"] is not JsonArray actors)
                continue;
            foreach (JsonObject actor in actors.OfType<JsonObject>())
            {
                if (string.Equals(actor["tag"]?.GetValue<string>(), tag, StringComparison.Ordinal))
                    return true;
            }
        }
        return false;
    }

    private static string getMapStem(string mapKey)
    {
        string normalized = mapKey.Replace('\\', '/').Trim('/');
        int separator = normalized.LastIndexOf('/');
        return separator < 0 ? normalized : normalized[(separator + 1)..];
    }
}
