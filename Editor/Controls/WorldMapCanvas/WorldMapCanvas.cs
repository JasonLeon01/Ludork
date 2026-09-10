using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class WorldMapCanvas : Control, IDisposable
{
    internal const string ChildDragPrefix = "LUDORK_WORLD_CHILD:";
    private static readonly IBrush OutsideBrush = new SolidColorBrush(Color.Parse("#111111"));
    private static readonly IBrush WorldBrush = new SolidColorBrush(Color.Parse("#202020"));
    private static readonly IBrush HoleBrush = new SolidColorBrush(Color.Parse("#171717"));
    private static readonly Pen WorldBorderPen = new(new SolidColorBrush(Color.Parse("#777777")), 1);
    private static readonly Pen GridPen = new(new SolidColorBrush(Color.FromArgb(35, 255, 255, 255)), 1);
    private static readonly Pen PlacementPen = new(new SolidColorBrush(Color.FromArgb(220, 240, 240, 240)), 1);
    private static readonly Pen SelectedPlacementPen = new(new SolidColorBrush(Color.Parse("#ffd54f")), 2);
    private static readonly IBrush ValidGhostBrush = new SolidColorBrush(Color.FromArgb(90, 80, 220, 110));
    private static readonly IBrush InvalidGhostBrush = new SolidColorBrush(Color.FromArgb(120, 235, 70, 70));
    private static readonly Pen ValidGhostPen = new(new SolidColorBrush(Color.Parse("#66dd88")), 2);
    private static readonly Pen InvalidGhostPen = new(new SolidColorBrush(Color.Parse("#ef5350")), 2);
    private static readonly IBrush SelectedCellBrush = new SolidColorBrush(Color.FromArgb(80, 255, 213, 79));
    private static readonly Pen SelectedCellPen = new(new SolidColorBrush(Color.Parse("#ffd54f")), 2);
    private readonly List<WorldMapPlacementPreview> placements = [];
    private readonly Dictionary<string, WorldMapChildSource> childMaps = new(StringComparer.Ordinal);
    private readonly Dictionary<string, MapCatalogEntry> childCatalog = new(StringComparer.Ordinal);
    private readonly Dictionary<string, bool> childLayerOrderValidity = new(StringComparer.Ordinal);
    private readonly WorldMapValidationService worldMapValidation = new();
    private WorldMapPreviewRenderer? renderer;
    private bool ownsRenderer;
    private bool disposed;
    private WorldMapPlacementPreview? selectedPlacement;
    private WorldMapPlacementPreview? movingPlacement;
    private Vector movingOffset;
    private WorldMapGhost? ghost;

    public WorldMapCanvas()
    {
        Focusable = true;
        ClipToBounds = true;
        DragDrop.SetAllowDrop(this, true);
        AddHandler(DragDrop.DragOverEvent, onDragOver);
        AddHandler(DragDrop.DropEvent, onDrop);
        AddHandler(DragDrop.DragLeaveEvent, onDragLeave);
        PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
    }

    public event EventHandler<WorldMapPlacementChangedEventArgs>? PlacementChanged;
    public event EventHandler<WorldMapPlacementRemovedEventArgs>? PlacementRemoved;
    public event EventHandler<WorldMapCellSelectedEventArgs>? WorldCellSelected;

    public bool PlacementEditingEnabled { get; set; } = true;
    public string? WorldKey { get; private set; }
    public JsonObject? Manifest { get; private set; }
    public int WorldWidth { get; private set; }
    public int WorldHeight { get; private set; }
    public (int X, int Y)? SelectedWorldCell { get; private set; }

    public void Configure(GameDataService gameData)
    {
        setRenderer(new WorldMapPreviewRenderer(gameData), true);
    }

    internal void Configure(WorldMapPreviewRenderer sharedRenderer)
    {
        setRenderer(sharedRenderer, false);
    }

    public void SetWorld(
        string? worldKey,
        JsonObject? manifest,
        IReadOnlyList<WorldMapChildSource> children)
    {
        int nextWorldWidth = getInt(manifest?["width"]);
        int nextWorldHeight = getInt(manifest?["height"]);
        bool preserveView = string.Equals(WorldKey, worldKey, StringComparison.Ordinal)
            && WorldWidth == nextWorldWidth
            && WorldHeight == nextWorldHeight;
        foreach (WorldMapChildSource child in childMaps.Values)
            child.ReleaseData();
        WorldKey = worldKey;
        Manifest = manifest;
        WorldWidth = nextWorldWidth;
        WorldHeight = nextWorldHeight;
        childMaps.Clear();
        childCatalog.Clear();
        foreach (WorldMapChildSource child in children)
        {
            childMaps[child.Key] = child;
            childCatalog[child.Key] = new MapCatalogEntry(
                child.Key,
                child.DisplayName,
                MapCatalogEntryKind.WorldChildMap,
                worldKey,
                child.Width,
                child.Height,
                child.LayerOrder,
                []);
        }
        placements.Clear();
        if (!string.IsNullOrWhiteSpace(worldKey) && manifest?["placements"] is JsonArray placementData)
        {
            foreach (JsonNode? node in placementData)
            {
                if (tryParsePlacement(worldKey, node as JsonObject, out WorldMapPlacementPreview? placement)
                    && placement is not null)
                {
                    placements.Add(placement);
                }
            }
        }
        rebuildLayerOrderValidity();
        selectedPlacement = selectedPlacement is null
            ? null
            : placements.FirstOrDefault(item => item.Child.Key == selectedPlacement.Child.Key);
        movingPlacement = null;
        ghost = null;
        if (!preserveView)
        {
            SelectedWorldCell = null;
            resetViewport();
        }
        InvalidateMeasure();
        InvalidateVisual();
    }

    public (int X, int Y)? WorldCellAt(Point position)
    {
        if (WorldWidth <= 0 || WorldHeight <= 0 || cellSize <= 0)
            return null;
        Point world = screenToWorld(position);
        int x = (int)Math.Floor(world.X);
        int y = (int)Math.Floor(world.Y);
        return x >= 0 && y >= 0 && x < WorldWidth && y < WorldHeight ? (x, y) : null;
    }

    public void SelectWorldCell(int? x, int? y)
    {
        if (x is int cellX
            && y is int cellY
            && cellX >= 0
            && cellY >= 0
            && cellX < WorldWidth
            && cellY < WorldHeight)
        {
            SelectedWorldCell = (cellX, cellY);
        }
        else
        {
            SelectedWorldCell = null;
        }
        InvalidateVisual();
    }

    public void SetSelectedWorldCell(JsonNode? value)
    {
        if (value is JsonArray { Count: >= 2 } position)
            SelectWorldCell(getNullableInt(position[0]), getNullableInt(position[1]));
        else
            SelectWorldCell(null, null);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        disposeViewport();
        setRenderer(null, false);
    }

    public override void Render(DrawingContext context)
    {
        Rect visibleCanvas = getVisibleCanvasRect();
        context.FillRectangle(OutsideBrush, visibleCanvas);
        if (WorldWidth <= 0 || WorldHeight <= 0)
            return;
        Rect worldRect = worldToScreen(new Rect(0, 0, WorldWidth, WorldHeight));
        context.FillRectangle(HoleBrush, worldRect);
        WorldMapPlacementPreview[] visiblePlacements = placements
            .Where(placement => worldToScreen(new Rect(
                    placement.X,
                    placement.Y,
                    placement.Width,
                    placement.Height))
                .Intersects(visibleCanvas))
            .ToArray();
        bool drawDetailedMaps = cellSize >= 1 && visiblePlacements.Length <= 32;
        HashSet<string> pinnedMaps = drawDetailedMaps
            ? visiblePlacements.Select(placement => placement.Child.Key).ToHashSet(StringComparer.Ordinal)
            : new HashSet<string>(StringComparer.Ordinal);
        foreach (WorldMapPlacementPreview placement in placements)
        {
            if (!pinnedMaps.Contains(placement.Child.Key))
                placement.Child.ReleaseData();
        }
        renderer?.TrimMapCache(pinnedMaps);
        int remainingLoads = 1;
        using (context.PushClip(worldRect.Intersect(visibleCanvas)))
        {
            List<(WorldMapPlacementPreview Placement, JsonObject Map)> detailedPlacements = [];
            foreach (WorldMapPlacementPreview placement in visiblePlacements)
            {
                if (movingPlacement == placement)
                    continue;
                Rect placementRect = getPlacementRect(placement);
                context.FillRectangle(WorldBrush, placementRect);
                if (drawDetailedMaps
                    && placement.Child.HasData
                    && placement.Child.LoadData() is JsonObject map)
                {
                    detailedPlacements.Add((placement, map));
                }
                if (drawDetailedMaps && !placement.Child.HasData)
                {
                    if (remainingLoads > 0)
                    {
                        remainingLoads -= 1;
                        placement.Child.ScheduleLoad(InvalidateVisual);
                    }
                }
            }
            IReadOnlyList<string> layerOrder = getWorldLayerOrder(visiblePlacements);
            if (renderer is not null)
            {
                foreach (string layerName in layerOrder)
                {
                    foreach ((WorldMapPlacementPreview placement, JsonObject map) in detailedPlacements)
                    {
                        renderer.DrawMapLayer(
                            context,
                            placement.Child.Key,
                            map,
                            layerName,
                            getPlacementRect(placement).TopLeft,
                            cellSize,
                            visibleCanvas);
                    }
                }
                HashSet<string> renderedActorGroups = layerOrder.ToHashSet(StringComparer.Ordinal);
                foreach ((WorldMapPlacementPreview placement, JsonObject map) in detailedPlacements)
                {
                    renderer.DrawRemainingActors(
                        context,
                        placement.Child.Key,
                        map,
                        renderedActorGroups,
                        getPlacementRect(placement).TopLeft,
                        cellSize,
                        visibleCanvas);
                }
                renderer.CompleteFrame();
            }
            foreach (WorldMapPlacementPreview placement in visiblePlacements)
            {
                if (movingPlacement != placement)
                    drawPlacementOverlay(context, placement);
            }
            drawGrid(context, worldRect, visibleCanvas);
            drawSelectedCell(context);
            if (ghost is not null)
                drawGhost(context, ghost);
        }
        context.DrawRectangle(null, WorldBorderPen, worldRect);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        PointerPoint point = args.GetCurrentPoint(this);
        Point position = args.GetPosition(this);
        Focus();
        if (tryStartPanning(args, point))
            return;
        WorldMapPlacementPreview? hit = hitTestPlacement(position);
        if (point.Properties.IsRightButtonPressed && PlacementEditingEnabled && hit is not null)
        {
            selectedPlacement = hit;
            showPlacementContextMenu(hit);
            InvalidateVisual();
            args.Handled = true;
            return;
        }
        if (!point.Properties.IsLeftButtonPressed)
            return;
        if (WorldCellAt(position) is { } cell)
        {
            SelectedWorldCell = cell;
            WorldCellSelected?.Invoke(this, new WorldMapCellSelectedEventArgs(cell.X, cell.Y));
        }
        if (!PlacementEditingEnabled || hit is null)
        {
            selectedPlacement = hit;
            InvalidateVisual();
            args.Handled = true;
            return;
        }
        selectedPlacement = hit;
        movingPlacement = hit;
        Point worldPosition = screenToWorld(position);
        movingOffset = new Vector(worldPosition.X - hit.X, worldPosition.Y - hit.Y);
        ghost = new WorldMapGhost(hit.Child, hit.X, hit.Y, isPlacementValid(hit.Child, hit.X, hit.Y, hit));
        args.Pointer.Capture(this);
        InvalidateVisual();
        args.Handled = true;
    }

    protected override void OnPointerMoved(PointerEventArgs args)
    {
        base.OnPointerMoved(args);
        Point position = args.GetPosition(this);
        if (tryUpdatePanning(args))
            return;
        if (movingPlacement is null)
            return;
        Point world = screenToWorld(position);
        int x = (int)Math.Floor(world.X - movingOffset.X);
        int y = (int)Math.Floor(world.Y - movingOffset.Y);
        ghost = new WorldMapGhost(
            movingPlacement.Child,
            x,
            y,
            isPlacementValid(movingPlacement.Child, x, y, movingPlacement));
        InvalidateVisual();
        args.Handled = true;
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs args)
    {
        base.OnPointerReleased(args);
        if (tryStopPanning(args))
            return;
        if (movingPlacement is null)
            return;
        WorldMapPlacementPreview moving = movingPlacement;
        WorldMapGhost? releaseGhost = ghost;
        movingPlacement = null;
        ghost = null;
        args.Pointer.Capture(null);
        if (releaseGhost is { IsValid: true }
            && (releaseGhost.X != moving.X || releaseGhost.Y != moving.Y)
            && WorldKey is not null)
        {
            PlacementChanged?.Invoke(
                this,
                new WorldMapPlacementChangedEventArgs(
                    WorldKey,
                    moving.Child.Key,
                    releaseGhost.X,
                    releaseGhost.Y));
        }
        InvalidateVisual();
        args.Handled = true;
    }

    protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs args)
    {
        base.OnPointerCaptureLost(args);
        cancelPanning();
        movingPlacement = null;
        ghost = null;
        InvalidateVisual();
    }

    protected override void OnKeyDown(KeyEventArgs args)
    {
        base.OnKeyDown(args);
        if (!PlacementEditingEnabled
            || args.Key is not (Key.Delete or Key.Back)
            || selectedPlacement is null
            || WorldKey is null)
        {
            return;
        }
        PlacementRemoved?.Invoke(
            this,
            new WorldMapPlacementRemovedEventArgs(WorldKey, selectedPlacement.Child.Key));
        args.Handled = true;
    }

    private void onDragOver(object? sender, DragEventArgs args)
    {
        if (!PlacementEditingEnabled || tryGetDraggedChild(args) is not WorldMapChildSource child)
        {
            ghost = null;
            args.DragEffects = DragDropEffects.None;
            args.Handled = true;
            InvalidateVisual();
            return;
        }
        Point world = screenToWorld(args.GetPosition(this));
        int x = (int)Math.Floor(world.X);
        int y = (int)Math.Floor(world.Y);
        bool valid = isPlacementValid(child, x, y, getPlacement(child.Key));
        ghost = new WorldMapGhost(child, x, y, valid);
        args.DragEffects = valid ? DragDropEffects.Copy : DragDropEffects.None;
        args.Handled = true;
        InvalidateVisual();
    }

    private void onDrop(object? sender, DragEventArgs args)
    {
        WorldMapGhost? dropGhost = ghost;
        ghost = null;
        if (dropGhost is { IsValid: true } && WorldKey is not null)
        {
            PlacementChanged?.Invoke(
                this,
                new WorldMapPlacementChangedEventArgs(
                    WorldKey,
                    dropGhost.Child.Key,
                    dropGhost.X,
                    dropGhost.Y));
        }
        args.Handled = true;
        InvalidateVisual();
    }

    private void onDragLeave(object? sender, RoutedEventArgs args)
    {
        ghost = null;
        InvalidateVisual();
    }

    private void drawPlacementOverlay(
        DrawingContext context,
        WorldMapPlacementPreview placement)
    {
        Rect rect = getPlacementRect(placement);
        context.DrawRectangle(
            null,
            selectedPlacement == placement ? SelectedPlacementPen : PlacementPen,
            rect);
        if (cellSize < 3 || rect.Width < 40 || rect.Height < 18)
            return;
        FormattedText label = new(
            placement.Child.DisplayName,
            CultureInfo.CurrentCulture,
            FlowDirection.LeftToRight,
            Typeface.Default,
            12,
            Brushes.White);
        Rect labelBackground = new(
            rect.Left + 3,
            rect.Top + 3,
            Math.Min(rect.Width - 6, label.Width + 8),
            Math.Min(rect.Height - 6, label.Height + 4));
        if (labelBackground.Width <= 0 || labelBackground.Height <= 0)
            return;
        context.FillRectangle(new SolidColorBrush(Color.FromArgb(190, 20, 20, 20)), labelBackground);
        using (context.PushClip(labelBackground))
            context.DrawText(label, new Point(labelBackground.X + 4, labelBackground.Y + 2));
    }

    private Rect getPlacementRect(WorldMapPlacementPreview placement)
    {
        return worldToScreen(new Rect(
            placement.X,
            placement.Y,
            placement.Width,
            placement.Height));
    }

    private IReadOnlyList<string> getWorldLayerOrder(
        IReadOnlyList<WorldMapPlacementPreview> visiblePlacements)
    {
        List<string> result = [];
        if (Manifest?["layerOrder"] is JsonArray manifestOrder)
        {
            foreach (JsonNode? node in manifestOrder)
            {
                string? layerName = getString(node);
                if (!string.IsNullOrWhiteSpace(layerName)
                    && !result.Contains(layerName, StringComparer.Ordinal))
                {
                    result.Add(layerName);
                }
            }
        }
        foreach (WorldMapPlacementPreview placement in visiblePlacements)
        {
            foreach (string layerName in placement.Child.LayerOrder)
            {
                if (!result.Contains(layerName, StringComparer.Ordinal))
                    result.Add(layerName);
            }
        }
        return result;
    }

    private void drawGhost(DrawingContext context, WorldMapGhost value)
    {
        Rect rect = worldToScreen(new Rect(value.X, value.Y, getMapWidth(value.Child), getMapHeight(value.Child)));
        context.FillRectangle(value.IsValid ? ValidGhostBrush : InvalidGhostBrush, rect);
        context.DrawRectangle(null, value.IsValid ? ValidGhostPen : InvalidGhostPen, rect);
    }

    private void drawGrid(
        DrawingContext context,
        Rect worldRect,
        Rect visibleCanvas)
    {
        if (cellSize < 8)
            return;
        int minX = Math.Clamp(
            (int)Math.Floor((visibleCanvas.Left - worldRect.Left) / cellSize),
            0,
            WorldWidth);
        int minY = Math.Clamp(
            (int)Math.Floor((visibleCanvas.Top - worldRect.Top) / cellSize),
            0,
            WorldHeight);
        int maxX = Math.Clamp(
            (int)Math.Ceiling((visibleCanvas.Right - worldRect.Left) / cellSize),
            0,
            WorldWidth);
        int maxY = Math.Clamp(
            (int)Math.Ceiling((visibleCanvas.Bottom - worldRect.Top) / cellSize),
            0,
            WorldHeight);
        for (int x = minX; x <= maxX; x++)
        {
            double position = worldRect.Left + x * cellSize;
            context.DrawLine(GridPen, new Point(position, worldRect.Top), new Point(position, worldRect.Bottom));
        }
        for (int y = minY; y <= maxY; y++)
        {
            double position = worldRect.Top + y * cellSize;
            context.DrawLine(GridPen, new Point(worldRect.Left, position), new Point(worldRect.Right, position));
        }
    }

    private void drawSelectedCell(DrawingContext context)
    {
        if (SelectedWorldCell is not { } cell)
            return;
        Rect rect = worldToScreen(new Rect(cell.X, cell.Y, 1, 1));
        context.FillRectangle(SelectedCellBrush, rect);
        context.DrawRectangle(null, SelectedCellPen, rect);
    }

    private void showPlacementContextMenu(WorldMapPlacementPreview placement)
    {
        MenuItem remove = new() { Header = LocaleService.Get("WORLD_REMOVE_PLACEMENT") };
        remove.Click += (_, _) =>
        {
            if (WorldKey is not null)
                PlacementRemoved?.Invoke(this, new WorldMapPlacementRemovedEventArgs(WorldKey, placement.Child.Key));
        };
        new ContextMenu { ItemsSource = new object[] { remove } }.Open(this);
    }

    private WorldMapChildSource? tryGetDraggedChild(DragEventArgs args)
    {
        string? payload = args.DataTransfer.TryGetText();
        if (payload is null || !payload.StartsWith(ChildDragPrefix, StringComparison.Ordinal))
            return null;
        string key = payload[ChildDragPrefix.Length..];
        return childMaps.GetValueOrDefault(key);
    }

    private bool tryParsePlacement(
        string worldKey,
        JsonObject? data,
        out WorldMapPlacementPreview? placement)
    {
        if (data is null
            || data["map"] is not JsonValue mapValue
            || !mapValue.TryGetValue(out string? childPath)
            || string.IsNullOrWhiteSpace(childPath)
            || data["rect"] is not JsonArray { Count: >= 4 } rect)
        {
            placement = null;
            return false;
        }
        string childKey = WorldMapPanel.combineChildKey(worldKey, childPath);
        if (!childMaps.TryGetValue(childKey, out WorldMapChildSource? child))
        {
            placement = null;
            return false;
        }
        int x = getInt(rect[0]);
        int y = getInt(rect[1]);
        int width = getInt(rect[2]);
        int height = getInt(rect[3]);
        if (width <= 0 || height <= 0)
        {
            placement = null;
            return false;
        }
        placement = new WorldMapPlacementPreview(child, x, y, width, height);
        return true;
    }

    private bool isPlacementValid(
        WorldMapChildSource child,
        int x,
        int y,
        WorldMapPlacementPreview? ignored)
    {
        int width = getMapWidth(child);
        int height = getMapHeight(child);
        if (width <= 0
            || height <= 0
            || x < 0
            || y < 0
            || x + width > WorldWidth
            || y + height > WorldHeight)
        {
            return false;
        }
        Rect candidate = new(x, y, width, height);
        foreach (WorldMapPlacementPreview placement in placements)
        {
            if (placement == ignored)
                continue;
            if (candidate.Intersects(new Rect(placement.X, placement.Y, placement.Width, placement.Height)))
                return false;
        }
        return childLayerOrderValidity.GetValueOrDefault(child.Key);
    }

    private void rebuildLayerOrderValidity()
    {
        childLayerOrderValidity.Clear();
        if (WorldKey is null)
            return;
        List<WorldMapPlacement> current = placements
            .Select(placement => new WorldMapPlacement(
                Path.GetFileName(placement.Child.Key) + ".json",
                new WorldMapRect(
                    placement.X,
                    placement.Y,
                    placement.Width,
                    placement.Height)))
            .ToList();
        HashSet<string> placed = placements
            .Select(placement => placement.Child.Key)
            .ToHashSet(StringComparer.Ordinal);
        foreach (WorldMapChildSource child in childMaps.Values)
        {
            if (placed.Contains(child.Key))
            {
                childLayerOrderValidity[child.Key] = true;
                continue;
            }
            List<WorldMapPlacement> candidate = new(current)
            {
                new WorldMapPlacement(
                    Path.GetFileName(child.Key) + ".json",
                    new WorldMapRect(0, 0, child.Width, child.Height)),
            };
            childLayerOrderValidity[child.Key] = worldMapValidation.TryMergeLayerOrder(
                WorldKey,
                candidate,
                childCatalog) is not null;
        }
    }

    private WorldMapPlacementPreview? hitTestPlacement(Point screenPosition)
    {
        Point world = screenToWorld(screenPosition);
        for (int index = placements.Count - 1; index >= 0; index--)
        {
            WorldMapPlacementPreview placement = placements[index];
            if (new Rect(placement.X, placement.Y, placement.Width, placement.Height).Contains(world))
                return placement;
        }
        return null;
    }

    private WorldMapPlacementPreview? getPlacement(string childKey)
    {
        return placements.FirstOrDefault(item => string.Equals(item.Child.Key, childKey, StringComparison.Ordinal));
    }

    private static int getMapWidth(WorldMapChildSource child)
    {
        return Math.Max(0, child.Width);
    }

    private static int getMapHeight(WorldMapChildSource child)
    {
        return Math.Max(0, child.Height);
    }

    private static int getInt(JsonNode? value)
    {
        return value?.GetValue<int?>() ?? 0;
    }

    private static int? getNullableInt(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out int result) ? result : null;
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? result) ? result : null;
    }

    private void setRenderer(WorldMapPreviewRenderer? nextRenderer, bool ownsNextRenderer)
    {
        if (ReferenceEquals(renderer, nextRenderer) && ownsRenderer == ownsNextRenderer)
            return;
        WorldMapPreviewRenderer? previousRenderer = renderer;
        bool disposePreviousRenderer = ownsRenderer;
        if (previousRenderer is not null)
            previousRenderer.PreviewChanged -= onPreviewChanged;
        renderer = nextRenderer;
        ownsRenderer = ownsNextRenderer;
        if (renderer is not null)
            renderer.PreviewChanged += onPreviewChanged;
        if (disposePreviousRenderer)
            previousRenderer?.Dispose();
        InvalidateVisual();
    }

    private void onPreviewChanged(object? sender, EventArgs args)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => onPreviewChanged(sender, args));
            return;
        }
        if (!disposed)
            InvalidateVisual();
    }

    private sealed record WorldMapPlacementPreview(
        WorldMapChildSource Child,
        int X,
        int Y,
        int Width,
        int Height);

    private sealed record WorldMapGhost(
        WorldMapChildSource Child,
        int X,
        int Y,
        bool IsValid);
}
