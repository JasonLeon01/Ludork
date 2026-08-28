using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using Ludork.ViewModels;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class MapPanel
{
    private void handleLightPointerPressed(PointerPressedEventArgs args, Point position, int width, int height)
    {
        PointerPoint pointer = args.GetCurrentPoint(this);
        if (pointer.Properties.IsRightButtonPressed)
        {
            if (getMapBasePosition(position, width, height) is { } contextPosition)
                setSelectedLightIndex(hitTestLight(contextPosition));
            showLightContextMenu(position, width, height);
            args.Handled = true;
            return;
        }
        if (!pointer.Properties.IsLeftButtonPressed || getMapBasePosition(position, width, height) is not { } basePosition)
            return;
        int? hit = hitTestLight(basePosition);
        setSelectedLightIndex(hit);
        if (hit is not int index || CurrentMapData?["lights"] is not JsonArray lights || lights[index] is not JsonObject light || !tryGetLight(light, out Point center, out double radius))
        {
            InvalidateVisual();
            return;
        }
        double distance = getDistance(basePosition, center);
        args.Pointer.Capture(this);
        if (Math.Abs(distance - radius) <= LightEdgeTolerance)
        {
            lightRadiusDragging = true;
            lightDragCenter = center;
        }
        else
        {
            lightMoveDragging = true;
            lightDragOffset = basePosition - center;
        }
        args.Handled = true;
        InvalidateVisual();
    }

    private void updateLightDrag(Point position, int width, int height)
    {
        if (!lightMoveDragging && !lightRadiusDragging || selectedLightIndex is not int index || getMapBasePosition(position, width, height) is not { } basePosition || CurrentMapData?["lights"] is not JsonArray lights || lights[index] is not JsonObject light)
            return;
        recordMapEditSnapshot();
        if (lightMoveDragging)
            light["position"] = new JsonArray(basePosition.X - lightDragOffset.X, basePosition.Y - lightDragOffset.Y);
        else
            light["radius"] = Math.Max(0, getDistance(basePosition, lightDragCenter));
        markMapModified();
        LightDataChanged?.Invoke(this, new LightDataChangedEventArgs(CurrentMapKey ?? string.Empty, index, light));
        InvalidateVisual();
    }

    private void handleActorPointerPressed(PointerPressedEventArgs args, (int X, int Y) grid)
    {
        if (selectedLayerName is null)
            return;
        PointerPoint point = args.GetCurrentPoint(this);
        int? hit = getMapDisplayPosition(args.GetPosition(this), out int width, out int height) is { } mapPosition
            ? hitTestActor(selectedLayerName, mapPosition)
            : null;
        if (point.Properties.IsRightButtonPressed)
        {
            setSelectedActor(hit is null ? null : selectedLayerName, hit, true);
            showActorContextMenu(grid);
            args.Handled = true;
            InvalidateVisual();
            return;
        }
        if (!point.Properties.IsLeftButtonPressed)
            return;
        if (hit is int index)
        {
            setSelectedActor(selectedLayerName, index, true);
            if (selectedLayerEditable)
            {
                actorMoveIndex = index;
                actorMoveLayer = selectedLayerName;
                args.Pointer.Capture(this);
            }
        }
        else if (selectedLayerEditable && !string.IsNullOrWhiteSpace(pendingActor))
            placeActor(pendingActor!, grid);
        else
            setSelectedActor(null, null, true);
        args.Handled = true;
        InvalidateVisual();
    }

    private void moveSelectedActor((int X, int Y) grid)
    {
        if (!selectedLayerEditable || actorMoveIndex is not int index || actorMoveLayer is null || getActorList(actorMoveLayer, false) is not JsonArray actors || actors[index] is not JsonObject actor)
            return;
        if (tryGetActorPosition(actor, out int oldX, out int oldY) && oldX == grid.X && oldY == grid.Y)
            return;
        recordMapEditSnapshot();
        actor["position"] = new JsonArray(grid.X, grid.Y);
        markActorDataModified();
        InvalidateVisual();
    }

    private void writeTileSelection((int X, int Y) grid)
    {
        if (!selectedLayerEditable || selectedLayerName is null || CurrentMapData?["layers"]?[selectedLayerName] is not JsonObject layer)
            return;
        bool changed = false;
        if (selectedTiles is { } tiles)
        {
            for (int y = 0; y < tiles.Height; y++)
            for (int x = 0; x < tiles.Width; x++)
                changed |= writeCell(layer, grid.X + x, grid.Y + y, tiles.OriginTileNumber + y * getTilesetColumnCount(layer) + x, null);
        }
        else
            changed = writeCell(layer, grid.X, grid.Y, null, selectedAutoTileKey);
        if (changed)
            scheduleBrushLayerRefresh(selectedLayerName);
    }

    private void writeTileRectangle((int X, int Y) start, (int X, int Y) end)
    {
        int minX = Math.Min(start.X, end.X);
        int minY = Math.Min(start.Y, end.Y);
        int maxX = Math.Max(start.X, end.X);
        int maxY = Math.Max(start.Y, end.Y);
        bool changed = false;
        if (!selectedLayerEditable || selectedLayerName is null || CurrentMapData?["layers"]?[selectedLayerName] is not JsonObject layer)
            return;
        for (int y = minY; y <= maxY; y++)
        for (int x = minX; x <= maxX; x++)
        {
            if (selectedTiles is { } tiles)
            {
                for (int py = 0; py < tiles.Height; py++)
                for (int px = 0; px < tiles.Width; px++)
                    changed |= writeCell(layer, x + px, y + py, tiles.OriginTileNumber + py * getTilesetColumnCount(layer) + px, null);
            }
            else
                changed |= writeCell(layer, x, y, null, selectedAutoTileKey);
        }
        if (changed)
        {
            dirtyLayerNames.Add(selectedLayerName);
            animationStateDirty = true;
        }
    }

    private bool writeCell(JsonObject layer, int x, int y, int? tileNumber, string? autoTileKey)
    {
        if (!tryGetMapSize(out int width, out int height) || x < 0 || y < 0 || x >= width || y >= height)
            return false;
        JsonNode? nextTile = tileNumber is null ? null : JsonValue.Create(tileNumber.Value);
        JsonNode? nextAuto = string.IsNullOrWhiteSpace(autoTileKey) ? null : JsonValue.Create(autoTileKey);
        JsonNode? currentTile = getCell(layer["tiles"] as JsonArray, x, y);
        JsonNode? currentAuto = getCell(layer["autoTiles"] as JsonArray, x, y);
        if (JsonNode.DeepEquals(currentTile, nextTile) && JsonNode.DeepEquals(currentAuto, nextAuto))
            return false;
        recordMapEditSnapshot();
        JsonArray tiles = ensureGrid(layer, "tiles", width, height);
        JsonArray autoTiles = ensureGrid(layer, "autoTiles", width, height);
        JsonArray tileRow = getRow(tiles, y)!;
        JsonArray autoRow = getRow(autoTiles, y)!;
        tileRow[x] = nextTile;
        autoRow[x] = nextAuto;
        markMapModified();
        return true;
    }

    private void pickTileAt((int X, int Y) grid)
    {
        if (selectedLayerName is null || CurrentMapData?["layers"]?[selectedLayerName] is not JsonObject layer)
            return;
        JsonArray? autoRow = getRow(layer["autoTiles"] as JsonArray, grid.Y);
        string? autoKey = autoRow?[grid.X]?.GetValue<string>();
        if (!string.IsNullOrWhiteSpace(autoKey))
        {
            selectedAutoTileKey = autoKey;
            selectedTiles = null;
        }
        else if (tryGetInt(getRow(layer["tiles"] as JsonArray, grid.Y)?[grid.X], out int tile))
        {
            selectedTiles = new TileSelection(tile, 1, 1);
            selectedAutoTileKey = null;
        }
        else
        {
            selectedTiles = null;
            selectedAutoTileKey = null;
        }
        TileSelectionPicked?.Invoke(this, new TileSelectionChangedEventArgs(selectedTiles, selectedAutoTileKey));
        InvalidateVisual();
    }

    private void showEditFeedback(string key)
    {
        EditFeedbackRequested?.Invoke(this, LocaleService.Get(key));
    }

    private void placeActor(string reference, (int X, int Y) grid)
    {
        if (!selectedLayerEditable || selectedLayerName is null || hasActorAt(selectedLayerName, grid))
            return;
        recordMapHistorySnapshot();
        JsonArray actors = getActorList(selectedLayerName, true)!;
        JsonObject actor = new()
        {
            ["tag"] = makeActorTag(reference, selectedLayerName, grid),
            ["bp"] = reference,
            ["position"] = new JsonArray(grid.X, grid.Y),
        };
        actors.Add(actor);
        invalidateActorRenderStates();
        setSelectedActor(selectedLayerName, actors.Count - 1, true);
        markActorDataModified();
    }

    private void pasteActor((int X, int Y) grid)
    {
        if (!selectedLayerEditable || actorClipboard is null || selectedLayerName is null || hasActorAt(selectedLayerName, grid))
            return;
        JsonObject copy = (JsonObject)actorClipboard.DeepClone();
        string reference = copy["bp"]?.GetValue<string>() ?? string.Empty;
        copy["position"] = new JsonArray(grid.X, grid.Y);
        copy["tag"] = makeActorTag(reference, selectedLayerName, grid);
        recordMapHistorySnapshot();
        JsonArray actors = getActorList(selectedLayerName, true)!;
        actors.Add(copy);
        invalidateActorRenderStates();
        if (actorClassVarChangesClipboard is not null && copy["tag"]?.GetValue<string>() is string newTag)
        {
            JsonObject root = CurrentMapData?["BPClassVarChanged"] as JsonObject ?? new JsonObject();
            CurrentMapData!["BPClassVarChanged"] = root;
            root[newTag] = actorClassVarChangesClipboard.DeepClone();
        }
        setSelectedActor(selectedLayerName, actors.Count - 1, true);
        markActorDataModified();
        InvalidateVisual();
    }

    private void deleteSelectedActor()
    {
        if (!selectedLayerEditable || selectedActorLayer is null || selectedActorIndex is not int index || getActorList(selectedActorLayer, false) is not JsonArray actors || index < 0 || index >= actors.Count)
            return;
        JsonObject? actor = actors[index] as JsonObject;
        recordMapHistorySnapshot();
        if (actor?["tag"]?.GetValue<string>() is string tag && CurrentMapData?["BPClassVarChanged"] is JsonObject root)
        {
            root.Remove(tag);
            if (root.Count == 0)
                CurrentMapData.Remove("BPClassVarChanged");
        }
        actors.RemoveAt(index);
        invalidateActorRenderStates();
        setSelectedActor(null, null, true);
        markActorDataModified();
        InvalidateVisual();
    }

    private void showActorContextMenu((int X, int Y) grid)
    {
        MenuItem copy = new() { Header = LocaleService.Get("COPY"), IsEnabled = getSelectedActor() is not null };
        copy.Click += (_, _) => copySelectedActorToClipboard();
        MenuItem paste = new() { Header = LocaleService.Get("PASTE"), IsEnabled = selectedLayerEditable && actorClipboard is not null && selectedLayerName is not null && !hasActorAt(selectedLayerName, grid) };
        paste.Click += (_, _) => pasteActor(grid);
        MenuItem delete = new() { Header = LocaleService.Get("DELETE"), IsEnabled = selectedLayerEditable && getSelectedActor() is not null };
        delete.Click += (_, _) => deleteSelectedActor();
        ContextMenu menu = new() { ItemsSource = new object[] { copy, paste, delete } };
        menu.Open(this);
    }

    private Matrix createActorTransform(int gridX, int gridY, Vector translation, Vector scale, double rotation)
    {
        double displayScale = tileSize / (double)SourceTileSize;
        double radians = rotation * Math.PI / 180.0;
        double cos = Math.Cos(radians);
        double sin = Math.Sin(radians);
        double sx = scale.X * displayScale;
        double sy = scale.Y * displayScale;
        double m11 = cos * sx;
        double m12 = sin * sx;
        double m21 = -sin * sy;
        double m22 = cos * sy;
        Rect cell = getLocalTileRect(gridX, gridY);
        double offsetX = cell.X + translation.X * displayScale;
        double offsetY = cell.Y + translation.Y * displayScale;
        return new Matrix(m11, m12, m21, m22, offsetX, offsetY);
    }

    private void setSelectedActor(string? layerName, int? index, bool notify, bool force = false)
    {
        JsonObject? actor = null;
        if (layerName is not null && index is int actorIndex && getActorList(layerName, false) is JsonArray actors
            && actorIndex >= 0 && actorIndex < actors.Count)
            actor = actors[actorIndex] as JsonObject;
        if (actor is null)
        {
            layerName = null;
            index = null;
        }
        bool changed = !string.Equals(selectedActorLayer, layerName, StringComparison.Ordinal) || selectedActorIndex != index;
        selectedActorLayer = layerName;
        selectedActorIndex = index;
        if (!notify || !changed && !force)
            return;
        ActorSelectionChanged?.Invoke(this, new ActorSelectionChangedEventArgs(
            CurrentMapKey ?? string.Empty,
            selectedActorLayer,
            selectedActorIndex,
            actor));
    }

    private void copySelectedActorToClipboard()
    {
        JsonObject? actor = getSelectedActor();
        actorClipboard = actor is null ? null : (JsonObject)actor.DeepClone();
        actorClassVarChangesClipboard = null;
        if (actor?["tag"]?.GetValue<string>() is not string tag
            || CurrentMapData?["BPClassVarChanged"]?[tag] is not JsonObject changes
            || changes.Count == 0)
            return;
        actorClassVarChangesClipboard = (JsonObject)changes.DeepClone();
    }

}

