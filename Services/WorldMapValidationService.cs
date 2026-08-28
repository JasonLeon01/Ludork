using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using Ludork.Models;

namespace Ludork.Services;

public sealed class WorldMapValidationService
{
    private static readonly HashSet<string> AllowedFields = new(StringComparer.Ordinal)
    {
        "type",
        "width",
        "height",
        "fog",
        "fogPower",
        "fogOx",
        "fogOy",
        "fogDistort",
        "layerOrder",
        "placements",
    };
    private static readonly HashSet<string> ForbiddenEnvironmentFields = new(StringComparer.Ordinal)
    {
        "ambientLight",
        "bgm",
        "bgmFilter",
        "bgs",
        "bgsFilter",
    };

    public WorldMapValidationResult Validate(
        string worldKey,
        JsonObject manifest,
        IReadOnlyDictionary<string, MapCatalogEntry> childMaps)
    {
        List<WorldMapValidationIssue> issues = [];
        List<WorldMapPlacement> placements = [];
        validateWorldKey(worldKey, issues);
        validateFields(manifest, issues);
        int width = readPositiveInt(manifest["width"], "width", issues);
        int height = readPositiveInt(manifest["height"], "height", issues);
        validateFog(manifest, issues);
        IReadOnlyList<string> storedLayerOrder = readLayerOrder(manifest["layerOrder"], issues);
        readPlacements(worldKey, manifest["placements"], childMaps, width, height, placements, issues);
        IReadOnlyList<string> mergedLayerOrder = mergeLayerOrder(worldKey, placements, childMaps, issues);
        if (!storedLayerOrder.SequenceEqual(mergedLayerOrder, StringComparer.Ordinal))
        {
            issues.Add(new WorldMapValidationIssue(
                "layerOrderMismatch",
                "layerOrder must equal the stable topological merge of placed child maps."));
        }
        return new WorldMapValidationResult(issues, mergedLayerOrder, placements);
    }

    public IReadOnlyList<string>? TryMergeLayerOrder(
        string worldKey,
        IReadOnlyList<WorldMapPlacement> placements,
        IReadOnlyDictionary<string, MapCatalogEntry> childMaps)
    {
        List<WorldMapValidationIssue> issues = [];
        IReadOnlyList<string> result = mergeLayerOrder(worldKey, placements, childMaps, issues);
        return issues.Count == 0 ? result : null;
    }

    private static void validateWorldKey(string worldKey, ICollection<WorldMapValidationIssue> issues)
    {
        if (string.IsNullOrWhiteSpace(worldKey)
            || worldKey.Contains('/')
            || worldKey.Contains('\\')
            || worldKey is "." or ".."
            || string.Equals(worldKey, "_world", StringComparison.OrdinalIgnoreCase))
        {
            issues.Add(new WorldMapValidationIssue(
                "invalidWorldKey",
                "A world key must be one direct directory name under Data/Maps."));
        }
    }

    private static void validateFields(JsonObject manifest, ICollection<WorldMapValidationIssue> issues)
    {
        foreach (string field in manifest.Select(item => item.Key))
        {
            if (ForbiddenEnvironmentFields.Contains(field))
            {
                issues.Add(new WorldMapValidationIssue(
                    "forbiddenEnvironmentField",
                    $"World manifests must not contain {field}."));
            }
            else if (!AllowedFields.Contains(field))
            {
                issues.Add(new WorldMapValidationIssue(
                    "unknownField",
                    $"World manifests must not contain the unknown field {field}."));
            }
        }
        if (!string.Equals(readString(manifest["type"]), "worldMap", StringComparison.Ordinal))
        {
            issues.Add(new WorldMapValidationIssue(
                "invalidType",
                "World manifests must declare type as worldMap."));
        }
    }

    private static void validateFog(JsonObject manifest, ICollection<WorldMapValidationIssue> issues)
    {
        if (manifest["fog"] is not JsonValue fog || !fog.TryGetValue(out string? _))
            issues.Add(new WorldMapValidationIssue("invalidFog", "fog must be a string."));
        readInt(manifest["fogPower"], "fogPower", issues);
        readFiniteDouble(manifest["fogOx"], "fogOx", issues);
        readFiniteDouble(manifest["fogOy"], "fogOy", issues);
        readInt(manifest["fogDistort"], "fogDistort", issues);
    }

