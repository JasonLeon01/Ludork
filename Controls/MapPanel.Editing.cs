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
        if (!selectedLayerEditable || actorMoveIndex is not int index || actorMoveLayer is null || getActorList(actorMoveLayer) is not JsonArray actors || index < 0 || index >= actors.Count || actors[index] is not JsonObject actor)
            return;
        if (tryGetActorPosition(actor, out int oldX, out int oldY) && oldX == grid.X && oldY == grid.Y)
            return;
        if (gameData is null || CurrentMapKey is null
            || !gameData.MoveMapActor(CurrentMapKey, actorMoveLayer, index, actor["tag"]?.GetValue<string>() ?? string.Empty, grid.X, grid.Y))
            return;
        InvalidateVisual();
    }

    private void writeTileSelection((int X, int Y) grid)
    {
        writeTileRectangle(grid, grid);
    }

    private void writeTileRectangle((int X, int Y) start, (int X, int Y) end)
    {
        if (!selectedLayerEditable || selectedLayerName is null || gameData is null || CurrentMapKey is null
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
        gameData.PaintMapCells(CurrentMapKey, selectedLayerName, cells);
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
        if (!selectedLayerEditable || selectedLayerName is null || gameData is null || CurrentMapKey is null)
            return;
        int? index = gameData.AddMapActor(CurrentMapKey, selectedLayerName,
            new JsonObject { ["bp"] = reference }, grid.X, grid.Y);
        if (index is not null)
            setSelectedActor(selectedLayerName, index, true);
    }

    private void pasteActor((int X, int Y) grid)
    {
        if (!selectedLayerEditable || actorClipboard is null || selectedLayerName is null || gameData is null || CurrentMapKey is null)
            return;
        int? index = gameData.AddMapActor(CurrentMapKey, selectedLayerName, actorClipboard, grid.X, grid.Y, actorClassVarChangesClipboard);
        if (index is not null)
            setSelectedActor(selectedLayerName, index, true);
        InvalidateVisual();
    }

    private void deleteSelectedActor()
    {
        if (!selectedLayerEditable || selectedActorLayer is null || selectedActorIndex is not int index
            || getSelectedActor() is not JsonObject actor || gameData is null || CurrentMapKey is null)
            return;
        if (gameData.DeleteMapActor(CurrentMapKey, selectedActorLayer, index, actor["tag"]?.GetValue<string>() ?? string.Empty))
            setSelectedActor(null, null, true);
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
        if (layerName is not null && index is int actorIndex && getActorList(layerName) is JsonArray actors
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

