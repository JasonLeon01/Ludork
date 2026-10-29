using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class AssetDataService
{
    public bool RenameAutoTile(string oldKey, string newKey)
    {
        return store.RenameDocumentResource("AutoTiles", oldKey, newKey);
    }

    public bool PasteTileset(string key, bool isAutoTile, JsonObject source)
    {
        key = ProjectDataStore.normalizeDataKey(key);
        EditorDocumentCollection target = (isAutoTile ? autoTileDocuments : tilesetDocuments);
        if (!store.canCreateDocument(isAutoTile ? "AutoTiles" : "Tilesets", key))
            return false;
        JsonObject copy = (JsonObject)source.DeepClone();
        copy["name"] = key;
        store.RecordDocumentSnapshot(isAutoTile ? "AutoTiles" : "Tilesets", key);
        target.Add(key, copy);
        completeTilesetEdit();
        return true;
    }

    public bool DeleteTileset(string key, bool isAutoTile)
    {
        return store.DeleteDocumentResource(isAutoTile ? "AutoTiles" : "Tilesets", key);
    }

    public bool UpdateTilesetName(string key, bool isAutoTile, string name)
    {
        if (!tryGetTilesetForEdit(key, isAutoTile, out JsonObject target)
            || (target["name"]?.GetValue<string>() ?? string.Empty) == name)
        {
            return false;
        }
        bool changed = store.ApplyDocumentEdits(isAutoTile ? "AutoTiles" : "Tilesets", ProjectDataStore.normalizeDataKey(key),
            [new JsonDataEdit(JsonDataEdit.Operation.Set, ["name"], JsonValue.Create(name))]);
        if (changed)
            store.Maps.NotifyAllMapPreviewsChanged(false);
        return changed;
    }

    public bool UpdateTilesetImage(string key, bool isAutoTile, string assetPath, int width, int height)
    {
        if (!tryGetTilesetForEdit(key, isAutoTile, out JsonObject target)
            || width <= 0 || height <= 0
            || isAutoTile && (width < 96 || height < 128 || width % 96 != 0)
            || !store.AssetPaths.TryResolveAsset(assetPath, out _))
        {
            return false;
        }
        JsonObject next = (JsonObject)target.DeepClone();
        next["fileName"] = assetPath;
        if (isAutoTile)
            next["material"] ??= JsonValue.Create(string.Empty);
        else
            TilesetMetadata.ResizeForImage(next, width / store.Configs.getCellSize() * (height / store.Configs.getCellSize()));
        if (JsonNode.DeepEquals(target, next))
            return false;
        store.RecordDocumentSnapshot(isAutoTile ? "AutoTiles" : "Tilesets", key);
        (isAutoTile ? autoTileDocuments : tilesetDocuments)[ProjectDataStore.normalizeDataKey(key)] = next;
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
        List<JsonDataEdit> edits = [];
        if (isAutoTile)
            edits.Add(new(JsonDataEdit.Operation.Set, [property], value));
        else if (property == "materials")
        {
            foreach (int index in changes)
                edits.Add(new(value.GetValue<string>().Length == 0 ? JsonDataEdit.Operation.Remove : JsonDataEdit.Operation.Set,
                    [property, index.ToString(System.Globalization.CultureInfo.InvariantCulture)], value));
        }
        else if (target[property] is JsonArray current && current.Count == tileCount)
        {
            foreach (int index in changes)
                edits.Add(new(JsonDataEdit.Operation.Set, [property, index], value));
        }
        else
        {
            JsonObject resized = new() { [property] = target[property]?.DeepClone() };
            TilesetMetadata.Apply(resized, property, value, changes, tileCount, false);
            edits.Add(new(JsonDataEdit.Operation.Set, [property], resized[property]));
        }
        bool changed = store.ApplyDocumentEdits(isAutoTile ? "AutoTiles" : "Tilesets", ProjectDataStore.normalizeDataKey(key), edits);
        if (changed)
            store.Maps.NotifyAllMapPreviewsChanged(false);
        return changed;
    }

    public bool UpdateTilesetDirection(string key, string expectedAssetPath, int index, int tileCount, int direction, bool value)
    {
        if (direction < 0 || direction >= 4 || !tryGetTilesetForEdit(key, false, out JsonObject target))
            return false;
        JsonArray directions = (JsonArray)TilesetMetadata.Read(target, "dir4", index, false);
        directions[direction] = value;
        return UpdateTilesetMetadata(key, false, expectedAssetPath, "dir4", directions, [index], tileCount);
    }

    internal bool tryGetTilesetForEdit(string key, bool isAutoTile, out JsonObject target)
    {
        return (isAutoTile ? autoTileDocuments : tilesetDocuments).TryGetValue(ProjectDataStore.normalizeDataKey(key), out target!);
    }

    internal void completeTilesetEdit()
    {
        store.refreshModifiedState();
        store.Maps.NotifyAllMapPreviewsChanged(false);
    }

}
