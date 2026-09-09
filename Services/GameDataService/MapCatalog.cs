using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using Ludork.Models;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    private void loadMapCatalogCache()
    {
        loadedMapCatalogCache.Clear();
        nextMapCatalogCache.Clear();
        string path = getMapCatalogCachePath();
        if (!File.Exists(path))
            return;
        try
        {
            if (JsonNode.Parse(File.ReadAllText(path)) is not JsonObject root
                || root["version"]?.GetValue<int?>() != 4
                || root["entries"] is not JsonObject entries)
            {
                return;
            }
            foreach (KeyValuePair<string, JsonNode?> entry in entries)
            {
                if (entry.Value is JsonObject value)
                    loadedMapCatalogCache[entry.Key] = (JsonObject)value.DeepClone();
            }
        }
        catch (Exception exception) when (
            exception is JsonException
            or IOException
            or UnauthorizedAccessException)
        {
        }
    }

    private void saveMapCatalogCache()
    {
        string path = getMapCatalogCachePath();
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            JsonObject entries = new JsonObject();
            foreach (KeyValuePair<string, JsonObject> entry in nextMapCatalogCache)
                entries[entry.Key] = entry.Value.DeepClone();
            JsonObject root = new()
            {
                ["version"] = 4,
                ["entries"] = entries,
            };
            File.WriteAllText(path, root.ToJsonString(WriteOptions) + Environment.NewLine);
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException)
        {
        }
    }

    private string getMapCatalogCachePath()
    {
        return Path.Combine(ProjectPath, "Temp", "EditorMapCatalog.json");
    }

    private JsonObject? readMapFile(string path, bool requireType)
    {
        try
        {
            if (JsonNode.Parse(File.ReadAllText(path)) is not JsonObject data)
                return null;
            string? type = getString(data["type"]);
            if (requireType && !string.Equals(type, "map", StringComparison.Ordinal)
                || !requireType && type is not null && !string.Equals(type, "map", StringComparison.Ordinal))
            {
                return null;
            }
            data.Remove("type");
            if (!tryReadMapShape(data, out _, out _, out _, requireType))
                return null;
            return data;
        }
        catch (Exception exception) when (
            exception is JsonException
            or IOException
            or UnauthorizedAccessException)
        {
            return null;
        }
    }

    private MapCatalogEntry? readMapCatalogEntry(string path, string worldKey)
    {
        try
        {
            string cacheKey = Path.GetRelativePath(ProjectPath, path).Replace('\\', '/');
            FileInfo file = new(path);
            if (loadedMapCatalogCache.TryGetValue(cacheKey, out JsonObject? cached)
                && cached["length"]?.GetValue<long?>() == file.Length
                && cached["lastWriteUtcTicks"]?.GetValue<long?>() == file.LastWriteTimeUtc.Ticks
                && cached["creationUtcTicks"]?.GetValue<long?>() == file.CreationTimeUtc.Ticks
                && cached["entry"] is JsonObject cachedEntry
                && readMapCatalogEntry(cachedEntry) is MapCatalogEntry parsedCache
                && parsedCache.Kind == MapCatalogEntryKind.WorldChildMap
                && string.Equals(parsedCache.WorldKey, worldKey, StringComparison.Ordinal))
            {
                nextMapCatalogCache[cacheKey] = (JsonObject)cached.DeepClone();
                return parsedCache;
            }
            string? type = null;
            string? mapName = null;
            int width = 0;
            int height = 0;
            List<string> layerOrder = [];
            List<string> actorTags = [];
            Stack<int> actorArrays = new();
            byte[] buffer = new byte[65536];
            int buffered = 0;
            JsonReaderState state = default;
            string? property = null;
            bool readingLayerOrder = false;
            bool awaitingRootActors = false;
            bool awaitingGroupedActorArray = false;
            bool awaitingLayers = false;
            bool awaitingLayer = false;
            bool awaitingLayerActors = false;
            bool awaitingActorTag = false;
            int? groupedActorsDepth = null;
            int? layersDepth = null;
            int? layerDepth = null;
            bool layersObjectSeen = false;
            string? pendingLayerName = null;
            string? currentLayerName = null;
            string? pendingGridName = null;
            int? gridDepth = null;
            int? gridRowDepth = null;
            int gridRowCells = 0;
            List<int> gridRows = [];
            Dictionary<string, MapCatalogLayerShape> layerShapes = new(StringComparer.Ordinal);
            bool rootStarted = false;
            bool rootEnded = false;
            using FileStream stream = new FileStream(
                path,
                FileMode.Open,
                FileAccess.Read,
                FileShare.Read,
                buffer.Length,
                FileOptions.SequentialScan);
            while (!rootEnded)
            {
                if (buffered == buffer.Length)
                    Array.Resize(ref buffer, buffer.Length * 2);
                int read = stream.Read(buffer, buffered, buffer.Length - buffered);
                bool finalBlock = read == 0;
                int available = buffered + read;
                Utf8JsonReader reader = new Utf8JsonReader(
                    new ReadOnlySpan<byte>(buffer, 0, available),
                    finalBlock,
                    state);
                while (reader.Read())
                {
                    if (reader.TokenType == JsonTokenType.PropertyName)
                    {
                        string? nestedProperty = reader.GetString();
                        if (reader.CurrentDepth == 1
                            && string.Equals(nestedProperty, "actors", StringComparison.Ordinal))
                        {
                            awaitingRootActors = true;
                        }
                        else if (reader.CurrentDepth == 1
                            && string.Equals(nestedProperty, "layers", StringComparison.Ordinal))
                        {
                            awaitingLayers = true;
                        }
                        else if (groupedActorsDepth is int actorGroupsDepth
                            && reader.CurrentDepth == actorGroupsDepth + 1)
                        {
                            awaitingGroupedActorArray = true;
                        }
                        else if (layersDepth is int mapLayersDepth
                            && reader.CurrentDepth == mapLayersDepth + 1)
                        {
                            awaitingLayer = true;
                            pendingLayerName = nestedProperty;
                        }
                        else if (layerDepth is int mapLayerDepth
                            && reader.CurrentDepth == mapLayerDepth + 1
                            && string.Equals(nestedProperty, "actors", StringComparison.Ordinal))
                        {
                            awaitingLayerActors = true;
                        }
                        else if (layerDepth is int gridLayerDepth
                            && reader.CurrentDepth == gridLayerDepth + 1
                            && nestedProperty is "tiles" or "autoTiles")
                        {
                            pendingGridName = nestedProperty;
                        }
                        if (actorArrays.Count != 0
                            && reader.CurrentDepth == actorArrays.Peek() + 2
                            && string.Equals(nestedProperty, "tag", StringComparison.Ordinal))
                        {
                            awaitingActorTag = true;
                        }
                    }
                    else
                    {
                        if (awaitingRootActors)
                        {
                            if (reader.TokenType == JsonTokenType.StartArray)
                                actorArrays.Push(reader.CurrentDepth);
                            else if (reader.TokenType == JsonTokenType.StartObject)
                                groupedActorsDepth = reader.CurrentDepth;
                            awaitingRootActors = false;
                        }
                        if (awaitingGroupedActorArray)
                        {
                            if (reader.TokenType == JsonTokenType.StartArray)
                                actorArrays.Push(reader.CurrentDepth);
                            awaitingGroupedActorArray = false;
                        }
                        if (awaitingLayers)
                        {
                            if (reader.TokenType != JsonTokenType.StartObject)
                                return null;
                            layersDepth = reader.CurrentDepth;
                            layersObjectSeen = true;
                            awaitingLayers = false;
                        }
                        if (awaitingLayer)
                        {
                            if (reader.TokenType != JsonTokenType.StartObject
                                || string.IsNullOrWhiteSpace(pendingLayerName)
                                || layerShapes.ContainsKey(pendingLayerName))
                            {
                                return null;
                            }
                            layerDepth = reader.CurrentDepth;
                            currentLayerName = pendingLayerName;
                            layerShapes[currentLayerName] = new MapCatalogLayerShape();
                            pendingLayerName = null;
                            awaitingLayer = false;
                        }
                        if (awaitingLayerActors)
                        {
                            if (reader.TokenType == JsonTokenType.StartArray)
                                actorArrays.Push(reader.CurrentDepth);
                            awaitingLayerActors = false;
                        }
                        if (awaitingActorTag)
                        {
                            if (reader.TokenType == JsonTokenType.String
                                && reader.GetString() is string actorTag
                                && !string.IsNullOrWhiteSpace(actorTag))
                            {
                                actorTags.Add(actorTag);
                            }
                            awaitingActorTag = false;
                        }
                        if (pendingGridName is not null)
                        {
                            if (reader.TokenType != JsonTokenType.StartArray || currentLayerName is null)
                                return null;
                            MapCatalogLayerShape shape = layerShapes[currentLayerName];
                            if (pendingGridName == "tiles" && shape.TilesRows is not null
                                || pendingGridName == "autoTiles" && shape.AutoTileRows is not null)
                            {
                                return null;
                            }
                            gridDepth = reader.CurrentDepth;
                            gridRowDepth = null;
                            gridRows = [];
                            string gridName = pendingGridName;
                            pendingGridName = null;
                            shape.ActiveGrid = gridName;
                            continue;
                        }
                        if (gridDepth is int activeGridDepth)
                        {
                            if (gridRowDepth is null
                                && reader.TokenType == JsonTokenType.StartArray
                                && reader.CurrentDepth == activeGridDepth + 1)
                            {
                                gridRowDepth = reader.CurrentDepth;
                                gridRowCells = 0;
                                continue;
                            }
                            if (gridRowDepth is int activeRowDepth
                                && reader.TokenType == JsonTokenType.EndArray
                                && reader.CurrentDepth == activeRowDepth)
                            {
                                gridRows.Add(gridRowCells);
                                gridRowDepth = null;
                                continue;
                            }
                            if (gridRowDepth is int cellRowDepth
                                && reader.CurrentDepth == cellRowDepth + 1
                                && reader.TokenType is JsonTokenType.Null
                                    or JsonTokenType.String
                                    or JsonTokenType.Number
                                    or JsonTokenType.True
                                    or JsonTokenType.False)
                            {
                                gridRowCells += 1;
                                continue;
                            }
                            if (gridRowDepth is null
                                && reader.TokenType == JsonTokenType.EndArray
                                && reader.CurrentDepth == activeGridDepth
                                && currentLayerName is not null)
                            {
                                MapCatalogLayerShape shape = layerShapes[currentLayerName];
                                if (shape.ActiveGrid == "tiles")
                                    shape.TilesRows = gridRows;
                                else if (shape.ActiveGrid == "autoTiles")
                                    shape.AutoTileRows = gridRows;
                                else
                                    return null;
                                shape.ActiveGrid = null;
                                gridDepth = null;
                                gridRows = [];
                                continue;
                            }
                            return null;
                        }
                    }
                    if (!rootStarted)
                    {
                        if (reader.TokenType != JsonTokenType.StartObject || reader.CurrentDepth != 0)
                            return null;
                        rootStarted = true;
                        continue;
                    }
                    if (reader.TokenType == JsonTokenType.EndObject && reader.CurrentDepth == 0)
                    {
                        rootEnded = true;
                        continue;
                    }
                    if (reader.TokenType == JsonTokenType.EndArray
                        && actorArrays.Count != 0
                        && actorArrays.Peek() == reader.CurrentDepth)
                    {
                        actorArrays.Pop();
                    }
                    if (reader.TokenType == JsonTokenType.EndObject
                        && layerDepth == reader.CurrentDepth)
                    {
                        if (currentLayerName is null
                            || layerShapes[currentLayerName].TilesRows is null
                            || layerShapes[currentLayerName].AutoTileRows is null)
                        {
                            return null;
                        }
                        layerDepth = null;
                        currentLayerName = null;
                    }
                    if (reader.TokenType == JsonTokenType.EndObject
                        && layersDepth == reader.CurrentDepth)
                        layersDepth = null;
                    if (reader.TokenType == JsonTokenType.EndObject
                        && groupedActorsDepth == reader.CurrentDepth)
                        groupedActorsDepth = null;
                    if (reader.TokenType == JsonTokenType.PropertyName && reader.CurrentDepth == 1)
                    {
                        property = reader.GetString();
                        continue;
                    }
                    if (readingLayerOrder)
                    {
                        if (reader.TokenType == JsonTokenType.EndArray && reader.CurrentDepth == 1)
                        {
                            readingLayerOrder = false;
                            property = null;
                        }
                        else if (reader.TokenType == JsonTokenType.String && reader.CurrentDepth == 2)
                        {
                            string? layerName = reader.GetString();
                            if (string.IsNullOrWhiteSpace(layerName))
                                return null;
                            layerOrder.Add(layerName);
                        }
                        else
                            return null;
                        continue;
                    }
                    if (reader.CurrentDepth != 1 || property is null)
                        continue;
                    if (property == "layerOrder" && reader.TokenType == JsonTokenType.StartArray)
                    {
                        readingLayerOrder = true;
                        continue;
                    }
                    if (property == "type" && reader.TokenType == JsonTokenType.String)
                        type = reader.GetString();
                    else if (property == "mapName" && reader.TokenType == JsonTokenType.String)
                        mapName = reader.GetString();
                    else if (property == "width"
                        && reader.TokenType == JsonTokenType.Number
                        && reader.TryGetInt32(out int parsedWidth))
                    {
                        width = parsedWidth;
                    }
                    else if (property == "height"
                        && reader.TokenType == JsonTokenType.Number
                        && reader.TryGetInt32(out int parsedHeight))
                    {
                        height = parsedHeight;
                    }
                    property = null;
                }
                int consumed = checked((int)reader.BytesConsumed);
                buffered = available - consumed;
                if (buffered != 0)
                    Buffer.BlockCopy(buffer, consumed, buffer, 0, buffered);
                state = reader.CurrentState;
                if (finalBlock && !rootEnded)
                    return null;
            }
            if (!string.Equals(type, "map", StringComparison.Ordinal)
                || !isValidMapSize(width, height)
                || layerOrder.Count == 0
                || layerOrder.Any(string.IsNullOrWhiteSpace)
                || layerOrder.Distinct(StringComparer.Ordinal).Count() != layerOrder.Count
                || !layersObjectSeen
                || layerShapes.Count != layerOrder.Count
                || layerShapes.Keys.Except(layerOrder, StringComparer.Ordinal).Any()
                || layerOrder.Any(layerName =>
                    !layerShapes.TryGetValue(layerName, out MapCatalogLayerShape? shape)
                    || !isValidCatalogGrid(shape.TilesRows, width, height)
                    || !isValidCatalogGrid(shape.AutoTileRows, width, height)))
            {
                return null;
            }
            string stem = Path.GetFileNameWithoutExtension(path);
            string key = worldKey + "/" + stem;
            MapCatalogEntry result = new(
                key,
                string.IsNullOrWhiteSpace(mapName) ? stem : mapName,
                MapCatalogEntryKind.WorldChildMap,
                worldKey,
                width,
                height,
                layerOrder,
                actorTags);
            nextMapCatalogCache[cacheKey] = new JsonObject
            {
                ["length"] = file.Length,
                ["lastWriteUtcTicks"] = file.LastWriteTimeUtc.Ticks,
                ["creationUtcTicks"] = file.CreationTimeUtc.Ticks,
                ["entry"] = createMapCatalogData(result),
            };
            return result;
        }
        catch (Exception exception) when (
            exception is JsonException
            or IOException
            or UnauthorizedAccessException)
        {
            return null;
        }
    }

    private static bool isValidCatalogGrid(IReadOnlyList<int>? rows, int width, int height)
    {
        return rows is not null
            && rows.Count == height
            && rows.All(columns => columns == width);
    }

    private IReadOnlyList<MapCatalogEntry> getMapCatalogEntries()
    {
        return sections["MapCatalog"].Data.Values
            .Select(readMapCatalogEntry)
            .Where(entry => entry is not null)
            .Select(entry => entry!)
            .OrderBy(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap ? 1 : 0)
            .ThenBy(entry => entry.Key, StringComparer.Ordinal)
            .ToArray();
    }

    private IReadOnlyList<string> getAllMapKeys()
    {
        return getMapCatalogEntries()
            .Where(entry => entry.Kind is MapCatalogEntryKind.StandaloneMap or MapCatalogEntryKind.WorldChildMap)
            .Select(entry => entry.Key)
            .ToArray();
    }

    private bool containsMapKey(string key)
    {
        return tryGetMapCatalogEntry(normaliseMapKey(key), out _);
    }

    private bool tryGetMapCatalogEntry(string key, out MapCatalogEntry entry)
    {
        foreach (MapCatalogEntryKind kind in new[]
                 {
                     MapCatalogEntryKind.StandaloneMap,
                     MapCatalogEntryKind.WorldChildMap,
                 })
        {
            if (sections["MapCatalog"].Data.TryGetValue(
                    getMapCatalogDataKey(kind, key),
                    out JsonObject? data))
            {
                MapCatalogEntry? parsed = readMapCatalogEntry(data);
                if (parsed is not null)
                {
                    entry = parsed;
                    return true;
                }
            }
        }
        entry = null!;
        return false;
    }

    private bool tryGetMapsRelativePath(string fullPath, out string relativePath)
    {
        string mapsRoot = Path.GetFullPath(Path.Combine(ProjectPath, "Data", "Maps"));
        string relative = Path.GetRelativePath(mapsRoot, fullPath);
        if (Path.IsPathRooted(relative)
            || relative == ".."
            || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal))
        {
            relativePath = string.Empty;
            return false;
        }
        relativePath = relative == "." ? string.Empty : relative;
        return true;
    }

    private void setMapCatalogEntry(MapCatalogEntry entry)
    {
        sections["MapCatalog"].Data[getMapCatalogDataKey(entry.Kind, entry.Key)] =
            createMapCatalogData(entry);
    }

    private static JsonObject createMapCatalogData(MapCatalogEntry entry)
    {
        JsonArray layerOrder = new JsonArray();
        foreach (string name in entry.LayerOrder)
            layerOrder.Add(name);
        JsonArray actorTags = new JsonArray();
        foreach (string tag in entry.ActorTags)
            actorTags.Add(tag);
        JsonObject data = new JsonObject
        {
            ["key"] = entry.Key,
            ["displayName"] = entry.DisplayName,
            ["kind"] = entry.Kind.ToString(),
            ["width"] = entry.Width,
            ["height"] = entry.Height,
            ["layerOrder"] = layerOrder,
            ["actorTags"] = actorTags,
        };
        if (entry.WorldKey is not null)
            data["worldKey"] = entry.WorldKey;
        return data;
    }

    private void removeMapCatalogEntry(MapCatalogEntryKind kind, string key)
    {
        sections["MapCatalog"].Data.Remove(getMapCatalogDataKey(kind, key));
    }

    private static string getMapCatalogDataKey(MapCatalogEntryKind kind, string key)
    {
        return kind + ":" + key;
    }

    private static MapCatalogEntry? readMapCatalogEntry(JsonObject data)
    {
        string? key = getString(data["key"]);
        string? displayName = getString(data["displayName"]);
        string? kindName = getString(data["kind"]);
        if (string.IsNullOrWhiteSpace(key)
            || string.IsNullOrWhiteSpace(displayName)
            || !Enum.TryParse(kindName, out MapCatalogEntryKind kind))
        {
            return null;
        }
        return new MapCatalogEntry(
            key,
            displayName,
            kind,
            getString(data["worldKey"]),
            data["width"]?.GetValue<int?>() ?? 0,
            data["height"]?.GetValue<int?>() ?? 0,
            readStringArray(data["layerOrder"]),
            readStringArray(data["actorTags"]));
    }

    private static MapCatalogEntry createMapCatalogEntry(
        string key,
        MapCatalogEntryKind kind,
        string? worldKey,
        JsonObject map)
    {
        tryReadMapShape(map, out int width, out int height, out IReadOnlyList<string> layerOrder);
        string? mapName = getString(map["mapName"]);
        return new MapCatalogEntry(
            key,
            string.IsNullOrWhiteSpace(mapName) ? Path.GetFileName(key) : mapName,
            kind,
            worldKey,
            width,
            height,
            layerOrder,
            readActorTags(map));
    }

    private static IReadOnlyList<string> readActorTags(JsonObject map)
    {
        List<string> result = [];
        foreach (MapActorCollection collection in enumerateActorCollections(map))
            appendActorTags(collection.Actors, result);
        return result;
    }

    private static void appendActorTags(JsonArray actors, ICollection<string> result)
    {
        foreach (JsonObject actor in actors.OfType<JsonObject>())
        {
            string? tag = getString(actor["tag"]);
            if (!string.IsNullOrWhiteSpace(tag))
                result.Add(tag);
        }
    }

    private static bool tryReadMapShape(
        JsonObject map,
        out int width,
        out int height,
        out IReadOnlyList<string> layerOrder,
        bool validateGrids = false)
    {
        width = map["width"]?.GetValue<int?>() ?? 0;
        height = map["height"]?.GetValue<int?>() ?? 0;
        layerOrder = readStringArray(map["layerOrder"]);
        if (!isValidMapSize(width, height)
            || layerOrder.Count == 0
            || layerOrder.Any(string.IsNullOrWhiteSpace)
            || layerOrder.Distinct(StringComparer.Ordinal).Count() != layerOrder.Count)
        {
            return false;
        }
        if (!validateGrids)
            return true;
        if (map["layers"] is not JsonObject layers
            || layers.Count != layerOrder.Count
            || layers.Select(entry => entry.Key).Except(layerOrder, StringComparer.Ordinal).Any())
            return false;
        foreach (string layerName in layerOrder)
        {
            if (layers[layerName] is not JsonObject layer
                || !isValidMapGrid(layer["tiles"], width, height)
                || !isValidMapGrid(layer["autoTiles"], width, height))
            {
                return false;
            }
        }
        return true;
    }

    private static bool isValidMapGrid(JsonNode? value, int width, int height)
    {
        if (value is not JsonArray rows || rows.Count != height)
            return false;
        foreach (JsonNode? row in rows)
        {
            if (row is not JsonArray cells || cells.Count != width)
                return false;
        }
        return true;
    }

    private static bool mapMatchesCatalogEntry(JsonObject map, MapCatalogEntry entry)
    {
        return tryReadMapShape(map, out int width, out int height, out IReadOnlyList<string> layerOrder, true)
            && width == entry.Width
            && height == entry.Height
            && layerOrder.SequenceEqual(entry.LayerOrder, StringComparer.Ordinal);
    }

    private static IReadOnlyList<string> readStringArray(JsonNode? value)
    {
        if (value is not JsonArray array)
            return [];
        List<string> result = [];
        foreach (JsonNode? item in array)
        {
            string? text = getString(item);
            if (text is null)
                return [];
            result.Add(text);
        }
        return result;
    }
}