    private static IReadOnlyList<string> readLayerOrder(
        JsonNode? value,
        ICollection<WorldMapValidationIssue> issues)
    {
        if (value is not JsonArray array)
        {
            issues.Add(new WorldMapValidationIssue("invalidLayerOrder", "layerOrder must be an array."));
            return [];
        }
        List<string> result = [];
        HashSet<string> names = new(StringComparer.Ordinal);
        foreach (JsonNode? item in array)
        {
            string? name = readString(item);
            if (string.IsNullOrWhiteSpace(name) || !names.Add(name))
            {
                issues.Add(new WorldMapValidationIssue(
                    "invalidLayerOrder",
                    "layerOrder must contain unique non-empty strings."));
                continue;
            }
            result.Add(name);
        }
        return result;
    }

    private static void readPlacements(
        string worldKey,
        JsonNode? value,
        IReadOnlyDictionary<string, MapCatalogEntry> childMaps,
        int worldWidth,
        int worldHeight,
        ICollection<WorldMapPlacement> placements,
        ICollection<WorldMapValidationIssue> issues)
    {
        if (value is not JsonArray array)
        {
            issues.Add(new WorldMapValidationIssue("invalidPlacements", "placements must be an array."));
            return;
        }
        HashSet<string> placedMaps = new(StringComparer.Ordinal);
        foreach (JsonNode? item in array)
        {
            if (item is not JsonObject placement
                || placement.Count != 2
                || !placement.ContainsKey("map")
                || !placement.ContainsKey("rect"))
            {
                issues.Add(new WorldMapValidationIssue(
                    "invalidPlacement",
                    "Each placement must contain only map and rect."));
                continue;
            }
            string? mapFile = readString(placement["map"]);
            if (!isDirectMapFileName(mapFile))
            {
                issues.Add(new WorldMapValidationIssue(
                    "invalidPlacementMap",
                    "A placement map must be a direct .json child file.",
                    mapFile));
                continue;
            }
            string resolvedMapFile = mapFile!;
            string childKey = worldKey + "/" + Path.GetFileNameWithoutExtension(resolvedMapFile);
            if (!childMaps.TryGetValue(childKey, out MapCatalogEntry? child))
            {
                issues.Add(new WorldMapValidationIssue(
                    "missingChildMap",
                    $"The placed child map does not exist: {mapFile}.",
                    mapFile));
                continue;
            }
            if (!placedMaps.Add(childKey))
            {
                issues.Add(new WorldMapValidationIssue(
                    "duplicatePlacement",
                    $"The child map is placed more than once: {mapFile}.",
                    mapFile));
                continue;
            }
            if (!tryReadRect(placement["rect"], out WorldMapRect rect))
            {
                issues.Add(new WorldMapValidationIssue(
                    "invalidPlacementRect",
                    $"The placement rect is invalid: {mapFile}.",
                    mapFile));
                continue;
            }
            if (rect.Width != child.Width || rect.Height != child.Height)
            {
                issues.Add(new WorldMapValidationIssue(
                    "childSizeMismatch",
                    $"The placement rect size does not match the child map: {mapFile}.",
                    mapFile));
            }
            if ((long)rect.X + rect.Width > worldWidth || (long)rect.Y + rect.Height > worldHeight)
            {
                issues.Add(new WorldMapValidationIssue(
                    "placementOutOfBounds",
                    $"The child map is outside the world bounds: {mapFile}.",
                    mapFile));
            }
            foreach (WorldMapPlacement existing in placements)
            {
                if (rect.Intersects(existing.Rect))
                {
                    issues.Add(new WorldMapValidationIssue(
                        "overlappingPlacement",
                        $"The child map overlaps {existing.Map}: {mapFile}.",
                        mapFile));
                }
            }
            placements.Add(new WorldMapPlacement(resolvedMapFile, rect));
        }
    }

