using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    internal string getMapDataPath(string key)
    {
        return Path.Combine(
            store.ProjectPath,
            "Data",
            "Maps",
            normaliseMapKey(key).Replace('/', Path.DirectorySeparatorChar) + ".json");
    }

    internal string getReadableMapDataPath(string key)
    {
        key = normaliseMapKey(key);
        string path = getMapDataPath(key);
        if (File.Exists(path)
            || !store.Worlds.TryGetWorldForMap(key, out string worldKey)
            || !store.Worlds.TryGetPendingDirectoryMove(worldKey, out string? sourceDirectory))
        {
            return path;
        }
        string suffix = key[(worldKey.Length + 1)..];
        return Path.Combine(sourceDirectory, suffix + ".json");
    }

    internal Dictionary<string, List<MapActorTagLocation>> getMapActorTagIndex(
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
            foreach (MapActorCollection collection in WorldDataService.enumerateActorCollections(map))
            {
                for (int index = 0; index < collection.Actors.Count; index += 1)
                {
                    if (collection.Actors[index] is not JsonObject actor)
                        continue;
                    string? tag = ProjectDataStore.getString(actor["tag"]);
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

}
