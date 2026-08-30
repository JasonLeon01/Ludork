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
    private void loadMapsAndWorldMaps()
    {
        string mapsRoot = Path.Combine(ProjectPath, "Data", "Maps");
        if (!Directory.Exists(mapsRoot))
            return;
        loadMapCatalogCache();
        foreach (string path in Directory.EnumerateFiles(mapsRoot, "*.json", SearchOption.TopDirectoryOnly)
                     .OrderBy(value => value, StringComparer.Ordinal))
        {
            if (string.Equals(Path.GetFileName(path), "_world.json", StringComparison.OrdinalIgnoreCase))
            {
                addInvalidLoadPath(path);
                continue;
            }
            JsonObject? map = readMapFile(path, false);
            if (map is null)
            {
                addInvalidLoadPath(path);
                continue;
            }
            string key = Path.GetFileNameWithoutExtension(path);
            sections["Maps"].Data[key] = map;
            mapLoadedBytes[key] = new FileInfo(path).Length;
            touchMap(key);
            setMapCatalogEntry(createMapCatalogEntry(
                key,
                MapCatalogEntryKind.StandaloneMap,
                null,
                map));
        }
        foreach (string directory in Directory.EnumerateDirectories(mapsRoot, "*", SearchOption.TopDirectoryOnly)
                     .OrderBy(value => value, StringComparer.Ordinal))
        {
            loadWorldDirectory(directory);
        }
        saveMapCatalogCache();
    }

    private void loadWorldDirectory(string directory)
    {
        string worldKey = Path.GetFileName(directory);
        string[] directJsonFiles = Directory.EnumerateFiles(directory, "*", SearchOption.TopDirectoryOnly)
            .Where(path => string.Equals(
                Path.GetExtension(path),
                ".json",
                StringComparison.OrdinalIgnoreCase))
            .OrderBy(value => value, StringComparer.Ordinal)
            .ToArray();
        string[] manifestPaths = directJsonFiles
            .Where(path => string.Equals(
                Path.GetFileName(path),
                "_world.json",
                StringComparison.OrdinalIgnoreCase))
            .ToArray();
        string manifestPath = manifestPaths.FirstOrDefault()
            ?? Path.Combine(directory, "_world.json");
        string[] nestedDirectories = Directory.EnumerateDirectories(
            directory,
            "*",
            SearchOption.AllDirectories).ToArray();
        bool manifestValid = manifestPaths.Length == 1
            && string.Equals(
                Path.GetFileName(manifestPath),
                "_world.json",
                StringComparison.Ordinal);
        if (nestedDirectories.Length != 0 || !manifestValid)
        {
            foreach (string nested in nestedDirectories)
                addInvalidLoadPath(nested);
            if (!manifestValid)
            {
                foreach (string invalidManifest in manifestPaths)
                    addInvalidLoadPath(invalidManifest);
                if (manifestPaths.Length == 0)
                    addInvalidLoadPath(directory);
            }
            return;
        }
        if (!File.Exists(manifestPath)
            || sections["Maps"].Data.ContainsKey(worldKey)
            || tryGetMapCatalogEntry(worldKey, out _))
        {
            addInvalidLoadPath(File.Exists(manifestPath) ? manifestPath : directory);
            return;
        }
        Dictionary<string, MapCatalogEntry> children = new(StringComparer.Ordinal);
        bool childrenValid = true;
        foreach (string path in directJsonFiles
                     .Where(path => !string.Equals(
                         Path.GetFileName(path),
                         "_world.json",
                         StringComparison.OrdinalIgnoreCase))
                     .OrderBy(value => value, StringComparer.Ordinal))
        {
            MapCatalogEntry? child = readMapCatalogEntry(path, worldKey);
            if (child is null)
            {
                addInvalidLoadPath(path);
                childrenValid = false;
                continue;
            }
            children[child.Key] = child;
        }
        JsonObject? manifest = readWorldMapFile(manifestPath);
        if (manifest is null || !childrenValid)
        {
            addInvalidLoadPath(manifestPath);
            return;
        }
        WorldMapValidationResult validation = worldMapValidation.Validate(worldKey, manifest, children);
        if (!validation.IsValid)
        {
            addInvalidLoadPath(manifestPath);
            return;
        }
        sections["WorldMaps"].Data[worldKey] = manifest;
        setMapCatalogEntry(new MapCatalogEntry(
            worldKey,
            manifest["worldName"]!.GetValue<string>(),
            MapCatalogEntryKind.WorldMap,
            null,
            manifest["width"]!.GetValue<int>(),
            manifest["height"]!.GetValue<int>(),
            validation.LayerOrder,
            []));
        foreach (MapCatalogEntry child in children.Values)
            setMapCatalogEntry(child);
    }

    private static JsonObject? readWorldMapFile(string path)
    {
        try
        {
            return JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
        }
        catch (Exception exception) when (
            exception is JsonException
            or IOException
            or UnauthorizedAccessException)
        {
            return null;
        }
    }

    private IReadOnlyDictionary<string, MapCatalogEntry> getWorldChildCatalog(string worldKey)
    {
        return getMapCatalogEntries()
            .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap
                && string.Equals(entry.WorldKey, worldKey, StringComparison.Ordinal))
            .ToDictionary(entry => entry.Key, entry => entry, StringComparer.Ordinal);
    }

    private void setWorldCatalogLayerOrder(string worldKey, IReadOnlyList<string> layerOrder)
    {
        string catalogKey = getMapCatalogDataKey(MapCatalogEntryKind.WorldMap, worldKey);
        if (!sections["MapCatalog"].Data.TryGetValue(catalogKey, out JsonObject? data))
            return;
        JsonArray values = new JsonArray();
        foreach (string name in layerOrder)
            values.Add(name);
        data["layerOrder"] = values;
    }

    private static JsonObject createWorldMapData(
        WorldMapInfo info,
        IReadOnlyList<string> layerOrder,
        IReadOnlyList<WorldMapPlacement> placements)
    {
        JsonArray layerOrderData = new JsonArray();
        foreach (string name in layerOrder)
            layerOrderData.Add(name);
        JsonArray placementData = createPlacementData(placements);
        string fog = info.Fog.Trim();
        return new JsonObject
        {
            ["type"] = "worldMap",
            ["worldName"] = info.WorldName.Trim(),
            ["width"] = info.Width,
            ["height"] = info.Height,
            ["fog"] = fog,
            ["fogPower"] = fog.Length == 0 ? 0 : info.FogPower,
            ["fogOx"] = fog.Length == 0 ? 0.0 : info.FogOx,
            ["fogOy"] = fog.Length == 0 ? 0.0 : info.FogOy,
            ["fogDistort"] = fog.Length == 0 ? 0 : info.FogDistort,
            ["layerOrder"] = layerOrderData,
            ["placements"] = placementData,
        };
    }

    private static JsonObject replaceWorldComposition(
        JsonObject current,
        IReadOnlyList<string> layerOrder,
        IReadOnlyList<WorldMapPlacement> placements)
    {
        JsonObject result = (JsonObject)current.DeepClone();
        JsonArray layerOrderData = new JsonArray();
        foreach (string name in layerOrder)
            layerOrderData.Add(name);
        result["layerOrder"] = layerOrderData;
        result["placements"] = createPlacementData(placements);
        return result;
    }

    private static JsonArray createPlacementData(IReadOnlyList<WorldMapPlacement> placements)
    {
        JsonArray result = new JsonArray();
        foreach (WorldMapPlacement placement in placements)
        {
            result.Add(new JsonObject
            {
                ["map"] = placement.Map,
                ["rect"] = new JsonArray(
                    placement.Rect.X,
                    placement.Rect.Y,
                    placement.Rect.Width,
                    placement.Rect.Height),
            });
        }
        return result;
    }

    private bool tryGetWorldPlacement(
        string worldKey,
        string childMapKey,
        out WorldMapPlacement placement)
    {
        string childFile = Path.GetFileName(normaliseMapKey(childMapKey)) + ".json";
        WorldMapValidationResult validation = ValidateWorldMap(worldKey);
        WorldMapPlacement? found = validation.Placements.FirstOrDefault(item =>
            string.Equals(item.Map, childFile, StringComparison.Ordinal));
        placement = found!;
        return validation.IsValid && found is not null;
    }

    private static string formatWorldMapValidation(WorldMapValidationResult validation)
    {
        return string.Join(Environment.NewLine, validation.Issues.Select(issue => issue.Message));
    }

    private static bool isValidWorldKey(string key)
    {
        return isValidMapChildName(key)
            && !string.Equals(key, "_world", StringComparison.OrdinalIgnoreCase);
    }

    private static bool isValidMapChildName(string key)
    {
        return !string.IsNullOrWhiteSpace(key)
            && !key.Contains('/')
            && !key.Contains('\\')
            && key is not "." and not ".."
            && !string.Equals(key, "_world", StringComparison.OrdinalIgnoreCase)
            && key.IndexOfAny(Path.GetInvalidFileNameChars()) < 0;
    }

    private static string normalizeWorldKey(string value)
    {
        string key = normalizeDataKey(value);
        if (key.EndsWith("/_world.json", StringComparison.OrdinalIgnoreCase))
            key = key[..^"/_world.json".Length];
        else if (key.EndsWith("/_world", StringComparison.OrdinalIgnoreCase))
            key = key[..^"/_world".Length];
        return key.Trim('/');
    }

    private static string normalizeMapRuntimePath(string value)
    {
        string path = normalizeDataKey(value);
        const string marker = "Data/Maps/";
        int markerIndex = path.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
        return markerIndex >= 0 ? path[(markerIndex + marker.Length)..] : path;
    }

    private string getMapDataPath(string key)
    {
        return Path.Combine(
            ProjectPath,
            "Data",
            "Maps",
            normaliseMapKey(key).Replace('/', Path.DirectorySeparatorChar) + ".json");
    }

    private string getReadableMapDataPath(string key)
    {
        key = normaliseMapKey(key);
        string path = getMapDataPath(key);
        if (File.Exists(path)
            || !TryGetWorldForMap(key, out string worldKey)
            || !pendingWorldDirectoryMoves.TryGetValue(worldKey, out string? sourceDirectory))
        {
            return path;
        }
        string suffix = key[(worldKey.Length + 1)..];
        return Path.Combine(sourceDirectory, suffix + ".json");
    }

    private string getWorldDirectory(string worldKey)
    {
        return Path.Combine(ProjectPath, "Data", "Maps", normalizeWorldKey(worldKey));
    }

    private string getCopyMapStem(string worldKey, string sourceStem)
    {
        string baseStem = sourceStem + " (copy)";
        string stem = baseStem;
        for (int index = 1; containsMapKey(worldKey + "/" + stem); index += 1)
            stem = $"{baseStem} ({index})";
        return stem;
    }

    private void rewriteCopiedActorTags(
        JsonObject map,
        string worldKey,
        string oldStem,
        string newStem)
    {
        Dictionary<string, string> replacements = new(StringComparer.Ordinal);
        HashSet<string> assignedTags = new(StringComparer.Ordinal);
        foreach (MapActorCollection collection in enumerateActorCollections(map))
        {
            foreach (JsonObject actor in collection.Actors.OfType<JsonObject>())
            {
                string sourceTag = getString(actor["tag"]) ?? string.Empty;
                string prefix = oldStem + ".";
                string body = sourceTag.StartsWith(prefix, StringComparison.Ordinal)
                    ? sourceTag[prefix.Length..]
                    : sourceTag;
                if (string.IsNullOrWhiteSpace(body))
                    body = getCopiedActorDefaultTagBody(actor);
                string replacement = MapTagService.MakeUnique(
                    newStem + "." + body,
                    candidate => assignedTags.Contains(candidate)
                        || WorldActorTagExists(worldKey, candidate));
                assignedTags.Add(replacement);
                replacements[sourceTag] = replacement;
                actor["tag"] = replacement;
            }
        }
        if (map["BPClassVarChanged"] is not JsonObject changed)
            return;
        JsonObject rewritten = new JsonObject();
        foreach (KeyValuePair<string, JsonNode?> item in changed)
        {
            string key = replacements.GetValueOrDefault(item.Key, item.Key);
            rewritten[key] = item.Value?.DeepClone();
        }
        if (rewritten.Count == 0)
            map.Remove("BPClassVarChanged");
        else
            map["BPClassVarChanged"] = rewritten;
    }

    private static string getCopiedActorDefaultTagBody(JsonObject actor)
    {
        string blueprintReference = getString(actor["bp"]) ?? string.Empty;
        int x = 0;
        int y = 0;
        if (actor["position"] is JsonArray position)
        {
            if (position.Count > 0
                && position[0] is JsonValue xValue
                && xValue.TryGetValue(out int actorX))
            {
                x = actorX;
            }
            if (position.Count > 1
                && position[1] is JsonValue yValue
                && yValue.TryGetValue(out int actorY))
            {
                y = actorY;
            }
        }
        return MapTagService.CreateDefaultBody(blueprintReference, x, y);
    }

    private bool hasDuplicateWorldActorTags(
        string worldKey,
        string candidateKey,
        JsonObject candidate)
    {
        HashSet<string> tags = new(StringComparer.Ordinal);
        foreach (MapActorCollection collection in enumerateActorCollections(candidate))
        {
            foreach (JsonObject actor in collection.Actors.OfType<JsonObject>())
            {
                string? tag = getString(actor["tag"]);
                if (string.IsNullOrWhiteSpace(tag))
                    continue;
                if (!tags.Add(tag) || WorldActorTagExists(worldKey, tag, candidateKey))
                    return true;
            }
        }
        return false;
    }

    private Dictionary<string, List<MapActorTagLocation>> getMapActorTagIndex(
        string mapKey,
        bool bypassCache)
    {
        mapKey = normaliseMapKey(mapKey);
        if (!bypassCache
            && mapActorTagIndexes.TryGetValue(mapKey, out Dictionary<string, List<MapActorTagLocation>>? cached))
        {
            return cached;
        }
        Dictionary<string, List<MapActorTagLocation>> result = new(StringComparer.Ordinal);
        JsonObject? map = ReadMapSnapshotWithoutCaching(mapKey);
        if (map is not null)
        {
            foreach (MapActorCollection collection in enumerateActorCollections(map))
            {
                for (int index = 0; index < collection.Actors.Count; index += 1)
                {
                    if (collection.Actors[index] is not JsonObject actor)
                        continue;
                    string? tag = getString(actor["tag"]);
                    if (string.IsNullOrWhiteSpace(tag))
                        continue;
                    if (!result.TryGetValue(tag, out List<MapActorTagLocation>? locations))
                    {
                        locations = [];
                        result[tag] = locations;
                    }
                    locations.Add(new MapActorTagLocation(collection.LocationKey, index));
                }
            }
        }
        if (!bypassCache)
            mapActorTagIndexes[mapKey] = result;
        return result;
    }

    private static bool addActorTags(JsonObject map, ISet<string> tags)
    {
        foreach (MapActorCollection collection in enumerateActorCollections(map))
        {
            foreach (JsonObject actor in collection.Actors.OfType<JsonObject>())
            {
                string? tag = getString(actor["tag"]);
                if (!string.IsNullOrWhiteSpace(tag) && !tags.Add(tag))
                    return false;
            }
        }
        return true;
    }

    private static IEnumerable<MapActorCollection> enumerateActorCollections(JsonObject map)
    {
        if (map["actors"] is JsonObject groups)
        {
            foreach (KeyValuePair<string, JsonNode?> group in groups)
            {
                if (group.Value is JsonArray actors)
                    yield return new MapActorCollection(group.Key, actors);
            }
        }
        else if (map["actors"] is JsonArray actors)
        {
            yield return new MapActorCollection(string.Empty, actors);
        }
        if (map["layers"] is not JsonObject layers)
            yield break;
        foreach (KeyValuePair<string, JsonNode?> entry in layers)
        {
            if (entry.Value is JsonObject layer && layer["actors"] is JsonArray layerActors)
                yield return new MapActorCollection("\0" + entry.Key, layerActors);
        }
    }
}
