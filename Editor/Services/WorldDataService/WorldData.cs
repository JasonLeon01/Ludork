using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class WorldDataService
{
    internal void loadMapsAndWorldMaps()
    {
        string mapsRoot = Path.Combine(store.ProjectPath, "Data", "Maps");
        if (!Directory.Exists(mapsRoot))
            return;
        store.Maps.loadMapCatalogCache();
        string[] paths = Directory.EnumerateFiles(mapsRoot, "*.json", SearchOption.TopDirectoryOnly)
            .Where(path => !MapOrderService.IsOrderFile(path)).Order(StringComparer.Ordinal).ToArray();
        (JsonObject? Map, long Bytes)[] standalone = store.readLoadFiles(paths, path => path, (path, file) =>
        {
            if (string.Equals(Path.GetFileName(path), "_world.json", StringComparison.OrdinalIgnoreCase))
                return (null, 0L);
            try
            {
                byte[] content = file.ReadContent();
                return (MapDataService.decodeMapFile(ProjectLoadFile.ParseObject(content), false), content.LongLength);
            }
            catch (Exception exception) when (exception is JsonException or IOException or UnauthorizedAccessException)
            {
                return (null, 0L);
            }
        });
        Dictionary<string, string> blueprintErrors = store.getLoadedMapBlueprintErrors(paths.Select((path, index) =>
            (Path.GetFileNameWithoutExtension(path), standalone[index].Map)));
        for (int index = 0; index < paths.Length; index++)
        {
            string path = paths[index];
            string key = Path.GetFileNameWithoutExtension(path);
            if (blueprintErrors.TryGetValue(key, out string? error))
                store.rejectDataFile(path, error);
            else if (standalone[index].Map is JsonObject map)
                store.Maps.LoadStandaloneMap(key, map, standalone[index].Bytes);
            else
                store.addInvalidLoadPath(path);
        }
        WorldDirectory[] worlds = Directory.EnumerateDirectories(mapsRoot, "*", SearchOption.TopDirectoryOnly)
            .Order(StringComparer.Ordinal).Select(inspectWorldDirectory).ToArray();
        WorldDirectory[] valid = worlds.Where(world => world.InvalidPaths.Count == 0).ToArray();
        JsonObject?[] manifests = store.readLoadFiles(valid, world => world.ManifestPath, (_, file) => readWorldMapFile(file));
        (string Key, string Path)[] files = valid.SelectMany(world => world.Children.Select(path => (world.Key, path))).ToArray();
        MapDataService.WorldChildCatalogRead[] children = store.readLoadFiles(files,
            input => input.Path, (input, file) => store.Maps.readMapCatalogFile(file, input.Key));
        Dictionary<string, string> worldBlueprintErrors = store.getLoadedMapBlueprintErrors(files.Select((file, index) =>
            (file.Key + "/" + Path.GetFileNameWithoutExtension(file.Path), children[index].BlueprintData)));
        int manifestIndex = 0;
        int childIndex = 0;
        foreach (WorldDirectory world in worlds)
        {
            if (world.InvalidPaths.Count != 0)
            {
                foreach (string path in world.InvalidPaths)
                    store.addInvalidLoadPath(path);
                continue;
            }
            publishWorld(world, manifests[manifestIndex++], children.AsSpan(childIndex, world.Children.Length), worldBlueprintErrors);
            childIndex += world.Children.Length;
        }
        store.Maps.saveMapCatalogCache();
    }

    private sealed record WorldDirectory(string Key, string ManifestPath, string[] Children, IReadOnlyList<string> InvalidPaths);

    private WorldDirectory inspectWorldDirectory(string directory)
    {
        store.reportDataRead(directory);
        string worldKey = Path.GetFileName(directory);
        string[] directJsonFiles = Directory.EnumerateFiles(directory, "*", SearchOption.TopDirectoryOnly)
            .Where(path => string.Equals(
                Path.GetExtension(path),
                ".json",
                StringComparison.OrdinalIgnoreCase)
                && !MapOrderService.IsOrderFile(path))
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
        List<string> invalid = new(nestedDirectories);
        if (!manifestValid)
            invalid.AddRange(manifestPaths.Length == 0 ? [directory] : manifestPaths);
        else if (invalid.Count == 0 && (!File.Exists(manifestPath) || store.Maps.containsMapKey(worldKey)
            || store.Maps.tryGetMapCatalogEntry(worldKey, out _)))
            invalid.Add(File.Exists(manifestPath) ? manifestPath : directory);
        return new(worldKey, manifestPath, directJsonFiles.Where(path => path != manifestPath).ToArray(), invalid);
    }

    private void publishWorld(WorldDirectory world, JsonObject? manifest, ReadOnlySpan<MapDataService.WorldChildCatalogRead> prepared,
        IReadOnlyDictionary<string, string> blueprintErrors)
    {
        string worldKey = world.Key;
        string manifestPath = world.ManifestPath;
        Dictionary<string, MapCatalogEntry> children = new(StringComparer.Ordinal);
        bool childrenValid = true;
        for (int index = 0; index < prepared.Length; index++)
        {
            string path = world.Children[index];
            (MapCatalogEntry? child, _, string? fingerprint, string? error) = prepared[index];
            string key = worldKey + "/" + Path.GetFileNameWithoutExtension(path);
            error ??= blueprintErrors.GetValueOrDefault(key);
            if (child is not null && error is null)
            {
                children[child.Key] = child;
                store.Maps.rememberMapCatalogFile(path, child, fingerprint);
            }
            else
            {
                if (error is not null)
                    store.rejectDataFile(path, error);
                else
                    store.addInvalidLoadPath(path);
                childrenValid = false;
            }
        }
        if (manifest is null || !childrenValid)
        {
            store.addInvalidLoadPath(manifestPath);
            return;
        }
        WorldMapValidationResult validation = worldMapValidation.Validate(worldKey, manifest, children);
        if (!validation.IsValid)
        {
            store.addInvalidLoadPath(manifestPath);
            return;
        }
        worldDocuments[worldKey] = manifest;
        store.Maps.setMapCatalogEntry(new MapCatalogEntry(
            worldKey,
            manifest["worldName"]!.GetValue<string>(),
            MapCatalogEntryKind.WorldMap,
            null,
            manifest["width"]!.GetValue<int>(),
            manifest["height"]!.GetValue<int>(),
            validation.LayerOrder,
            []));
        foreach (MapCatalogEntry child in children.Values)
            store.Maps.setMapCatalogEntry(child);
    }

    private static JsonObject? readWorldMapFile(ProjectLoadFile file)
    {
        try
        {
            return ProjectLoadFile.ParseObject(file.ReadContent());
        }
        catch (Exception exception) when (
            exception is JsonException
            or IOException
            or UnauthorizedAccessException)
        {
            return null;
        }
    }

    internal IReadOnlyDictionary<string, MapCatalogEntry> getWorldChildCatalog(string worldKey)
    {
        return store.Maps.getWorldCatalog(worldKey);
    }

    internal static JsonObject createWorldMapData(
        WorldMapInfo info,
        IReadOnlyList<string> layerOrder,
        IReadOnlyList<WorldMapPlacement> placements)
    {
        JsonArray layerOrderData = new JsonArray();
        foreach (string name in layerOrder)
            layerOrderData.Add(name);
        JsonArray placementData = createPlacementData(placements);
        return new JsonObject
        {
            ["type"] = "worldMap",
            ["worldName"] = info.WorldName.Trim(),
            ["width"] = info.Width,
            ["height"] = info.Height,
            ["fog"] = info.Fog.DeepClone(),
            ["panorama"] = info.Panorama.Trim(),
            ["layerOrder"] = layerOrderData,
            ["placements"] = placementData,
        };
    }

    internal static JsonObject replaceWorldComposition(
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

    internal static JsonArray createPlacementData(IReadOnlyList<WorldMapPlacement> placements)
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

    internal bool tryGetWorldPlacement(
        string worldKey,
        string childMapKey,
        out WorldMapPlacement placement)
    {
        string childFile = Path.GetFileName(MapDataService.normaliseMapKey(childMapKey)) + ".json";
        WorldMapValidationResult validation = ValidateWorldMap(worldKey);
        WorldMapPlacement? found = validation.Placements.FirstOrDefault(item =>
            string.Equals(item.Map, childFile, StringComparison.Ordinal));
        placement = found!;
        return validation.IsValidForEditing && found is not null;
    }

    internal static string formatWorldMapValidation(WorldMapValidationResult validation)
    {
        return string.Join(Environment.NewLine, validation.Issues.Select(issue => issue.Message));
    }

    internal static bool isValidWorldKey(string key)
    {
        return isValidMapChildName(key)
            && !string.Equals(key, "_world", StringComparison.OrdinalIgnoreCase);
    }

    internal static bool isValidMapChildName(string key)
    {
        return !string.IsNullOrWhiteSpace(key)
            && !key.Contains('/')
            && !key.Contains('\\')
            && key is not "." and not ".."
            && !string.Equals(key, "_world", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(key, "_info", StringComparison.OrdinalIgnoreCase)
            && key.IndexOfAny(Path.GetInvalidFileNameChars()) < 0;
    }

    internal static string normalizeWorldKey(string value)
    {
        string key = ProjectDataStore.normalizeDataKey(value);
        if (key.EndsWith("/_world.json", StringComparison.OrdinalIgnoreCase))
            key = key[..^"/_world.json".Length];
        else if (key.EndsWith("/_world", StringComparison.OrdinalIgnoreCase))
            key = key[..^"/_world".Length];
        return key.Trim('/');
    }

    internal static string normalizeMapRuntimePath(string value)
    {
        string path = ProjectDataStore.normalizeDataKey(value);
        const string marker = "Data/Maps/";
        int markerIndex = path.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
        return markerIndex >= 0 ? path[(markerIndex + marker.Length)..] : path;
    }

    internal string getWorldDirectory(string worldKey)
    {
        return Path.Combine(store.ProjectPath, "Data", "Maps", normalizeWorldKey(worldKey));
    }

    internal string getCopyMapStem(string worldKey, string sourceStem)
    {
        string baseStem = sourceStem + " (copy)";
        string stem = baseStem;
        for (int index = 1; !store.canCreateDocument("Maps", worldKey + "/" + stem); index += 1)
            stem = $"{baseStem} ({index})";
        return stem;
    }

    internal void rewriteCopiedActorTags(
        JsonObject map,
        string worldKey,
        string oldStem,
        string newStem)
    {
        Dictionary<string, string> replacements = new(StringComparer.Ordinal);
        HashSet<string> assignedTags = new(StringComparer.Ordinal);
        foreach (MapDataService.MapActorCollection collection in enumerateActorCollections(map))
        {
            foreach (JsonObject actor in collection.Actors.OfType<JsonObject>())
            {
                string sourceTag = ProjectDataStore.getString(actor["tag"]) ?? string.Empty;
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

    internal static string getCopiedActorDefaultTagBody(JsonObject actor)
    {
        string blueprintReference = ProjectDataStore.getString(actor["bp"]) ?? string.Empty;
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

    internal bool hasDuplicateWorldActorTags(
        string worldKey,
        string candidateKey,
        JsonObject candidate)
    {
        HashSet<string> tags = new(StringComparer.Ordinal);
        foreach (MapDataService.MapActorCollection collection in enumerateActorCollections(candidate))
        {
            foreach (JsonObject actor in collection.Actors.OfType<JsonObject>())
            {
                string? tag = ProjectDataStore.getString(actor["tag"]);
                if (string.IsNullOrWhiteSpace(tag))
                    continue;
                if (!tags.Add(tag) || WorldActorTagExists(worldKey, tag, candidateKey))
                    return true;
            }
        }
        return false;
    }

    internal static bool addActorTags(JsonObject map, ISet<string> tags)
    {
        foreach (MapDataService.MapActorCollection collection in enumerateActorCollections(map))
        {
            foreach (JsonObject actor in collection.Actors.OfType<JsonObject>())
            {
                string? tag = ProjectDataStore.getString(actor["tag"]);
                if (!string.IsNullOrWhiteSpace(tag) && !tags.Add(tag))
                    return false;
            }
        }
        return true;
    }

    internal static IEnumerable<MapDataService.MapActorCollection> enumerateActorCollections(JsonObject map)
    {
        if (map["actors"] is JsonObject groups)
        {
            foreach (KeyValuePair<string, JsonNode?> group in groups)
            {
                if (group.Value is JsonArray actors)
                    yield return new MapDataService.MapActorCollection(group.Key, actors);
            }
        }
        else if (map["actors"] is JsonArray actors)
        {
            yield return new MapDataService.MapActorCollection(string.Empty, actors);
        }
        if (map["layers"] is not JsonObject layers)
            yield break;
        foreach (KeyValuePair<string, JsonNode?> entry in layers)
        {
            if (entry.Value is JsonObject layer && layer["actors"] is JsonArray layerActors)
                yield return new MapDataService.MapActorCollection("\0" + entry.Key, layerActors);
        }
    }

}
