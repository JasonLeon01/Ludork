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

public enum MapEditMode
{
    Tile,
    Light,
    Actor,
}

public interface IMapLayerShaderRenderer
{
    void renderLayer(DrawingContext context, JsonObject layer, Rect layerBounds);
}

public sealed class MapPanel : Control
{
    private const int SourceTileSize = 32;
    private const int MinTileSize = 8;
    private const int MaxTileSize = 128;
    private const int TileSizeStep = 4;
    private const int TileBrushRenderInterval = 16;
    private const double OtherLayerOpacity = 0.5;
    private const double LightEdgeTolerance = 8.0;

    private static readonly IBrush CheckerLightBrush = new SolidColorBrush(Color.FromRgb(220, 220, 220));
    private static readonly IBrush CheckerDarkBrush = new SolidColorBrush(Color.FromRgb(180, 180, 180));
    private static readonly IBrush MissingActorBrush = new SolidColorBrush(Color.FromArgb(160, 0, 120, 255));
    private static readonly IBrush ActorOriginBrush = new SolidColorBrush(Color.FromArgb(220, 0, 255, 0));
    private static readonly Pen HoverPen = new(new SolidColorBrush(Colors.Black), 1);
    private static readonly Pen SelectedActorPen = new(new SolidColorBrush(Color.FromRgb(255, 220, 0)), 2);

    private readonly Stopwatch animationClock = Stopwatch.StartNew();
    private readonly DispatcherTimer animationTimer;
    private readonly DispatcherTimer tileBrushRenderTimer;
    private readonly Dictionary<string, CachedBitmap> bitmapCache = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, Bitmap> hueCache = new(StringComparer.OrdinalIgnoreCase);
    private readonly List<Bitmap> retiredBitmaps = [];
    private readonly Dictionary<string, LayerRenderCache> layerRenderCaches = new(StringComparer.Ordinal);
    private readonly Dictionary<string, List<ActorRenderState>> actorRenderStates = new(StringComparer.Ordinal);
    private readonly HashSet<string> dirtyLayerNames = new(StringComparer.Ordinal);
    private readonly HashSet<string> pendingBrushLayerNames = new(StringComparer.Ordinal);
    private readonly HashSet<string> animatedAutoTileLayerNames = new(StringComparer.Ordinal);
    private readonly EditorZoomInput zoomInput = new();
    private GameDataService? gameData;
    private BlueprintPreviewService? previewService;
    private AutoTileRenderer? autoTileRenderer;
    private IMapLayerShaderRenderer? layerShaderRenderer;
    private string? selectedLayerName;
    private bool selectedLayerEditable;
    private TileSelection? selectedTiles;
    private string? selectedAutoTileKey;
    private string? pendingActor;
    private (int X, int Y)? hoverGrid;
    private (int X, int Y)? rectangleStart;
    private int tileSize = SourceTileSize;
    private double continuousTileSize = SourceTileSize;
    private bool tileBrushDragging;
    private bool mapEditSnapshotRecorded;
    private int? selectedLightIndex;
    private int? selectedActorIndex;
    private string? selectedActorLayer;
    private bool lightMoveDragging;
    private bool lightRadiusDragging;
    private Vector lightDragOffset;
    private Point lightDragCenter;
    private int? actorMoveIndex;
    private string? actorMoveLayer;
    private JsonObject? actorClipboard;
    private JsonObject? actorClassVarChangesClipboard;
    private ViewportRenderCache? checkerboardRenderCache;
    private CacheGeometry? cacheGeometry;
    private ActorRenderState? pendingActorRenderState;
    private ScrollViewer? hostScrollViewer;
    private bool actorRenderStatesDirty = true;
    private bool pendingActorRenderStateDirty = true;
    private bool animationStateDirty = true;
    private bool hasAnimatedActors;
    private bool actorPreviewActivityUpdatePending;
    private int renderedAutoTileFrame;
    private MapZoomAnchor? pendingMapZoomAnchor;

