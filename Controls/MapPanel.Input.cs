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
    protected override Size MeasureOverride(Size availableSize)
    {
        if (!tryGetMapSize(out int mapWidth, out int mapHeight))
            return new Size(0, 0);
        return new Size(snapToDevicePixel(mapWidth * tileSize), snapToDevicePixel(mapHeight * tileSize));
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnAttachedToVisualTree(e);
        LayoutUpdated += onLayoutUpdated;
        bindHostScrollViewer(this.FindAncestorOfType<ScrollViewer>());
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
    {
        LayoutUpdated -= onLayoutUpdated;
        pendingMapZoomAnchor = null;
        bindHostScrollViewer(null);
        base.OnDetachedFromVisualTree(e);
    }

    public override void Render(DrawingContext context)
    {
        if (CurrentMapData is null || gameData is null || autoTileRenderer is null || !tryGetMapSize(out int mapWidth, out int mapHeight))
            return;

        Rect mapRect = getMapRect(mapWidth, mapHeight);
        Rect visibleMapRect = getVisibleLocalMapRect(mapRect);
        if (visibleMapRect.Width <= 0 || visibleMapRect.Height <= 0)
            return;
        ensureRenderCaches(mapWidth, mapHeight, visibleMapRect);
        using (context.PushClip(mapRect))
        using (context.PushTransform(Matrix.CreateTranslation(mapRect.X, mapRect.Y)))
        {
            drawCheckerboard(context);
            JsonObject? layers = CurrentMapData["layers"] as JsonObject;
            if (layers is not null)
            {
                foreach (KeyValuePair<string, JsonNode?> entry in layers)
                {
                    if (entry.Value is not JsonObject layer || !isLayerVisible(layer))
                        continue;
                    double opacity = selectedLayerName is null || entry.Key == selectedLayerName ? 1.0 : OtherLayerOpacity;
                    using (context.PushOpacity(opacity))
                    {
                        drawLayer(context, entry.Key);
                        layerShaderRenderer?.renderLayer(context, layer, getLocalMapRect(mapWidth, mapHeight));
                        drawActors(context, entry.Key);
                    }
                }
            }

            if (EditMode == MapEditMode.Light)
                drawLightOverlay(context);
            drawHoverAndPlacement(context);
        }
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        Focus();
        mapEditSnapshotRecorded = false;
        if (CurrentMapData is null || gameData is null || !tryGetMapSize(out int width, out int height))
            return;

        PointerPoint point = args.GetCurrentPoint(this);
        Point position = point.Position;
        if (EditMode == MapEditMode.Light)
        {
            handleLightPointerPressed(args, position, width, height);
            return;
        }

        if (getGridPosition(position, width, height) is not { } grid)
            return;
        hoverGrid = grid;
        if (EditMode == MapEditMode.Actor)
        {
            handleActorPointerPressed(args, grid);
            return;
        }

        if (selectedLayerName is null || !point.Properties.IsLeftButtonPressed && !point.Properties.IsRightButtonPressed)
            return;
        if (point.Properties.IsRightButtonPressed)
        {
            pickTileAt(grid);
            args.Handled = true;
            return;
        }
        if (!selectedLayerEditable)
        {
            showEditFeedback("LAYER_NOT_EDITABLE");
            args.Handled = true;
            return;
        }
        if (args.KeyModifiers.HasFlag(KeyModifiers.Shift))
        {
            rectangleStart = grid;
            args.Pointer.Capture(this);
            args.Handled = true;
            return;
        }
        tileBrushDragging = true;
        args.Pointer.Capture(this);
        writeTileSelection(grid);
        args.Handled = true;
    }

    protected override void OnPointerMoved(PointerEventArgs args)
    {
        base.OnPointerMoved(args);
        if (CurrentMapData is null || !tryGetMapSize(out int width, out int height))
            return;
        Point position = args.GetPosition(this);
        (int X, int Y)? grid = getGridPosition(position, width, height);
        if (grid != hoverGrid)
        {
            hoverGrid = grid;
            InvalidateVisual();
        }
        if (EditMode == MapEditMode.Light)
        {
            updateLightDrag(position, width, height);
            return;
        }
        if (grid is null)
            return;
        if (EditMode == MapEditMode.Actor)
        {
            moveSelectedActor(grid.Value);
            return;
        }
        if (tileBrushDragging && args.GetCurrentPoint(this).Properties.IsLeftButtonPressed && rectangleStart is null)
            writeTileSelection(grid.Value);
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs args)
    {
        base.OnPointerReleased(args);
        if (CurrentMapData is not null && tryGetMapSize(out int width, out int height))
        {
            (int X, int Y)? grid = getGridPosition(args.GetPosition(this), width, height);
            if (EditMode == MapEditMode.Tile && rectangleStart is not null && grid is not null)
                writeTileRectangle(rectangleStart.Value, grid.Value);
        }
        rectangleStart = null;
        tileBrushDragging = false;
        lightMoveDragging = false;
        lightRadiusDragging = false;
        actorMoveIndex = null;
        actorMoveLayer = null;
        mapEditSnapshotRecorded = false;
        args.Pointer.Capture(null);
        InvalidateVisual();
    }

    protected override void OnPointerExited(PointerEventArgs args)
    {
        base.OnPointerExited(args);
        if (!tileBrushDragging
            && rectangleStart is null
            && !lightMoveDragging
            && !lightRadiusDragging
            && actorMoveIndex is null)
        {
            hoverGrid = null;
            InvalidateVisual();
        }
    }

    protected override void OnKeyDown(KeyEventArgs args)
    {
        base.OnKeyDown(args);
        bool primary = EditorShortcuts.HasPrimaryModifier(args.KeyModifiers);
        if (primary && args.Key == Key.C && EditMode == MapEditMode.Actor)
        {
            copySelectedActorToClipboard();
            args.Handled = true;
            return;
        }
        if (primary && args.Key == Key.V && EditMode == MapEditMode.Actor && hoverGrid is { } pasteGrid)
        {
            pasteActor(pasteGrid);
            args.Handled = true;
            return;
        }
        if (args.Key == Key.Delete && EditMode == MapEditMode.Actor)
        {
            deleteSelectedActor();
            args.Handled = true;
            return;
        }
        if (args.Key == Key.Delete && EditMode == MapEditMode.Light)
        {
            deleteSelectedLight();
            args.Handled = true;
        }
    }

    protected override void OnPointerWheelChanged(PointerWheelEventArgs args)
    {
        base.OnPointerWheelChanged(args);
        if (zoomInput.ShouldSuppressWheel())
        {
            args.Handled = true;
            return;
        }
        if (!EditorZoomInput.ShouldZoomWheel(args.KeyModifiers, true))
            return;
        int steps = args.Delta.Y > 0 ? 1 : args.Delta.Y < 0 ? -1 : 0;
        if (steps == 0)
            return;
        setTileSize(
            Math.Clamp(tileSize + steps * TileSizeStep, MinTileSize, MaxTileSize),
            args.GetPosition(this),
            getZoomViewportPoint(args));
        args.Handled = true;
    }

    private void onPointerTouchPadGestureMagnify(
        object? sender,
        PointerDeltaEventArgs args)
    {
        if (!EditorZoomInput.IsMacOS)
            return;
        zoomInput.MarkMagnify();
        double nextTileSize = EditorZoomInput.ScaleByFactor(
            continuousTileSize,
            EditorZoomInput.GetMagnifyFactor(args.Delta.Y),
            MinTileSize,
            MaxTileSize);
        setTileSize(
            nextTileSize,
            args.GetPosition(this),
            getZoomViewportPoint(args));
        args.Handled = true;
    }

    private Point getZoomViewportPoint(PointerEventArgs args)
    {
        return hostScrollViewer is null
            ? args.GetPosition(this)
            : args.GetPosition(hostScrollViewer);
    }

    private void setTileSize(
        double nextContinuousTileSize,
        Point contentPoint,
        Point viewportPoint)
    {
        MapZoomAnchor? nextAnchor = null;
        if (tryGetMapSize(out int mapWidth, out int mapHeight))
        {
            Rect mapRect = getMapRect(mapWidth, mapHeight);
            nextAnchor = new MapZoomAnchor(
                (contentPoint.X - mapRect.X) / tileSize,
                (contentPoint.Y - mapRect.Y) / tileSize,
                viewportPoint);
        }
        continuousTileSize = Math.Clamp(
            nextContinuousTileSize,
            MinTileSize,
            MaxTileSize);
        int nextTileSize = Math.Clamp(
            (int)Math.Round(
                continuousTileSize,
                MidpointRounding.AwayFromZero),
            MinTileSize,
            MaxTileSize);
        if (nextTileSize == tileSize)
            return;
        tileSize = nextTileSize;
        pendingMapZoomAnchor = nextAnchor;
        disposeMapRenderCaches();
        InvalidateMeasure();
        InvalidateVisual();
    }

    private void onLayoutUpdated(object? sender, EventArgs args)
    {
        if (pendingMapZoomAnchor is null)
            return;
        applyMapZoomAnchor();
    }

    private void applyMapZoomAnchor()
    {
        if (pendingMapZoomAnchor is not MapZoomAnchor anchor
            || hostScrollViewer is null
            || !tryGetMapSize(out int mapWidth, out int mapHeight))
        {
            pendingMapZoomAnchor = null;
            return;
        }
        pendingMapZoomAnchor = null;
        Rect mapRect = getMapRect(mapWidth, mapHeight);
        Point contentAnchor = new(
            mapRect.X + anchor.MapX * tileSize,
            mapRect.Y + anchor.MapY * tileSize);
        hostScrollViewer.Offset = EditorZoomInput.GetAnchoredOffset(
            contentAnchor,
            anchor.ViewportPoint,
            hostScrollViewer.Extent,
            hostScrollViewer.Viewport);
    }

}

