using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    public bool PaintMapCells(string mapKey, string layerName, IReadOnlyList<MapTileEdit> cells)
    {
        if (getMap(mapKey) is not JsonObject map || map["layers"]?[layerName] is not JsonObject layer
            || !(layer["visible"]?.GetValue<bool?>() ?? true))
            return false;
        int width = map["width"]?.GetValue<int>() ?? 0;
        int height = map["height"]?.GetValue<int>() ?? 0;
        Dictionary<(int X, int Y), MapTileEdit> values = new();
        foreach (MapTileEdit cell in cells)
        {
            if (cell.X >= 0 && cell.Y >= 0 && cell.X < width && cell.Y < height)
                values[(cell.X, cell.Y)] = cell;
        }
        List<JsonDataEdit> edits = [];
        Dictionary<string, JsonArray?> expandedGrids = new(StringComparer.Ordinal);
        foreach (MapTileEdit cell in values.Values)
        {
            JsonNode? tile = cell.TileNumber is int number ? JsonValue.Create(number) : null;
            JsonNode? autoTile = string.IsNullOrWhiteSpace(cell.AutoTileKey) ? null : JsonValue.Create(cell.AutoTileKey);
            if (JsonNode.DeepEquals(readMapCell(layer["tiles"], cell.X, cell.Y), tile)
                && JsonNode.DeepEquals(readMapCell(layer["autoTiles"], cell.X, cell.Y), autoTile))
                continue;
            addMapTileEdit(layer, layerName, "tiles", cell, tile, width, height, expandedGrids, edits);
            addMapTileEdit(layer, layerName, "autoTiles", cell, autoTile, width, height, expandedGrids, edits);
        }
        foreach (KeyValuePair<string, JsonArray?> grid in expandedGrids)
            if (grid.Value is not null)
                edits.Add(new(JsonDataEdit.Operation.Set, ["layers", layerName, grid.Key], grid.Value));
        return commitMapEdits(mapKey, map, edits);
    }

    public int? AddMapActor(string mapKey, string layerName, JsonObject source, int x, int y, JsonObject? overrides = null)
    {
        if (getMap(mapKey) is not JsonObject map || map["layers"]?[layerName] is not JsonObject layer
            || !(layer["visible"]?.GetValue<bool?>() ?? true) || !isMapPositionValid(map, x, y))
            return null;
        JsonArray? actors = map["actors"]?[layerName] as JsonArray;
        if (actors?.OfType<JsonObject>().Any(actor => actor["position"] is JsonArray position
                && position.Count >= 2 && int.TryParse(position[0]?.ToString(), out int actorX) && actorX == x
                && int.TryParse(position[1]?.ToString(), out int actorY) && actorY == y) == true)
            return null;
        string reference = ProjectDataStore.getString(source["bp"]) ?? string.Empty;
        if (reference.Length == 0)
            return null;
        JsonObject candidate = (JsonObject)source.DeepClone();
        string tag = MapTagService.CreateDefault(store, mapKey, reference, x, y);
        candidate["tag"] = tag;
        candidate["position"] = new JsonArray(x, y);
        int index = actors?.Count ?? 0;
        List<JsonDataEdit> edits = [new(JsonDataEdit.Operation.Insert, ["actors", layerName, index], candidate)];
        if (overrides is not null)
            edits.Add(new(JsonDataEdit.Operation.Set, ["BPClassVarChanged", tag], overrides));
        return commitMapEdits(mapKey, map, edits) ? index : null;
    }

    public bool MoveMapActor(string mapKey, string layerName, int index, string expectedTag, int x, int y)
    {
        if (!tryGetEditableActor(mapKey, layerName, index, expectedTag, out JsonObject map, out _))
            return false;
        int width = map["width"]?.GetValue<int>() ?? 0;
        int height = map["height"]?.GetValue<int>() ?? 0;
        if (width <= 0 || height <= 0)
            return false;
        return commitMapEdits(mapKey, map,
            [new(JsonDataEdit.Operation.Set, ["actors", layerName, index, "position"],
                new JsonArray(Math.Clamp(x, 0, width - 1), Math.Clamp(y, 0, height - 1)))]);
    }

    public bool DeleteMapActor(string mapKey, string layerName, int index, string expectedTag)
    {
        if (!tryGetEditableActor(mapKey, layerName, index, expectedTag, out JsonObject map, out _))
            return false;
        List<JsonDataEdit> edits = [new(JsonDataEdit.Operation.Remove, ["actors", layerName, index])];
        addRemoveActorOverrides(map, expectedTag, null, edits);
        return commitMapEdits(mapKey, map, edits);
    }

    public string? RenameMapActorTag(string mapKey, string layerName, int index, string expectedTag, string requestedTag)
    {
        if (!tryGetEditableActor(mapKey, layerName, index, expectedTag, out JsonObject map, out _))
            return null;
        string tag = MapTagService.MakeUnique(store, mapKey, requestedTag, layerName, index);
        if (string.Equals(tag, expectedTag, StringComparison.Ordinal))
            return tag;
        List<JsonDataEdit> edits = [new(JsonDataEdit.Operation.Set, ["actors", layerName, index, "tag"], JsonValue.Create(tag))];
        if (map["BPClassVarChanged"] is JsonObject root && root[expectedTag] is JsonObject oldChanges)
        {
            JsonObject merged = root[tag]?.DeepClone() as JsonObject ?? [];
            foreach (KeyValuePair<string, JsonNode?> pair in oldChanges)
                merged[pair.Key] = pair.Value?.DeepClone();
            edits.Add(new(JsonDataEdit.Operation.Remove, ["BPClassVarChanged", expectedTag]));
            edits.Add(new(JsonDataEdit.Operation.Set, ["BPClassVarChanged", tag], merged));
        }
        return commitMapEdits(mapKey, map, edits) ? tag : null;
    }

    public bool SetMapActorOverride(string mapKey, string layerName, int index, string expectedTag, string name, JsonNode? value)
    {
        return tryGetEditableActor(mapKey, layerName, index, expectedTag, out JsonObject map, out _)
            && commitMapEdits(mapKey, map, [new(JsonDataEdit.Operation.Set, ["BPClassVarChanged", expectedTag, name], value)]);
    }

    public bool RemoveMapActorOverrides(string mapKey, string layerName, int index, string expectedTag, string? name = null)
    {
        if (!tryGetEditableActor(mapKey, layerName, index, expectedTag, out JsonObject map, out _))
            return false;
        List<JsonDataEdit> edits = [];
        addRemoveActorOverrides(map, expectedTag, name, edits);
        return commitMapEdits(mapKey, map, edits);
    }

    public int? AddMapLight(string mapKey, double x, double y)
    {
        if (getMap(mapKey) is not JsonObject map || !double.IsFinite(x) || !double.IsFinite(y))
            return null;
        int index = (map["lights"] as JsonArray)?.Count ?? 0;
        JsonObject light = new()
        {
            ["position"] = new JsonArray(x, y),
            ["color"] = new JsonArray(255, 255, 255, 255),
            ["radius"] = 256.0,
            ["intensity"] = 1.0,
        };
        return commitMapEdits(mapKey, map, [new(JsonDataEdit.Operation.Insert, ["lights", index], light)]) ? index : null;
    }

    public bool UpdateMapLight(string mapKey, int index, JsonObject expected, JsonObject light)
    {
        return getMap(mapKey) is JsonObject map
            && map["lights"] is JsonArray lights && index >= 0 && index < lights.Count
            && JsonNode.DeepEquals(lights[index], expected)
            && commitMapEdits(mapKey, map, [new(JsonDataEdit.Operation.Set, ["lights", index], light)]);
    }

    public bool DeleteMapLight(string mapKey, int index, JsonObject expected)
    {
        return getMap(mapKey) is JsonObject map
            && map["lights"] is JsonArray lights && index >= 0 && index < lights.Count
            && JsonNode.DeepEquals(lights[index], expected)
            && commitMapEdits(mapKey, map, [new(JsonDataEdit.Operation.Remove, ["lights", index])]);
    }

    public bool MapActorTagExists(string mapKey, string tag, string? ignoredLayerName = null, int? ignoredActorIndex = null)
    {
        return getMap(mapKey) is JsonObject map && MapTagService.ContainsTag(map, tag, ignoredLayerName, ignoredActorIndex);
    }

    internal bool commitMapEdits(string mapKey, JsonObject map, IReadOnlyList<JsonDataEdit> edits)
    {
        JsonDataEdit[] changed = edits.Where(edit => edit.Changes(map)).ToArray();
        if (changed.Length == 0 || changed.Any(edit => !edit.CanApply(map)))
            return false;
        mapKey = normaliseMapKey(mapKey);
        store.RecordMapSnapshot(mapKey);
        MapDataEditedEventArgs change = new(mapKey, changed);
        change.ApplyTo(map);
        if (change.ChangesActors)
            NotifyMapActorsChanged(mapKey);
        store.refreshModifiedState();
        NotifyMapContentChanged(mapKey, change);
        return true;
    }

    internal bool tryGetEditableActor(string mapKey, string layerName, int index, string expectedTag,
        out JsonObject map, out JsonObject actor)
    {
        map = getMap(mapKey)!;
        actor = null!;
        if (map?["layers"]?[layerName] is not JsonObject layer
            || !(layer["visible"]?.GetValue<bool?>() ?? true)
            || map["actors"]?[layerName] is not JsonArray actors || index < 0 || index >= actors.Count
            || actors[index] is not JsonObject current
            || !string.Equals(ProjectDataStore.getString(current["tag"]) ?? string.Empty, expectedTag, StringComparison.Ordinal))
            return false;
        actor = current;
        return true;
    }

    internal static void addRemoveActorOverrides(JsonObject map, string tag, string? name, ICollection<JsonDataEdit> edits)
    {
        if (map["BPClassVarChanged"] is not JsonObject root || root[tag] is not JsonObject changes
            || name is not null && !changes.ContainsKey(name))
            return;
        if (name is null || changes.Count == 1)
            edits.Add(new(JsonDataEdit.Operation.Remove, root.Count == 1 ? ["BPClassVarChanged"] : ["BPClassVarChanged", tag]));
        else
            edits.Add(new(JsonDataEdit.Operation.Remove, ["BPClassVarChanged", tag, name]));
    }

    internal static JsonNode? readMapCell(JsonNode? grid, int x, int y)
    {
        return grid is JsonArray rows && y < rows.Count && rows[y] is JsonArray row && x < row.Count ? row[x] : null;
    }

    internal static void addMapTileEdit(JsonObject layer, string layerName, string gridName, MapTileEdit cell,
        JsonNode? value, int width, int height, IDictionary<string, JsonArray?> expanded, ICollection<JsonDataEdit> edits)
    {
        if (!expanded.TryGetValue(gridName, out JsonArray? grid))
        {
            JsonArray? original = layer[gridName] as JsonArray;
            if (original is not null && original.Count >= height
                && original.Take(height).All(row => row is JsonArray cells && cells.Count >= width))
            {
                expanded[gridName] = null;
                edits.Add(new(JsonDataEdit.Operation.Set, ["layers", layerName, gridName, cell.Y, cell.X], value));
                return;
            }
            grid = original?.DeepClone() as JsonArray ?? [];
            while (grid.Count < height)
                grid.Add(new JsonArray());
            for (int y = 0; y < height; y++)
            {
                if (grid[y] is not JsonArray row)
                {
                    row = [];
                    grid[y] = row;
                }
                while (row.Count < width)
                    row.Add(null);
            }
            expanded[gridName] = grid;
        }
        if (grid is null)
            edits.Add(new(JsonDataEdit.Operation.Set, ["layers", layerName, gridName, cell.Y, cell.X], value));
        else
            ((JsonArray)grid[cell.Y]!)[cell.X] = value?.DeepClone();
    }

    internal static bool isMapPositionValid(JsonObject map, int x, int y)
    {
        return x >= 0 && y >= 0 && x < (map["width"]?.GetValue<int>() ?? 0) && y < (map["height"]?.GetValue<int>() ?? 0);
    }

}