    public MapPanel()
    {
        Focusable = true;
        ClipToBounds = true;
        animationTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(100) };
        animationTimer.Tick += onAnimationTick;
        tileBrushRenderTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(TileBrushRenderInterval) };
        tileBrushRenderTimer.Tick += onTileBrushRenderTick;
        PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
        EffectiveViewportChanged += (_, _) => scheduleActorPreviewActivityUpdate();
        Unloaded += (_, _) => disposeRenderResources();
    }

    public string? CurrentMapKey { get; private set; }
    public JsonObject? CurrentMapData { get; private set; }
    public int RefreshCount { get; private set; }
    public string? PendingActor => pendingActor;
    public string? SelectedActorLayer => selectedActorLayer;
    public int? SelectedActorIndex => selectedActorIndex;
    public bool IsSelectedLayerEditable => selectedLayerEditable;
    public MapEditMode EditMode { get; private set; } = MapEditMode.Tile;
    public event EventHandler<TileSelectionChangedEventArgs>? TileSelectionPicked;
    public event EventHandler<ActorSelectionChangedEventArgs>? ActorSelectionChanged;
    public event EventHandler? ActorDataChanged;
    public event EventHandler<LightSelectionChangedEventArgs>? LightSelectionChanged;
    public event EventHandler<LightDataChangedEventArgs>? LightDataChanged;
    public event EventHandler<string>? EditFeedbackRequested;

    public void configure(GameDataService nextGameData, BlueprintPreviewService nextPreviewService)
    {
        if (ReferenceEquals(gameData, nextGameData) && ReferenceEquals(previewService, nextPreviewService))
            return;
        if (previewService is not null)
            previewService.VisualsInvalidated -= onActorVisualsInvalidated;
        disposeMapRenderCaches();
        invalidateActorRenderStates();
        invalidatePendingActorRenderState();
        disposeCachedBitmaps();
        autoTileRenderer?.Dispose();
        gameData = nextGameData;
        previewService = nextPreviewService;
        previewService.VisualsInvalidated += onActorVisualsInvalidated;
        autoTileRenderer = new AutoTileRenderer(nextGameData);
        tileSize = Math.Clamp(nextGameData.getCellSize(), MinTileSize, MaxTileSize);
        continuousTileSize = tileSize;
        InvalidateMeasure();
        InvalidateVisual();
    }

    public void setLayerShaderRenderer(IMapLayerShaderRenderer? renderer)
    {
        layerShaderRenderer = renderer;
        InvalidateVisual();
    }

    public void refreshMap(string? mapKey, JsonObject? mapData)
    {
        CurrentMapKey = mapKey;
        CurrentMapData = mapData;
        RefreshCount += 1;
        disposeMapRenderCaches();
        invalidateActorRenderStates();
        invalidatePendingActorRenderState();
        hoverGrid = null;
        rectangleStart = null;
        setSelectedLightIndex(null);
        setSelectedActor(null, null, true, true);
        InvalidateMeasure();
        InvalidateVisual();
    }

    public void setSelectedLayer(string? layerName)
    {
        selectedLayerName = layerName;
        if (!string.Equals(selectedActorLayer, layerName, StringComparison.Ordinal))
            setSelectedActor(null, null, true);
        InvalidateVisual();
    }

    public void setSelectedLayerEditable(bool editable)
    {
        selectedLayerEditable = editable;
        flushPendingBrushLayers();
        disposeMapRenderCaches();
        scheduleActorPreviewActivityUpdate();
        InvalidateVisual();
    }

    public void setTileSelection(TileSelection? tiles, string? autoTileKey)
    {
        selectedTiles = tiles;
        selectedAutoTileKey = string.IsNullOrWhiteSpace(autoTileKey) ? null : autoTileKey;
        InvalidateVisual();
    }

    public void setEditMode(MapEditMode mode)
    {
        EditMode = mode;
        rectangleStart = null;
        tileBrushDragging = false;
        flushPendingBrushLayers();
        lightMoveDragging = false;
        lightRadiusDragging = false;
        actorMoveIndex = null;
        actorMoveLayer = null;
        if (mode != MapEditMode.Actor)
            setSelectedActor(null, null, true);
        InvalidateVisual();
    }

    public void clearLightSelection()
    {
        setSelectedLightIndex(null);
    }

    public void refreshSelectedActor()
    {
        if (actorRenderStatesDirty)
        {
            InvalidateVisual();
            return;
        }
        if (selectedActorLayer is not string layerName
            || selectedActorIndex is not int index
            || getActorList(layerName, false) is not JsonArray actors
            || !actorRenderStates.TryGetValue(layerName, out List<ActorRenderState>? states)
            || actors.Count != states.Count
            || index < 0
            || index >= actors.Count
            || actors[index] is not JsonObject actor
            || !ReferenceEquals(states[index].Actor, actor))
        {
            invalidateActorRenderStates();
            InvalidateVisual();
            return;
        }
        disposeActorPreviewLease(states[index].PreviewLease);
        states[index] = createActorRenderState(actor);
        animationStateDirty = true;
        scheduleActorPreviewActivityUpdate();
        InvalidateVisual();
    }

    public bool updateSelectedActorPosition(int x, int y)
    {
        if (!selectedLayerEditable
            || getSelectedActor() is not JsonObject actor
            || !tryGetMapSize(out int width, out int height))
        {
            return false;
        }
        int nextX = Math.Clamp(x, 0, width - 1);
        int nextY = Math.Clamp(y, 0, height - 1);
        if (tryGetActorPosition(actor, out int currentX, out int currentY)
            && currentX == nextX
            && currentY == nextY)
        {
            return false;
        }
        gameData?.RecordSnapshot();
        actor["position"] = new JsonArray(nextX, nextY);
        markMapModified();
        ActorDataChanged?.Invoke(this, EventArgs.Empty);
        scheduleActorPreviewActivityUpdate();
        InvalidateVisual();
        return true;
    }

    public void selectActor(string layerName, int? actorIndex)
    {
        if (EditMode != MapEditMode.Actor
            || !string.Equals(selectedLayerName, layerName, StringComparison.Ordinal))
        {
            return;
        }
        setSelectedActor(layerName, actorIndex, true);
        InvalidateVisual();
    }

    public string makeUniqueActorTag(string tag, string? ignoreLayerName = null, int? ignoreIndex = null)
    {
        string baseTag = tag.Trim();
        if (baseTag.Length == 0)
            return string.Empty;
        string candidate = baseTag;
        int suffix = 2;
        while (actorTagExists(candidate, ignoreLayerName, ignoreIndex))
        {
            candidate = $"{baseTag}_{suffix}";
            suffix += 1;
        }
        return candidate;
    }

    public void updateSelectedLight(JsonObject lightData)
    {
        if (selectedLightIndex is not int index || CurrentMapData?["lights"] is not JsonArray lights || index < 0 || index >= lights.Count || lights[index] is not JsonObject light)
            return;
        JsonObject next = new()
        {
            ["position"] = lightData["position"]?.DeepClone(),
            ["color"] = lightData["color"]?.DeepClone(),
            ["radius"] = lightData["radius"]?.DeepClone(),
            ["intensity"] = lightData["intensity"]?.DeepClone(),
        };
        if (JsonNode.DeepEquals(light, next))
            return;
        recordMapEditSnapshot();
        lights[index] = next;
        markMapModified();
        LightDataChanged?.Invoke(this, new LightDataChangedEventArgs(CurrentMapKey ?? string.Empty, index, next));
        InvalidateVisual();
    }

    public void setPendingActor(string? blueprintReference)
    {
        string? nextPendingActor = string.IsNullOrWhiteSpace(blueprintReference)
            ? null
            : blueprintReference.Trim();
        if (string.Equals(pendingActor, nextPendingActor, StringComparison.Ordinal))
            return;
        pendingActor = nextPendingActor;
        invalidatePendingActorRenderState();
        InvalidateVisual();
    }

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

    private void drawCheckerboard(DrawingContext context)
    {
        if (checkerboardRenderCache is null)
            return;
        PixelSize pixelSize = checkerboardRenderCache.Bitmap.PixelSize;
        context.DrawImage(checkerboardRenderCache.Bitmap, new Rect(0, 0, pixelSize.Width, pixelSize.Height), checkerboardRenderCache.Viewport);
    }

    private void drawLayer(DrawingContext context, string layerName)
    {
        if (!layerRenderCaches.TryGetValue(layerName, out LayerRenderCache? cache))
            return;
        PixelSize pixelSize = cache.Bitmap.PixelSize;
        context.DrawImage(cache.Bitmap, new Rect(0, 0, pixelSize.Width, pixelSize.Height), cache.Viewport);
    }

    private void drawActors(DrawingContext context, string layerName)
    {
        if (!actorRenderStates.TryGetValue(layerName, out List<ActorRenderState>? actors))
            return;
        for (int index = 0; index < actors.Count; index++)
        {
            ActorRenderState actor = actors[index];
            if (!tryGetActorPosition(actor.Actor, out int gridX, out int gridY))
                continue;
            Bitmap? image = actor.PreviewLease?.Frame ?? actor.Image;
            if (image is null)
            {
                context.FillRectangle(MissingActorBrush, getLocalTileRect(gridX, gridY));
                continue;
            }
            Rect source = getActorRenderSource(actor);
            Matrix transform = createActorTransform(gridX, gridY, actor.Translation, actor.Scale, actor.Rotation);
            using (context.PushTransform(transform))
            {
                Rect destination = new Rect(-actor.Origin.X, -actor.Origin.Y, source.Width, source.Height);
                context.DrawImage(image, source, destination);
                if (selectedActorLayer == layerName && selectedActorIndex == index)
                    context.DrawRectangle(null, SelectedActorPen, destination);
            }
            Rect cell = getLocalTileRect(gridX, gridY);
            context.DrawEllipse(ActorOriginBrush, null, cell.Center, 3, 3);
        }
    }

    private void drawLightOverlay(DrawingContext context)
    {
        if (CurrentMapData?["lights"] is not JsonArray lights)
            return;
        double scale = tileSize / (double)SourceTileSize;
        for (int index = 0; index < lights.Count; index++)
        {
            if (lights[index] is not JsonObject light || !tryGetLight(light, out Point center, out double radius))
                continue;
            Point displayCenter = new(snapToDevicePixel(center.X * scale), snapToDevicePixel(center.Y * scale));
            double displayRadius = radius * scale;
            bool selected = selectedLightIndex == index;
            Color outline = selected ? Color.FromRgb(255, 220, 0) : Color.FromRgb(0, 200, 0);
            Color fill = getLightFill(light);
            context.DrawEllipse(new SolidColorBrush(fill), new Pen(new SolidColorBrush(outline), 2), displayCenter, displayRadius, displayRadius);
            context.DrawEllipse(new SolidColorBrush(outline), null, displayCenter, 3, 3);
        }
    }

    private void drawHoverAndPlacement(DrawingContext context)
    {
        if (hoverGrid is not { } grid || CurrentMapData is null)
            return;
        (int X, int Y) end = grid;
        (int X, int Y) start = rectangleStart ?? end;
        int minX = Math.Min(start.X, end.X);
        int minY = Math.Min(start.Y, end.Y);
        int maxX = Math.Max(start.X, end.X);
        int maxY = Math.Max(start.Y, end.Y);
        Rect first = getLocalTileRect(minX, minY);
        Rect last = getLocalTileRect(maxX, maxY);
        double devicePixel = 1.0 / getRenderScaling();
        context.DrawRectangle(null, HoverPen, new Rect(first.X, first.Y, last.Right - first.X - devicePixel, last.Bottom - first.Y - devicePixel));
        if (EditMode == MapEditMode.Actor && selectedLayerName is not null && !string.IsNullOrWhiteSpace(pendingActor))
            drawPendingActor(context, grid);
    }

    private void drawPendingActor(DrawingContext context, (int X, int Y) grid)
    {
        if (pendingActorRenderState is not { } actor)
            return;
        Bitmap? image = actor.PreviewLease?.Frame ?? actor.Image;
        if (image is null)
            return;
        Rect source = getActorRenderSource(actor);
        Matrix transform = createActorTransform(grid.X, grid.Y, actor.Translation, actor.Scale, actor.Rotation);
        using (context.PushOpacity(0.5))
        using (context.PushTransform(transform))
            context.DrawImage(image, source, new Rect(-actor.Origin.X, -actor.Origin.Y, source.Width, source.Height));
    }

    private void ensureRenderCaches(int mapWidth, int mapHeight, Rect viewport)
    {
        double renderScale = getRenderScaling();
        if (animationStateDirty)
            renderedAutoTileFrame = getAutoTileFrame();
        CacheGeometry nextGeometry = new(mapWidth, mapHeight, tileSize, renderScale, viewport);
        if (cacheGeometry != nextGeometry)
        {
            disposeViewportRenderCaches();
            cacheGeometry = nextGeometry;
        }
        checkerboardRenderCache ??= buildCheckerboardRenderCache(viewport, renderScale);
        ensureActorRenderStates();
        ensurePendingActorRenderState();
        scheduleActorPreviewActivityUpdate();
        JsonObject? layers = CurrentMapData?["layers"] as JsonObject;
        if (layers is not null)
        {
            HashSet<string> activeLayers = new(StringComparer.Ordinal);
            foreach (KeyValuePair<string, JsonNode?> entry in layers)
            {
                if (entry.Value is not JsonObject layer || !isLayerVisible(layer))
                    continue;
                activeLayers.Add(entry.Key);
                if (!layerRenderCaches.ContainsKey(entry.Key) || dirtyLayerNames.Contains(entry.Key))
                {
                    if (layerRenderCaches.Remove(entry.Key, out LayerRenderCache? previous))
                        previous.Dispose();
                    layerRenderCaches[entry.Key] = buildLayerRenderCache(layer, mapWidth, mapHeight, viewport, renderScale);
                    dirtyLayerNames.Remove(entry.Key);
                }
            }
            foreach (string layerName in new List<string>(layerRenderCaches.Keys))
            {
                if (!activeLayers.Contains(layerName) && layerRenderCaches.Remove(layerName, out LayerRenderCache? removed))
                    removed.Dispose();
            }
        }
        rebuildAnimationStateIfNeeded();
    }

    private ViewportRenderCache buildCheckerboardRenderCache(Rect viewport, double renderScale)
    {
        RenderTargetBitmap bitmap = createRenderTarget(viewport, renderScale);
        using DrawingContext context = bitmap.CreateDrawingContext();
        using (context.PushTransform(Matrix.CreateTranslation(-viewport.X, -viewport.Y)))
        {
            double checkerSize = tileSize / 2.0;
            int minColumn = Math.Max(0, (int)Math.Floor(viewport.X / checkerSize));
            int minRow = Math.Max(0, (int)Math.Floor(viewport.Y / checkerSize));
            int maxColumn = Math.Max(minColumn, (int)Math.Ceiling(viewport.Right / checkerSize));
            int maxRow = Math.Max(minRow, (int)Math.Ceiling(viewport.Bottom / checkerSize));
            for (int y = minRow; y < maxRow; y++)
            {
                double top = snapToDevicePixel(y * checkerSize);
                double bottom = snapToDevicePixel((y + 1) * checkerSize);
                for (int x = minColumn; x < maxColumn; x++)
                {
                    double left = snapToDevicePixel(x * checkerSize);
                    double right = snapToDevicePixel((x + 1) * checkerSize);
                    IBrush brush = (x + y) % 2 == 0 ? CheckerLightBrush : CheckerDarkBrush;
                    context.FillRectangle(brush, new Rect(left, top, right - left, bottom - top));
                }
            }
        }
        return new ViewportRenderCache(bitmap, viewport);
    }

    private LayerRenderCache buildLayerRenderCache(JsonObject layer, int mapWidth, int mapHeight, Rect viewport, double renderScale)
    {
        RenderTargetBitmap bitmap = createRenderTarget(viewport, renderScale);
        using DrawingContext context = bitmap.CreateDrawingContext();
        using (context.PushTransform(Matrix.CreateTranslation(-viewport.X, -viewport.Y)))
            drawLayerCells(context, layer, mapWidth, mapHeight, viewport);
        return new LayerRenderCache(bitmap, viewport);
    }

    private void drawLayerCells(DrawingContext context, JsonObject layer, int mapWidth, int mapHeight, Rect viewport)
    {
        if (gameData is null || autoTileRenderer is null)
            return;
        string? tilesetKey = layer["layerTileset"]?.GetValue<string>();
        Bitmap? tileset = getTileset(tilesetKey);
        JsonArray? tiles = layer["tiles"] as JsonArray;
        JsonArray? autoTiles = layer["autoTiles"] as JsonArray;
        int minX = Math.Max(0, (int)Math.Floor(viewport.X / tileSize));
        int minY = Math.Max(0, (int)Math.Floor(viewport.Y / tileSize));
        int maxX = Math.Min(mapWidth, (int)Math.Ceiling(viewport.Right / tileSize));
        int maxY = Math.Min(mapHeight, (int)Math.Ceiling(viewport.Bottom / tileSize));
        for (int y = minY; y < maxY; y++)
        {
            JsonArray? tileRow = getRow(tiles, y);
            JsonArray? autoRow = getRow(autoTiles, y);
            for (int x = minX; x < maxX; x++)
            {
                Rect destination = getLocalTileRect(x, y);
                string? autoKey = autoRow?[x]?.GetValue<string>();
                if (!string.IsNullOrWhiteSpace(autoKey))
                {
                    autoTileRenderer.drawTile(context, autoKey, autoTiles!, x, y, destination, renderedAutoTileFrame);
                    continue;
                }
                if (tileset is null || !tryGetInt(tileRow?[x], out int number))
                    continue;
                int columns = Math.Max(1, tileset.PixelSize.Width / SourceTileSize);
                int sourceX = number % columns * SourceTileSize;
                int sourceY = number / columns * SourceTileSize;
                Rect source = new(sourceX, sourceY, SourceTileSize, SourceTileSize);
                if (source.Right <= tileset.PixelSize.Width && source.Bottom <= tileset.PixelSize.Height)
                    context.DrawImage(tileset, source, destination);
            }
        }
    }

    private static RenderTargetBitmap createRenderTarget(Rect viewport, double renderScale)
    {
        int width = Math.Max(1, (int)Math.Ceiling(viewport.Width * renderScale));
        int height = Math.Max(1, (int)Math.Ceiling(viewport.Height * renderScale));
        Vector dpi = new(96 * renderScale, 96 * renderScale);
        return new RenderTargetBitmap(new PixelSize(width, height), dpi);
    }

    private void ensureActorRenderStates()
    {
        if (!actorRenderStatesDirty)
            return;
        disposeActorPreviewLeases();
        actorRenderStates.Clear();
        Dictionary<string, ActorVisualDescriptor?> sharedDescriptors = new(StringComparer.Ordinal);
        using IDisposable? resolutionBatch = previewService?.BeginResolutionBatch();
        if (CurrentMapData?["actors"] is JsonObject actorGroups)
        {
            foreach (KeyValuePair<string, JsonNode?> entry in actorGroups)
            {
                if (entry.Value is not JsonArray actors)
                    continue;
                List<ActorRenderState> states = new(actors.Count);
                foreach (JsonNode? node in actors)
                {
                    JsonObject actor = node as JsonObject ?? new JsonObject();
                    states.Add(createActorRenderState(actor, sharedDescriptors));
                }
                actorRenderStates[entry.Key] = states;
            }
        }
        actorRenderStatesDirty = false;
        animationStateDirty = true;
    }

    private void ensurePendingActorRenderState()
    {
        if (!pendingActorRenderStateDirty)
            return;
        disposeActorPreviewLease(pendingActorRenderState?.PreviewLease);
        pendingActorRenderState = null;
        if (!string.IsNullOrWhiteSpace(pendingActor))
        {
            JsonObject ghost = new() { ["bp"] = pendingActor };
            pendingActorRenderState = createActorRenderState(ghost);
        }
        pendingActorRenderStateDirty = false;
        animationStateDirty = true;
    }

    private ActorRenderState createActorRenderState(JsonObject actor)
    {
        ActorVisualDescriptor? descriptor = resolveActorVisual(actor, null);
        return createActorRenderState(actor, descriptor);
    }

    private ActorRenderState createActorRenderState(
        JsonObject actor,
        Dictionary<string, ActorVisualDescriptor?> sharedDescriptors)
    {
        ActorVisualDescriptor? descriptor = resolveActorVisual(actor, sharedDescriptors);
        return createActorRenderState(actor, descriptor);
    }

    private ActorVisualDescriptor? resolveActorVisual(
        JsonObject actor,
        Dictionary<string, ActorVisualDescriptor?>? sharedDescriptors)
    {
        if (CurrentMapData is null || previewService is null)
            return null;
        string reference = actor["bp"]?.GetValue<string>() ?? string.Empty;
        string tag = actor["tag"]?.GetValue<string>() ?? string.Empty;
        JsonObject? overrides = tag.Length == 0
            ? null
            : CurrentMapData["BPClassVarChanged"]?[tag] as JsonObject;
        if (overrides?.Count == 0)
            overrides = null;
        if (sharedDescriptors is null || overrides is not null)
            return previewService.tryResolveActorVisual(reference, overrides);
        if (!sharedDescriptors.TryGetValue(reference, out ActorVisualDescriptor? descriptor))
        {
            descriptor = previewService.tryResolveActorVisual(reference);
            sharedDescriptors[reference] = descriptor;
        }
        return descriptor;
    }

    private ActorRenderState createActorRenderState(
        JsonObject actor,
        ActorVisualDescriptor? descriptor)
    {
        if (descriptor is null)
            return ActorRenderState.Missing(actor);
        Bitmap? image = getActorBitmap(descriptor.TexturePath);
        if (image is null)
            return ActorRenderState.Missing(actor);
        PixelRect textureRect = descriptor.BaseTextureRect;
        Rect source = new(textureRect.X, textureRect.Y, textureRect.Width, textureRect.Height);
        if (source.Width <= 0 || source.Height <= 0 || source.Right > image.PixelSize.Width || source.Bottom > image.PixelSize.Height)
            return ActorRenderState.Missing(actor);
        if (Math.Abs(descriptor.Hue % 360) > 0.001)
            image = getHueImage(descriptor.TexturePath, image, descriptor.Hue);
        ActorPreviewLease? previewLease = null;
        if (descriptor.RequiresNativePreview && previewService is not null)
        {
            previewLease = previewService.ActorPreviews.Acquire(descriptor, 0, false);
            previewLease.FrameChanged += onActorPreviewFrameChanged;
        }
        return new ActorRenderState(
            actor,
            image,
            source,
            descriptor.Translation,
            descriptor.Scale,
            descriptor.Origin,
            descriptor.Rotation,
            descriptor.Animated,
            descriptor.SwitchInterval,
            descriptor.FrameCount,
            previewLease);
    }

    private Rect getActorRenderSource(ActorRenderState actor)
    {
        if (actor.PreviewLease is { Frame: not null } preview)
        {
            PixelRect rect = preview.SourceRect;
            return new Rect(rect.X, rect.Y, rect.Width, rect.Height);
        }
        if (!actor.Animated || actor.FrameCount <= 1 || actor.Image is null)
            return actor.BaseSource;
        TimeSpan elapsed = previewService?.ActorPreviews.Elapsed ?? animationClock.Elapsed;
        int frame = (int)(elapsed.TotalSeconds / actor.Interval) % actor.FrameCount;
        double sourceX = (actor.BaseSource.X + frame * actor.BaseSource.Width) % actor.Image.PixelSize.Width;
        return new Rect(sourceX, actor.BaseSource.Y, actor.BaseSource.Width, actor.BaseSource.Height);
    }

    private void onActorPreviewFrameChanged(object? sender, EventArgs args)
    {
        if (Dispatcher.UIThread.CheckAccess())
        {
            InvalidateVisual();
            return;
        }
        Dispatcher.UIThread.Post(InvalidateVisual);
    }

    private void updateActorPreviewActivity()
    {
        if (!tryGetMapSize(out int mapWidth, out int mapHeight))
        {
            setActorPreviewActivity(false);
            return;
        }
        Rect mapRect = getMapRect(mapWidth, mapHeight);
        updateActorPreviewActivity(getVisibleLocalMapRect(mapRect));
    }

    private void scheduleActorPreviewActivityUpdate()
    {
        if (actorPreviewActivityUpdatePending)
            return;
        actorPreviewActivityUpdatePending = true;
        Dispatcher.UIThread.Post(() =>
        {
            actorPreviewActivityUpdatePending = false;
            updateActorPreviewActivity();
        }, DispatcherPriority.Background);
    }

    private void updateActorPreviewActivity(Rect viewport)
    {
        bool panelActive = IsEffectivelyVisible && VisualRoot is not null
            && viewport.Width > 0 && viewport.Height > 0;
        foreach (KeyValuePair<string, List<ActorRenderState>> entry in actorRenderStates)
        {
            bool layerVisible = CurrentMapData?["layers"]?[entry.Key] is JsonObject layer
                && isLayerVisible(layer);
            foreach (ActorRenderState actor in entry.Value)
            {
                if (actor.PreviewLease is null)
                    continue;
                bool active = panelActive
                    && layerVisible
                    && tryGetActorPosition(actor.Actor, out int gridX, out int gridY)
                    && getActorPreviewBounds(actor, gridX, gridY).Intersects(viewport);
                actor.PreviewLease.IsActive = active;
            }
        }
        if (pendingActorRenderState?.PreviewLease is { } pendingLease)
            pendingLease.IsActive = panelActive && EditMode == MapEditMode.Actor && hoverGrid is not null;
    }

    private Rect getActorPreviewBounds(ActorRenderState actor, int gridX, int gridY)
    {
        Rect source = getActorRenderSource(actor);
        Rect destination = new(-actor.Origin.X, -actor.Origin.Y, source.Width, source.Height);
        return destination.TransformToAABB(
            createActorTransform(gridX, gridY, actor.Translation, actor.Scale, actor.Rotation));
    }

    private void setActorPreviewActivity(bool active)
    {
        foreach (List<ActorRenderState> actors in actorRenderStates.Values)
        {
            foreach (ActorRenderState actor in actors)
            {
                if (actor.PreviewLease is not null)
                    actor.PreviewLease.IsActive = active;
            }
        }
        if (pendingActorRenderState?.PreviewLease is not null)
            pendingActorRenderState.PreviewLease.IsActive = active;
    }

    private void disposeActorPreviewLeases()
    {
        foreach (List<ActorRenderState> actors in actorRenderStates.Values)
        {
            foreach (ActorRenderState actor in actors)
                disposeActorPreviewLease(actor.PreviewLease);
        }
    }

    private void disposeActorPreviewLease(ActorPreviewLease? lease)
    {
        if (lease is null)
            return;
        lease.FrameChanged -= onActorPreviewFrameChanged;
        lease.Dispose();
    }

    private void rebuildAnimationStateIfNeeded()
    {
        if (!animationStateDirty || autoTileRenderer is null)
            return;
        animatedAutoTileLayerNames.Clear();
        if (CurrentMapData?["layers"] is JsonObject layers)
        {
            foreach (KeyValuePair<string, JsonNode?> entry in layers)
            {
                if (entry.Value?["autoTiles"] is not JsonArray grid)
                    continue;
                HashSet<string> keys = new(StringComparer.Ordinal);
                foreach (JsonNode? rowNode in grid)
                {
                    if (rowNode is not JsonArray row)
                        continue;
                    foreach (JsonNode? value in row)
                    {
                        string? key = value?.GetValue<string>();
                        if (!string.IsNullOrWhiteSpace(key))
                            keys.Add(key);
                    }
                }
                foreach (string key in keys)
                {
                    if (autoTileRenderer.getFrameCount(key) > 1)
                    {
                        animatedAutoTileLayerNames.Add(entry.Key);
                        break;
                    }
                }
            }
        }
        hasAnimatedActors = pendingActorRenderState is { Animated: true, FrameCount: > 1 };
        if (!hasAnimatedActors)
        {
            foreach (List<ActorRenderState> states in actorRenderStates.Values)
            {
                if (states.Exists(actor => actor.Animated && actor.FrameCount > 1))
                {
                    hasAnimatedActors = true;
                    break;
                }
            }
        }
        renderedAutoTileFrame = getAutoTileFrame();
        animationStateDirty = false;
        if (hasAnimatedActors || animatedAutoTileLayerNames.Count > 0)
        {
            if (!animationTimer.IsEnabled)
                animationTimer.Start();
        }
        else
            animationTimer.Stop();
    }

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
        markMapModified();
        ActorDataChanged?.Invoke(this, EventArgs.Empty);
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
        gameData?.RecordSnapshot();
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
        markMapModified();
        ActorDataChanged?.Invoke(this, EventArgs.Empty);
    }

    private void pasteActor((int X, int Y) grid)
    {
        if (!selectedLayerEditable || actorClipboard is null || selectedLayerName is null || hasActorAt(selectedLayerName, grid))
            return;
        JsonObject copy = (JsonObject)actorClipboard.DeepClone();
        string reference = copy["bp"]?.GetValue<string>() ?? string.Empty;
        copy["position"] = new JsonArray(grid.X, grid.Y);
        copy["tag"] = makeActorTag(reference, selectedLayerName, grid);
        gameData?.RecordSnapshot();
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
        markMapModified();
        ActorDataChanged?.Invoke(this, EventArgs.Empty);
        InvalidateVisual();
    }

    private void deleteSelectedActor()
    {
        if (!selectedLayerEditable || selectedActorLayer is null || selectedActorIndex is not int index || getActorList(selectedActorLayer, false) is not JsonArray actors || index < 0 || index >= actors.Count)
            return;
        JsonObject? actor = actors[index] as JsonObject;
        gameData?.RecordSnapshot();
        if (actor?["tag"]?.GetValue<string>() is string tag && CurrentMapData?["BPClassVarChanged"] is JsonObject root)
        {
            root.Remove(tag);
            if (root.Count == 0)
                CurrentMapData.Remove("BPClassVarChanged");
        }
        actors.RemoveAt(index);
        invalidateActorRenderStates();
        setSelectedActor(null, null, true);
        markMapModified();
        ActorDataChanged?.Invoke(this, EventArgs.Empty);
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

    private Bitmap? getTileset(string? key)
    {
        if (gameData is null || string.IsNullOrWhiteSpace(key) || !gameData.TilesetData.TryGetValue(key, out JsonObject? data))
            return null;
        string? fileName = data["fileName"]?.GetValue<string>();
        return loadBitmap(Path.Combine(gameData.ProjectPath, "Assets", "Tilesets", fileName ?? string.Empty));
    }

    private Bitmap? getActorBitmap(string? texturePath)
    {
        if (gameData is null || string.IsNullOrWhiteSpace(texturePath))
            return null;
        string path = Path.IsPathRooted(texturePath)
            ? texturePath
            : texturePath.StartsWith("Assets/", StringComparison.OrdinalIgnoreCase) || texturePath.StartsWith("Assets\\", StringComparison.OrdinalIgnoreCase)
                ? Path.Combine(gameData.ProjectPath, texturePath)
                : Path.Combine(gameData.ProjectPath, "Assets", "Characters", texturePath);
        return loadBitmap(path);
    }

    private Bitmap? loadBitmap(string path)
    {
        FileInfo file = new(path);
        if (bitmapCache.TryGetValue(path, out CachedBitmap cached))
        {
            if (file.Exists
                && cached.ModifiedAt == file.LastWriteTimeUtc
                && cached.Length == file.Length)
            {
                return cached.Image;
            }
            retiredBitmaps.Add(cached.Image);
            retireHueImages(path);
            bitmapCache.Remove(path);
        }
        if (!file.Exists)
            return null;
        Bitmap image = new(path);
        bitmapCache[path] = new CachedBitmap(file.LastWriteTimeUtc, file.Length, image);
        return image;
    }

    private Bitmap getHueImage(string path, Bitmap source, double hue)
    {
        string cacheKey = path + "|" + (hue % 360).ToString("F3", CultureInfo.InvariantCulture);
        if (hueCache.TryGetValue(cacheKey, out Bitmap? cached))
            return cached;
        int width = source.PixelSize.Width;
        int height = source.PixelSize.Height;
        WriteableBitmap result = new(new PixelSize(width, height), new Vector(96, 96), Avalonia.Platform.PixelFormat.Bgra8888, Avalonia.Platform.AlphaFormat.Unpremul);
        using ILockedFramebuffer frame = result.Lock();
        byte[] pixels = new byte[frame.RowBytes * height];
        source.CopyPixels(new PixelRect(source.PixelSize), frame.Address, pixels.Length, frame.RowBytes);
        double offset = (hue % 360 + 360) % 360 / 360.0;
        for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            int index = y * frame.RowBytes + x * 4;
            if (pixels[index + 3] == 0)
                continue;
            (double h, double s, double v) = rgbToHsv(pixels[index + 2] / 255.0, pixels[index + 1] / 255.0, pixels[index] / 255.0);
            (double r, double g, double b) = hsvToRgb((h + offset) % 1.0, s, v);
            pixels[index] = (byte)Math.Round(b * 255);
            pixels[index + 1] = (byte)Math.Round(g * 255);
            pixels[index + 2] = (byte)Math.Round(r * 255);
        }
        Marshal.Copy(pixels, 0, frame.Address, pixels.Length);
        hueCache[cacheKey] = result;
        return result;
    }

    private void retireHueImages(string path)
    {
        string prefix = path + "|";
        foreach (string key in hueCache.Keys
            .Where(key => key.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
            .ToArray())
        {
            retiredBitmaps.Add(hueCache[key]);
            hueCache.Remove(key);
        }
    }

    private static (double H, double S, double V) rgbToHsv(double r, double g, double b)
    {
        double max = Math.Max(r, Math.Max(g, b));
        double min = Math.Min(r, Math.Min(g, b));
        double delta = max - min;
        if (delta == 0)
            return (0, 0, max);
        double h = max == r ? ((g - b) / delta + (g < b ? 6 : 0)) : max == g ? (b - r) / delta + 2 : (r - g) / delta + 4;
        return (h / 6, max == 0 ? 0 : delta / max, max);
    }

    private static (double R, double G, double B) hsvToRgb(double h, double s, double v)
    {
        int sector = (int)Math.Floor(h * 6) % 6;
        double f = h * 6 - Math.Floor(h * 6);
        double p = v * (1 - s);
        double q = v * (1 - f * s);
        double t = v * (1 - (1 - f) * s);
        return sector switch { 0 => (v, t, p), 1 => (q, v, p), 2 => (p, v, t), 3 => (p, q, v), 4 => (t, p, v), _ => (v, p, q) };
    }

    private void showLightContextMenu(Point position, int width, int height)
    {
        if (getMapBasePosition(position, width, height) is not { } basePosition)
            return;
        MenuItem add = new() { Header = LocaleService.Get("NEW_LIGHT_SOURCE") };
        add.Click += (_, _) =>
        {
            gameData?.RecordSnapshot();
            JsonArray lights = ensureLights();
            lights.Add(new JsonObject
            {
                ["position"] = new JsonArray(basePosition.X, basePosition.Y),
                ["color"] = new JsonArray(255, 255, 255, 255),
                ["radius"] = 256.0,
                ["intensity"] = 1.0,
            });
            setSelectedLightIndex(lights.Count - 1);
            markMapModified();
            InvalidateVisual();
        };
        MenuItem delete = new() { Header = LocaleService.Get("DELETE"), IsEnabled = selectedLightIndex is not null };
        delete.Click += (_, _) => deleteSelectedLight();
        new ContextMenu { ItemsSource = new object[] { add, delete } }.Open(this);
    }

    private void deleteSelectedLight()
    {
        if (selectedLightIndex is not int index
            || CurrentMapData?["lights"] is not JsonArray lights
            || index < 0
            || index >= lights.Count)
        {
            return;
        }
        gameData?.RecordSnapshot();
        lights.RemoveAt(index);
        setSelectedLightIndex(null);
        markMapModified();
        InvalidateVisual();
    }

    private int? hitTestLight(Point position)
    {
        if (CurrentMapData?["lights"] is not JsonArray lights)
            return null;
        int? best = null;
        double bestDistance = double.MaxValue;
        for (int index = 0; index < lights.Count; index++)
        {
            if (lights[index] is not JsonObject light || !tryGetLight(light, out Point center, out double radius))
                continue;
            double distance = getDistance(position, center);
            if (distance <= radius && distance < bestDistance)
            {
                best = index;
                bestDistance = distance;
            }
        }
        return best;
    }

    private void setSelectedLightIndex(int? index)
    {
        JsonObject? light = null;
        if (index is int value && CurrentMapData?["lights"] is JsonArray lights && value >= 0 && value < lights.Count)
            light = lights[value] as JsonObject;
        int? nextIndex = light is null ? null : index;
        if (selectedLightIndex == nextIndex)
            return;
        selectedLightIndex = nextIndex;
        LightSelectionChanged?.Invoke(this, new LightSelectionChangedEventArgs(CurrentMapKey ?? string.Empty, selectedLightIndex, light));
        InvalidateVisual();
    }

    private int? hitTestActor(string layerName, Point mapPosition)
    {
        ensureActorRenderStates();
        if (!actorRenderStates.TryGetValue(layerName, out List<ActorRenderState>? actors))
            return null;
        for (int index = actors.Count - 1; index >= 0; index--)
        {
            ActorRenderState actor = actors[index];
            if (!tryGetActorPosition(actor.Actor, out int x, out int y))
                continue;
            Bitmap? image = actor.PreviewLease?.Frame ?? actor.Image;
            if (image is null)
            {
                if (getLocalTileRect(x, y).Contains(mapPosition))
                    return index;
                continue;
            }
            Rect source = getActorRenderSource(actor);
            Matrix transform = createActorTransform(x, y, actor.Translation, actor.Scale, actor.Rotation);
            if (!transform.TryInvert(out Matrix inverse))
                continue;
            Point local = inverse.Transform(mapPosition);
            if (new Rect(-actor.Origin.X, -actor.Origin.Y, source.Width, source.Height).Contains(local))
                return index;
        }
        return null;
    }

    private bool hasActorAt(string layerName, (int X, int Y) grid)
    {
        if (getActorList(layerName, false) is not JsonArray actors)
            return false;
        foreach (JsonNode? node in actors)
        {
            if (node is JsonObject actor && tryGetActorPosition(actor, out int x, out int y) && x == grid.X && y == grid.Y)
                return true;
        }
        return false;
    }

    private JsonObject? getSelectedActor()
    {
        return selectedActorLayer is not null && selectedActorIndex is int index && getActorList(selectedActorLayer, false) is JsonArray actors && index >= 0 && index < actors.Count
            ? actors[index] as JsonObject
            : null;
    }

    private JsonArray? getActorList(string layerName, bool create)
    {
        if (CurrentMapData is null)
            return null;
        if (CurrentMapData["actors"] is not JsonObject groups)
        {
            if (!create)
                return null;
            groups = new JsonObject();
            CurrentMapData["actors"] = groups;
        }
        if (groups[layerName] is JsonArray actors)
            return actors;
        if (!create)
            return null;
        actors = new JsonArray();
        groups[layerName] = actors;
        return actors;
    }

    private string makeActorTag(string reference, string layerName, (int X, int Y) grid)
    {
        string prefix = reference.Replace("Data.Blueprints.", string.Empty, StringComparison.Ordinal).Replace('.', '_');
        string candidate = $"{prefix}_default_{grid.X}_{grid.Y}";
        return makeUniqueActorTag(candidate);
    }

    private bool actorTagExists(string tag, string? ignoreLayerName, int? ignoreIndex)
    {
        if (CurrentMapData?["actors"] is not JsonObject groups)
            return false;
        foreach (KeyValuePair<string, JsonNode?> entry in groups)
        {
            if (entry.Value is not JsonArray actors)
                continue;
            for (int index = 0; index < actors.Count; index += 1)
            {
                if (entry.Key == ignoreLayerName && index == ignoreIndex)
                    continue;
                if (actors[index] is JsonObject actor && actor["tag"]?.GetValue<string>() == tag)
                    return true;
            }
        }
        return false;
    }

    private int getTilesetColumnCount(JsonObject layer)
    {
        Bitmap? tileset = getTileset(layer["layerTileset"]?.GetValue<string>());
        return tileset is null ? 1 : Math.Max(1, tileset.PixelSize.Width / SourceTileSize);
    }

    private static JsonArray ensureGrid(JsonObject layer, string name, int width, int height)
    {
        if (layer[name] is not JsonArray grid)
        {
            grid = new JsonArray();
            layer[name] = grid;
        }
        while (grid.Count < height)
            grid.Add(new JsonArray());
        for (int y = 0; y < height; y++)
        {
            if (grid[y] is not JsonArray row)
            {
                row = new JsonArray();
                grid[y] = row;
            }
            while (row.Count < width)
                row.Add(null);
        }
        return grid;
    }

    private JsonArray ensureLights()
    {
        if (CurrentMapData?["lights"] is JsonArray lights)
            return lights;
        JsonArray result = new();
        if (CurrentMapData is not null)
            CurrentMapData["lights"] = result;
        return result;
    }

    private bool tryGetMapSize(out int width, out int height)
    {
        width = CurrentMapData?["width"]?.GetValue<int?>() ?? 0;
        height = CurrentMapData?["height"]?.GetValue<int?>() ?? 0;
        return width > 0 && height > 0;
    }

    private Rect getMapRect(int width, int height)
    {
        double scale = getRenderScaling();
        double mapWidth = snapToDevicePixel(width * tileSize);
        double mapHeight = snapToDevicePixel(height * tileSize);
        double x = Math.Max(0, (Bounds.Width - mapWidth) / 2);
        double y = Math.Max(0, (Bounds.Height - mapHeight) / 2);
        return new Rect(Math.Round(x * scale) / scale, Math.Round(y * scale) / scale, mapWidth, mapHeight);
    }

    private Rect getVisibleLocalMapRect(Rect mapRect)
    {
        Rect clipBounds = getScrollViewportRect();
        double left = Math.Max(clipBounds.Left, mapRect.Left);
        double top = Math.Max(clipBounds.Top, mapRect.Top);
        double right = Math.Min(clipBounds.Right, mapRect.Right);
        double bottom = Math.Min(clipBounds.Bottom, mapRect.Bottom);
        if (right <= left || bottom <= top)
            return default;
        return new Rect(left - mapRect.X, top - mapRect.Y, right - left, bottom - top);
    }

    private Rect getScrollViewportRect()
    {
        if (hostScrollViewer is null)
            return new Rect(0, 0, Bounds.Width, Bounds.Height);
        return new Rect(hostScrollViewer.Offset.X, hostScrollViewer.Offset.Y, hostScrollViewer.Viewport.Width, hostScrollViewer.Viewport.Height);
    }

    private void bindHostScrollViewer(ScrollViewer? scrollViewer)
    {
        if (ReferenceEquals(hostScrollViewer, scrollViewer))
            return;
        if (hostScrollViewer is not null)
            hostScrollViewer.ScrollChanged -= onHostScrollChanged;
        hostScrollViewer = scrollViewer;
        if (hostScrollViewer is not null)
            hostScrollViewer.ScrollChanged += onHostScrollChanged;
    }

    private void onHostScrollChanged(object? sender, ScrollChangedEventArgs e)
    {
        InvalidateVisual();
    }

    private Rect getLocalMapRect(int width, int height)
    {
        return new Rect(0, 0, snapToDevicePixel(width * tileSize), snapToDevicePixel(height * tileSize));
    }

    private Rect getLocalTileRect(int x, int y)
    {
        double left = snapToDevicePixel(x * tileSize);
        double top = snapToDevicePixel(y * tileSize);
        double right = snapToDevicePixel((x + 1) * tileSize);
        double bottom = snapToDevicePixel((y + 1) * tileSize);
        return new Rect(left, top, right - left, bottom - top);
    }

    private double getRenderScaling()
    {
        return Math.Max(1.0, TopLevel.GetTopLevel(this)?.RenderScaling ?? 1.0);
    }

    private double snapToDevicePixel(double value)
    {
        double scale = getRenderScaling();
        return Math.Round(value * scale) / scale;
    }

    private (int X, int Y)? getGridPosition(Point position, int width, int height)
    {
        Rect mapRect = getMapRect(width, height);
        if (!mapRect.Contains(position))
            return null;
        int x = (int)((position.X - mapRect.X) / tileSize);
        int y = (int)((position.Y - mapRect.Y) / tileSize);
        return x >= 0 && y >= 0 && x < width && y < height ? (x, y) : null;
    }

    private Point? getMapBasePosition(Point position, int width, int height)
    {
        Rect mapRect = getMapRect(width, height);
        return mapRect.Contains(position)
            ? new Point((position.X - mapRect.X) * SourceTileSize / tileSize, (position.Y - mapRect.Y) * SourceTileSize / tileSize)
            : null;
    }

    private Point? getMapDisplayPosition(Point position, out int width, out int height)
    {
        if (!tryGetMapSize(out width, out height))
            return null;
        Rect mapRect = getMapRect(width, height);
        return mapRect.Contains(position) ? new Point(position.X - mapRect.X, position.Y - mapRect.Y) : null;
    }

    private int getAutoTileFrame() => (int)(animationClock.ElapsedMilliseconds / 500) % 1024;

    private void onAnimationTick(object? sender, EventArgs args)
    {
        bool redraw = hasAnimatedActors;
        int autoTileFrame = getAutoTileFrame();
        if (animatedAutoTileLayerNames.Count > 0 && autoTileFrame != renderedAutoTileFrame)
        {
            renderedAutoTileFrame = autoTileFrame;
            foreach (string layerName in animatedAutoTileLayerNames)
                dirtyLayerNames.Add(layerName);
            redraw = true;
        }
        if (redraw)
            InvalidateVisual();
    }

    private void onTileBrushRenderTick(object? sender, EventArgs args)
    {
        flushPendingBrushLayers();
    }

    private void scheduleBrushLayerRefresh(string layerName)
    {
        pendingBrushLayerNames.Add(layerName);
        animationStateDirty = true;
        if (!tileBrushRenderTimer.IsEnabled)
            tileBrushRenderTimer.Start();
    }

    private void flushPendingBrushLayers()
    {
        tileBrushRenderTimer.Stop();
        if (pendingBrushLayerNames.Count == 0)
            return;
        foreach (string layerName in pendingBrushLayerNames)
            dirtyLayerNames.Add(layerName);
        pendingBrushLayerNames.Clear();
        InvalidateVisual();
    }

    private static double getDistance(Point first, Point second)
    {
        double dx = first.X - second.X;
        double dy = first.Y - second.Y;
        return Math.Sqrt(dx * dx + dy * dy);
    }

    private static bool isLayerVisible(JsonObject layer)
    {
        return layer["visible"]?.GetValue<bool?>() ?? true;
    }

    private static JsonArray? getRow(JsonArray? grid, int y) => grid is not null && y >= 0 && y < grid.Count ? grid[y] as JsonArray : null;

    private static JsonNode? getCell(JsonArray? grid, int x, int y)
    {
        JsonArray? row = getRow(grid, y);
        return row is not null && x >= 0 && x < row.Count ? row[x] : null;
    }

    private static bool tryGetInt(JsonNode? value, out int result) => int.TryParse(value?.ToString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out result);

    private static bool tryGetActorPosition(JsonObject actor, out int x, out int y)
    {
        x = 0;
        y = 0;
        return actor["position"] is JsonArray position && position.Count >= 2 && tryGetInt(position[0], out x) && tryGetInt(position[1], out y);
    }

    private static bool tryGetLight(JsonObject light, out Point center, out double radius)
    {
        center = default;
        radius = 0;
        if (light["position"] is not JsonArray position || position.Count < 2)
            return false;
        double x = getDouble(position[0], 0);
        double y = getDouble(position[1], 0);
        radius = getDouble(light["radius"], 0);
        if (radius <= 0)
            return false;
        center = new Point(x, y);
        return true;
    }

    private static Color getLightFill(JsonObject light)
    {
        if (light["color"] is not JsonArray color || color.Count < 3)
            return Color.FromArgb(32, 255, 255, 255);
        byte r = (byte)Math.Clamp((int)getDouble(color[0], 255), 0, 255);
        byte g = (byte)Math.Clamp((int)getDouble(color[1], 255), 0, 255);
        byte b = (byte)Math.Clamp((int)getDouble(color[2], 255), 0, 255);
        byte a = color.Count > 3 ? (byte)Math.Clamp((int)getDouble(color[3], 255), 0, 255) : (byte)255;
        return Color.FromArgb((byte)Math.Clamp((int)(a * 0.15), 12, 80), r, g, b);
    }

    private static double getDouble(JsonNode? node, double fallback)
    {
        return double.TryParse(node?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ? value : fallback;
    }

    private void recordMapEditSnapshot()
    {
        if (mapEditSnapshotRecorded || gameData is null)
            return;
        gameData.RecordSnapshot();
        mapEditSnapshotRecorded = true;
    }

    private void markMapModified() => gameData?.refreshModifiedState();

    private void invalidateActorRenderStates()
    {
        disposeActorPreviewLeases();
        actorRenderStates.Clear();
        actorRenderStatesDirty = true;
        animationStateDirty = true;
    }

    private void invalidatePendingActorRenderState()
    {
        disposeActorPreviewLease(pendingActorRenderState?.PreviewLease);
        pendingActorRenderState = null;
        pendingActorRenderStateDirty = true;
        animationStateDirty = true;
    }

    private void disposeViewportRenderCaches()
    {
        checkerboardRenderCache?.Dispose();
        checkerboardRenderCache = null;
        foreach (LayerRenderCache cache in layerRenderCaches.Values)
            cache.Dispose();
        layerRenderCaches.Clear();
        dirtyLayerNames.Clear();
        cacheGeometry = null;
    }

    private void disposeMapRenderCaches()
    {
        animationTimer.Stop();
        tileBrushRenderTimer.Stop();
        pendingBrushLayerNames.Clear();
        animatedAutoTileLayerNames.Clear();
        hasAnimatedActors = false;
        animationStateDirty = true;
        disposeViewportRenderCaches();
    }

    private void disposeRenderResources()
    {
        disposeMapRenderCaches();
        invalidateActorRenderStates();
        invalidatePendingActorRenderState();
        disposeCachedBitmaps();
        autoTileRenderer?.Dispose();
    }

    private void disposeCachedBitmaps()
    {
        foreach (CachedBitmap bitmap in bitmapCache.Values)
            bitmap.Image.Dispose();
        foreach (Bitmap bitmap in hueCache.Values)
            bitmap.Dispose();
        foreach (Bitmap bitmap in retiredBitmaps)
            bitmap.Dispose();
        bitmapCache.Clear();
        hueCache.Clear();
        retiredBitmaps.Clear();
    }

    private void onActorVisualsInvalidated(object? sender, EventArgs args)
    {
        invalidateActorRenderStates();
        invalidatePendingActorRenderState();
        disposeCachedBitmaps();
        InvalidateVisual();
    }

    private sealed class ViewportRenderCache(RenderTargetBitmap bitmap, Rect viewport) : IDisposable
    {
        public RenderTargetBitmap Bitmap { get; } = bitmap;
        public Rect Viewport { get; } = viewport;

        public void Dispose()
        {
            Bitmap.Dispose();
        }
    }

    private readonly record struct CachedBitmap(DateTime ModifiedAt, long Length, Bitmap Image);

    private sealed class LayerRenderCache(RenderTargetBitmap bitmap, Rect viewport) : IDisposable
    {
        public RenderTargetBitmap Bitmap { get; } = bitmap;
        public Rect Viewport { get; } = viewport;

        public void Dispose()
        {
            Bitmap.Dispose();
        }
    }

    private sealed class ActorRenderState(
        JsonObject actor,
        Bitmap? image,
        Rect baseSource,
        Vector translation,
        Vector scale,
        Vector origin,
        double rotation,
        bool animated,
        double interval,
        int frameCount,
        ActorPreviewLease? previewLease)
    {
        public JsonObject Actor { get; } = actor;
        public Bitmap? Image { get; } = image;
        public Rect BaseSource { get; } = baseSource;
        public Vector Translation { get; } = translation;
        public Vector Scale { get; } = scale;
        public Vector Origin { get; } = origin;
        public double Rotation { get; } = rotation;
        public bool Animated { get; } = animated;
        public double Interval { get; } = interval;
        public int FrameCount { get; } = frameCount;
        public ActorPreviewLease? PreviewLease { get; } = previewLease;

        public static ActorRenderState Missing(JsonObject actor)
        {
            return new ActorRenderState(
                actor,
                null,
                default,
                Vector.Zero,
                new Vector(1, 1),
                Vector.Zero,
                0,
                false,
                0.2,
                1,
                null);
        }
    }

    private readonly record struct CacheGeometry(int MapWidth, int MapHeight, int TileSize, double RenderScale, Rect Viewport);
    private readonly record struct MapZoomAnchor(double MapX, double MapY, Point ViewportPoint);
}

public sealed class TileSelectionChangedEventArgs(TileSelection? tiles, string? autoTileKey) : EventArgs
{
    public TileSelection? Tiles { get; } = tiles;
    public string? AutoTileKey { get; } = autoTileKey;
}

public sealed class LightSelectionChangedEventArgs(string mapKey, int? index, JsonObject? lightData) : EventArgs
{
    public string MapKey { get; } = mapKey;
    public int? Index { get; } = index;
    public JsonObject? LightData { get; } = lightData;
}

public sealed class LightDataChangedEventArgs(string mapKey, int index, JsonObject lightData) : EventArgs
{
    public string MapKey { get; } = mapKey;
    public int Index { get; } = index;
    public JsonObject LightData { get; } = lightData;
}

public sealed class ActorSelectionChangedEventArgs(
    string mapKey,
    string? layerName,
    int? index,
    JsonObject? actorData) : EventArgs
{
    public string MapKey { get; } = mapKey;
    public string? LayerName { get; } = layerName;
    public int? Index { get; } = index;
    public JsonObject? ActorData { get; } = actorData;
    public string? BlueprintReference { get; } = actorData?["bp"]?.GetValue<string>();
}
