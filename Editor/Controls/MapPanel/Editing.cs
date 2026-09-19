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
using Ludork.Models;
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
        if (!pointer.Properties.IsLeftButtonPressed && !pointer.Properties.IsRightButtonPressed
            || getMapBasePosition(position, width, height) is not { } basePosition)
            return;
        int? actorHit = hitTestLightActor(position, basePosition, width, height, out int? hit);
        if (actorHit is int actorIndex && selectedLayerName is not null)
        {
            setSelectedLightIndex(null);
            setSelectedActor(selectedLayerName, actorIndex, true);
            if (pointer.Properties.IsLeftButtonPressed && canEditMap
                && getGridPosition(position, width, height) is { } grid
                && getSelectedActor() is JsonObject actor
                && tryGetActorPosition(actor, out int actorX, out int actorY))
            {
                actorMoveIndex = actorIndex;
                actorMoveLayer = selectedLayerName;
                actorMoveOffset = (grid.X - actorX, grid.Y - actorY);
                capturePointer(args.Pointer);
            }
            args.Handled = true;
            InvalidateVisual();
            return;
        }
        setSelectedActor(null, null, true);
        setSelectedLightIndex(hit);
        if (pointer.Properties.IsRightButtonPressed)
        {
            showLightContextMenu(position, width, height);
            args.Handled = true;
            return;
        }
        if (hit is not int index || CurrentMapData?["lights"] is not JsonArray lights || lights[index] is not JsonObject light || !tryGetLight(light, out Point center, out double radius))
        {
            InvalidateVisual();
            return;
        }
        double distance = getDistance(basePosition, center);
        capturePointer(args.Pointer);
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
        if (IsRuntimeEditing || editingContext?.IsEditable != true)
            return;
        if (!lightMoveDragging && !lightRadiusDragging || selectedLightIndex is not int index || getMapBasePosition(position, width, height) is not { } basePosition || CurrentMapData?["lights"] is not JsonArray lights || index < 0 || index >= lights.Count || lights[index] is not JsonObject light)
            return;
        JsonObject next = (JsonObject)light.DeepClone();
        if (lightMoveDragging)
            next["position"] = new JsonArray(basePosition.X - lightDragOffset.X, basePosition.Y - lightDragOffset.Y);
        else
            next["radius"] = Math.Max(0, getDistance(basePosition, lightDragCenter));
        if (gameData is null || CurrentMapKey is null || !gameData.UpdateMapLight(CurrentMapKey, index, light, next))
            return;
        LightDataChanged?.Invoke(this, new LightDataChangedEventArgs(CurrentMapKey, index, next));
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
        (string Layer, int Index)? target = resolveMapActorHit(selectedLayerName, hit);
        if (point.Properties.IsRightButtonPressed)
        {
            setSelectedActor(target?.Layer, target?.Index, true);
            showActorContextMenu(grid);
            args.Handled = true;
            InvalidateVisual();
            return;
        }
        if (!point.Properties.IsLeftButtonPressed)
            return;
        if (target is { } selected)
        {
            setSelectedActor(selected.Layer, selected.Index, true);
            if (canEditMap)
            {
                if (beginActorPropertyDrag(args))
                {
                    args.Handled = true;
                    InvalidateVisual();
                    return;
                }
                actorMoveIndex = selected.Index;
                actorMoveLayer = selected.Layer;
                movingRuntimeActorId = selectedRuntimeActorId;
                actorMoveOffset = default;
                if (IsRuntimeEditing && getSelectedActor() is JsonObject actor
                    && tryGetActorPosition(actor, out int actorX, out int actorY))
                    actorMoveOffset = (grid.X - actorX, grid.Y - actorY);
                capturePointer(args.Pointer);
            }
        }
        else if (canEditMap && !IsRuntimeEditing && !string.IsNullOrWhiteSpace(pendingActor))
            placeActor(pendingActor!, grid);
        else
            setSelectedActor(null, null, true);
        args.Handled = true;
        InvalidateVisual();
    }

    private void moveSelectedActor((int X, int Y) grid)
    {
        if (!canEditMap || actorMoveIndex is not int index || actorMoveLayer is null || getActorList(actorMoveLayer) is not JsonArray actors || index < 0 || index >= actors.Count || actors[index] is not JsonObject actor)
            return;
        if (IsRuntimeEditing)
        {
            if (actor["parentRuntimeId"] is not null)
                return;
        }
        if (IsRuntimeEditing || EditMode == MapEditMode.Light)
            grid = (grid.X - actorMoveOffset.X, grid.Y - actorMoveOffset.Y);
        if (tryGetActorPosition(actor, out int oldX, out int oldY) && oldX == grid.X && oldY == grid.Y)
            return;
        string actorId = IsRuntimeEditing ? movingRuntimeActorId ?? string.Empty : actor["tag"]?.GetValue<string>() ?? string.Empty;
        if (editingContext is null || CurrentMapKey is null || actorId.Length == 0
            || !editingContext.MoveActor(CurrentMapKey, actorMoveLayer, actorId, grid.X, grid.Y))
            return;
        InvalidateVisual();
    }

    private void writeTileSelection((int X, int Y) grid)
    {
        writeTileRectangle(grid, grid);
    }

    private void writeTileRectangle((int X, int Y) start, (int X, int Y) end)
    {
        if (!canEditMap || selectedLayerName is null || editingContext is null || CurrentMapKey is null
            || CurrentMapData?["layers"]?[selectedLayerName] is not JsonObject layer)
            return;
        List<MapTileEdit> cells = [];
        int columns = getTilesetColumnCount(layer);
        for (int y = Math.Min(start.Y, end.Y); y <= Math.Max(start.Y, end.Y); y++)
        for (int x = Math.Min(start.X, end.X); x <= Math.Max(start.X, end.X); x++)
        {
            if (selectedTiles is { } tiles)
            {
                for (int py = 0; py < tiles.Height; py++)
                for (int px = 0; px < tiles.Width; px++)
                    cells.Add(new MapTileEdit(x + px, y + py, tiles.OriginTileNumber + py * columns + px, null));
            }
            else
                cells.Add(new MapTileEdit(x, y, null, selectedAutoTileKey));
        }
        editingContext.PaintMapCells(CurrentMapKey, selectedLayerName, cells);
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

    private void fillTileRegion((int X, int Y) start)
    {
        if (!canEditMap || selectedLayerName is null || editingContext is null || CurrentMapKey is null
            || CurrentMapData?["layers"]?[selectedLayerName] is not JsonObject layer
            || !tryGetMapSize(out int width, out int height)
            || start.X < 0 || start.Y < 0 || start.X >= width || start.Y >= height)
            return;
        JsonArray? tiles = layer["tiles"] as JsonArray;
        JsonArray? autoTiles = layer["autoTiles"] as JsonArray;
        JsonNode? sourceTile = getCell(tiles, start.X, start.Y);
        JsonNode? sourceAutoTile = getCell(autoTiles, start.X, start.Y);
        bool[,] visited = new bool[height, width];
        Queue<(int X, int Y)> pending = new();
        List<MapTileEdit> cells = [];
        int columns = getTilesetColumnCount(layer);
        pending.Enqueue(start);
        while (pending.TryDequeue(out (int X, int Y) cell))
        {
            if (cell.X < 0 || cell.Y < 0 || cell.X >= width || cell.Y >= height || visited[cell.Y, cell.X])
                continue;
            visited[cell.Y, cell.X] = true;
            if (!JsonNode.DeepEquals(sourceTile, getCell(tiles, cell.X, cell.Y))
                || !JsonNode.DeepEquals(sourceAutoTile, getCell(autoTiles, cell.X, cell.Y)))
                continue;
            if (selectedTiles is { } selection)
            {
                int x = ((cell.X - start.X) % selection.Width + selection.Width) % selection.Width;
                int y = ((cell.Y - start.Y) % selection.Height + selection.Height) % selection.Height;
                cells.Add(new MapTileEdit(cell.X, cell.Y, selection.OriginTileNumber + y * columns + x, null));
            }
            else
                cells.Add(new MapTileEdit(cell.X, cell.Y, null, selectedAutoTileKey));
            pending.Enqueue((cell.X - 1, cell.Y));
            pending.Enqueue((cell.X + 1, cell.Y));
            pending.Enqueue((cell.X, cell.Y - 1));
            pending.Enqueue((cell.X, cell.Y + 1));
        }
        editingContext.PaintMapCells(CurrentMapKey, selectedLayerName, cells);
    }

    private void showEditFeedback(string key)
    {
        EditFeedbackRequested?.Invoke(this, LocaleService.Get(key));
    }

    private void placeActor(string reference, (int X, int Y) grid)
    {
        if (IsRuntimeEditing || !canEditMap || selectedLayerName is null || gameData is null || CurrentMapKey is null)
            return;
        int? index = gameData.AddMapActor(CurrentMapKey, selectedLayerName,
            new JsonObject { ["bp"] = reference }, grid.X, grid.Y);
        if (index is not null)
            setSelectedActor(selectedLayerName, index, true);
    }

    private void pasteActor((int X, int Y) grid)
    {
        if (IsRuntimeEditing || !canEditMap || actorClipboard is null || selectedLayerName is null || gameData is null || CurrentMapKey is null)
            return;
        int? index = gameData.AddMapActor(CurrentMapKey, selectedLayerName, actorClipboard, grid.X, grid.Y, actorClassVarChangesClipboard);
        if (index is not null)
            setSelectedActor(selectedLayerName, index, true);
        InvalidateVisual();
    }

    private void deleteSelectedActor()
    {
        if (!canEditMap || selectedActorLayer is null || selectedActorIndex is null
            || getSelectedActor() is not JsonObject actor || editingContext is null || CurrentMapKey is null)
            return;
        string actorId = IsRuntimeEditing ? selectedRuntimeActorId ?? string.Empty : actor["tag"]?.GetValue<string>() ?? string.Empty;
        if (actorId.Length != 0 && editingContext.DeleteActor(CurrentMapKey, selectedActorLayer, actorId))
            setSelectedActor(null, null, true);
        InvalidateVisual();
    }

    private void showActorContextMenu((int X, int Y) grid)
    {
        MenuItem copy = new() { Header = LocaleService.Get("COPY"), IsEnabled = getSelectedActor() is not null };
        copy.Click += (_, _) => copySelectedActorToClipboard();
        MenuItem paste = new() { Header = LocaleService.Get("PASTE"), IsEnabled = !IsRuntimeEditing && canEditMap && actorClipboard is not null && selectedLayerName is not null && !hasActorAt(selectedLayerName, grid) };
        paste.Click += (_, _) => pasteActor(grid);
        MenuItem delete = new() { Header = LocaleService.Get("DELETE"), IsEnabled = canEditMap && getSelectedActor() is not null };
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
        if (layerName is not null && index is int actorIndex && getActorList(layerName) is JsonArray actors
            && actorIndex >= 0 && actorIndex < actors.Count)
            actor = actors[actorIndex] as JsonObject;
        if (actor is null)
        {
            layerName = null;
            index = null;
        }
        bool changed = !string.Equals(selectedActorLayer, layerName, StringComparison.Ordinal) || selectedActorIndex != index;
        if (changed && (actorPropertyDrag is not null || propertyWheelTarget is not null))
            cancelMapGesture();
        selectedActorLayer = layerName;
        selectedActorIndex = index;
        selectedRuntimeActorId = IsRuntimeEditing ? actor?["runtimeId"]?.GetValue<string>() : null;
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