    private static IReadOnlyList<string> mergeLayerOrder(
        string worldKey,
        IReadOnlyList<WorldMapPlacement> placements,
        IReadOnlyDictionary<string, MapCatalogEntry> childMaps,
        ICollection<WorldMapValidationIssue> issues)
    {
        Dictionary<string, HashSet<string>> outgoing = new(StringComparer.Ordinal);
        Dictionary<string, int> incoming = new(StringComparer.Ordinal);
        Dictionary<string, int> priority = new(StringComparer.Ordinal);
        int nextPriority = 0;
        foreach (WorldMapPlacement placement in placements)
        {
            string childKey = worldKey + "/" + Path.GetFileNameWithoutExtension(placement.Map);
            if (!childMaps.TryGetValue(childKey, out MapCatalogEntry? child))
                continue;
            HashSet<string> childNames = new(StringComparer.Ordinal);
            foreach (string name in child.LayerOrder)
            {
                if (string.IsNullOrWhiteSpace(name) || !childNames.Add(name))
                {
                    issues.Add(new WorldMapValidationIssue(
                        "invalidChildLayerOrder",
                        $"The child map has an invalid layerOrder: {placement.Map}.",
                        placement.Map));
                    continue;
                }
                if (!outgoing.ContainsKey(name))
                {
                    outgoing[name] = new HashSet<string>(StringComparer.Ordinal);
                    incoming[name] = 0;
                    priority[name] = nextPriority;
                    nextPriority += 1;
                }
            }
            for (int earlier = 0; earlier < child.LayerOrder.Count; earlier += 1)
            {
                for (int later = earlier + 1; later < child.LayerOrder.Count; later += 1)
                {
                    string from = child.LayerOrder[earlier];
                    string to = child.LayerOrder[later];
                    if (outgoing.TryGetValue(from, out HashSet<string>? targets)
                        && incoming.ContainsKey(to)
                        && targets.Add(to))
                    {
                        incoming[to] += 1;
                    }
                }
            }
        }
        List<string> result = [];
        while (result.Count < incoming.Count)
        {
            string? next = incoming
                .Where(item => item.Value == 0 && !result.Contains(item.Key, StringComparer.Ordinal))
                .OrderBy(item => priority[item.Key])
                .Select(item => item.Key)
                .FirstOrDefault();
            if (next is null)
            {
                issues.Add(new WorldMapValidationIssue(
                    "layerOrderCycle",
                    "Placed child maps contain conflicting layer orders."));
                return [];
            }
            result.Add(next);
            foreach (string target in outgoing[next])
                incoming[target] -= 1;
        }
        return result;
    }

    private static bool isDirectMapFileName(string? value)
    {
        return !string.IsNullOrWhiteSpace(value)
            && string.Equals(Path.GetExtension(value), ".json", StringComparison.OrdinalIgnoreCase)
            && string.Equals(Path.GetFileName(value), value, StringComparison.Ordinal)
            && !string.Equals(value, "_world.json", StringComparison.OrdinalIgnoreCase);
    }

    private static bool tryReadRect(JsonNode? value, out WorldMapRect rect)
    {
        rect = default;
        if (value is not JsonArray array || array.Count != 4
            || !tryReadInt(array[0], out int x)
            || !tryReadInt(array[1], out int y)
            || !tryReadInt(array[2], out int width)
            || !tryReadInt(array[3], out int height)
            || x < 0 || y < 0 || width <= 0 || height <= 0)
        {
            return false;
        }
        rect = new WorldMapRect(x, y, width, height);
        return true;
    }

    private static int readPositiveInt(
        JsonNode? value,
        string field,
        ICollection<WorldMapValidationIssue> issues)
    {
        if (!tryReadInt(value, out int result) || result is < 1 or > 32768)
        {
            issues.Add(new WorldMapValidationIssue(
                "invalid" + char.ToUpperInvariant(field[0]) + field[1..],
                $"{field} must be an integer from 1 to 32768."));
            return 0;
        }
        return result;
    }

    private static int readInt(
        JsonNode? value,
        string field,
        ICollection<WorldMapValidationIssue> issues)
    {
        if (!tryReadInt(value, out int result))
            issues.Add(new WorldMapValidationIssue("invalid" + field, $"{field} must be an integer."));
        return result;
    }

    private static double readFiniteDouble(
        JsonNode? value,
        string field,
        ICollection<WorldMapValidationIssue> issues)
    {
        if (value is not JsonValue scalar
            || !scalar.TryGetValue(out double result)
            || !double.IsFinite(result))
        {
            issues.Add(new WorldMapValidationIssue("invalid" + field, $"{field} must be a finite number."));
            return 0.0;
        }
        return result;
    }

    private static bool tryReadInt(JsonNode? value, out int result)
    {
        result = 0;
        return value is JsonValue scalar && scalar.TryGetValue(out result);
    }

    private static string? readString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? result) ? result : null;
    }
}
