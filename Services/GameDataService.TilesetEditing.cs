using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    public bool RenameAutoTile(string oldKey, string newKey)
    {
        oldKey = normalizeDataKey(oldKey);
        newKey = normalizeDataKey(newKey);
        Dictionary<string, JsonObject> target = sections["AutoTiles"].Data;
        if (newKey.Length == 0 || !target.TryGetValue(oldKey, out JsonObject? value) || target.ContainsKey(newKey))
            return false;
        RecordSnapshot();
        target.Remove(oldKey);
        target.Add(newKey, value);
        completeTilesetEdit();
        return true;
    }

    public bool PasteTileset(string key, bool isAutoTile, JsonObject source)
    {
        key = normalizeDataKey(key);
        Dictionary<string, JsonObject> target = sections[isAutoTile ? "AutoTiles" : "Tilesets"].Data;
        if (key.Length == 0 || target.ContainsKey(key))
            return false;
        JsonObject copy = (JsonObject)source.DeepClone();
        copy["name"] = key;
        RecordSnapshot();
        target.Add(key, copy);
        completeTilesetEdit();
        return true;
    }

    public bool DeleteTileset(string key, bool isAutoTile)
    {
        key = normalizeDataKey(key);
        Dictionary<string, JsonObject> target = sections[isAutoTile ? "AutoTiles" : "Tilesets"].Data;
        if (!target.ContainsKey(key))
            return false;
        RecordSnapshot();
        target.Remove(key);
        completeTilesetEdit();
        return true;
    }

    public bool UpdateTilesetName(string key, bool isAutoTile, string name)
    {
        if (!tryGetTilesetForEdit(key, isAutoTile, out JsonObject target)
            || (target["name"]?.GetValue<string>() ?? string.Empty) == name)
        {
            return false;
        }
        RecordSnapshot();
        target["name"] = name;
        completeTilesetEdit();
        return true;
    }

    public bool UpdateTilesetImage(string key, bool isAutoTile, string assetPath, int width, int height)
    {
        if (!tryGetTilesetForEdit(key, isAutoTile, out JsonObject target)
            || width <= 0 || height <= 0
            || isAutoTile && (width < 96 || height < 128 || width % 96 != 0)
            || !GameAssetPath.TryResolveExistingFile(ProjectPath, assetPath, out _))
        {
            return false;
        }
        JsonObject next = (JsonObject)target.DeepClone();
        next["fileName"] = assetPath;
        if (isAutoTile)
            next["material"] ??= TilesetMetadata.CreateDefaultMaterial();
        else
            TilesetMetadata.ResizeForImage(next, width / getCellSize() * (height / getCellSize()));
        if (JsonNode.DeepEquals(target, next))
            return false;
        RecordSnapshot();
        sections[isAutoTile ? "AutoTiles" : "Tilesets"].Data[normalizeDataKey(key)] = next;
        completeTilesetEdit();
        return true;
    }

    public bool UpdateTilesetMetadata(string key, bool isAutoTile, string expectedAssetPath, string property, JsonNode value, IReadOnlyList<int> indices, int tileCount)
    {
        if (!tryGetTilesetForEdit(key, isAutoTile, out JsonObject target)
            || !string.Equals(target["fileName"]?.GetValue<string>() ?? string.Empty, expectedAssetPath, StringComparison.Ordinal)
            || !TilesetMetadata.IsValidValue(property, value, isAutoTile)
            || !isAutoTile && (tileCount <= 0 || indices.Count == 0 || indices.Any(index => index < 0 || index >= tileCount)))
        {
            return false;
        }
        int[] changes = isAutoTile ? [0] : indices.Distinct().ToArray();
        changes = changes.Where(index => !JsonNode.DeepEquals(TilesetMetadata.Read(target, property, index, isAutoTile), value)).ToArray();
        if (changes.Length == 0)
            return false;
        JsonNode copy = value.DeepClone();
        RecordSnapshot();
        TilesetMetadata.Apply(target, property, copy, changes, tileCount, isAutoTile);
        completeTilesetEdit();
        return true;
    }

    public bool UpdateTilesetDirection(string key, string expectedAssetPath, int index, int tileCount, int direction, bool value)
    {
        if (direction < 0 || direction >= 4 || !tryGetTilesetForEdit(key, false, out JsonObject target))
            return false;
        JsonArray directions = (JsonArray)TilesetMetadata.Read(target, "dir4", index, false);
        directions[direction] = value;
        return UpdateTilesetMetadata(key, false, expectedAssetPath, "dir4", directions, [index], tileCount);
    }

    public bool UpdateTilesetMaterial(string key, bool isAutoTile, string expectedAssetPath, int index, int tileCount, JsonObject initial, JsonObject edited)
    {
        if (!tryGetTilesetForEdit(key, isAutoTile, out JsonObject target))
            return false;
        string property = isAutoTile ? "material" : "materials";
        JsonObject current = (JsonObject)TilesetMetadata.Read(target, property, index, isAutoTile);
        JsonObject value = TilesetMetadata.MergeMaterial(current, initial, edited);
        return UpdateTilesetMetadata(key, isAutoTile, expectedAssetPath, property, value, isAutoTile ? [] : [index], tileCount);
    }

    private bool tryGetTilesetForEdit(string key, bool isAutoTile, out JsonObject target)
    {
        return sections[isAutoTile ? "AutoTiles" : "Tilesets"].Data.TryGetValue(normalizeDataKey(key), out target!);
    }

    private void completeTilesetEdit()
    {
        NotifyAllMapPreviewsChanged(false);
        refreshModifiedState();
    }
}
