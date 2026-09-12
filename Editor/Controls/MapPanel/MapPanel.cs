using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using Ludork.Models;
using Ludork.ViewModels;
using System;
using System.Collections.Generic;
using System.Diagnostics;
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

public sealed partial class MapPanel : Control
{
    private const int SourceTileSize = EngineConstants.CellSize;
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
    private readonly Dictionary<string, CachedBitmap> bitmapCache = new(StringComparer.Ordinal);
    private readonly Dictionary<string, Bitmap> hueCache = new(StringComparer.Ordinal);
    private readonly List<Bitmap> retiredBitmaps = [];
    private readonly Dictionary<string, LayerRenderCache> layerRenderCaches = new(StringComparer.Ordinal);
    private readonly Dictionary<string, List<ActorRenderState>> actorRenderStates = new(StringComparer.Ordinal);
    private readonly HashSet<string> dirtyLayerNames = new(StringComparer.Ordinal);
    private readonly HashSet<string> pendingBrushLayerNames = new(StringComparer.Ordinal);
    private readonly HashSet<string> animatedAutoTileLayerNames = new(StringComparer.Ordinal);
    private readonly EditorZoomInput zoomInput = new();
    private GameDataService? gameData;
    private IMapEditingContext? editingContext;
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
    private long mapEditGesture;
    private IPointer? capturedPointer;
    private readonly Dictionary<string, string?> tilesetPaths = new(StringComparer.Ordinal);
    private int? selectedLightIndex;
    private int? selectedActorIndex;
    private string? selectedActorLayer;
    private string? selectedRuntimeActorId;
    private bool lightMoveDragging;
    private bool lightRadiusDragging;
    private Vector lightDragOffset;
    private Point lightDragCenter;
    private int? actorMoveIndex;
    private string? actorMoveLayer;
    private string? movingRuntimeActorId;
    private (int X, int Y) actorMoveOffset;
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
    public string? SelectedRuntimeActorId => selectedRuntimeActorId;
    public bool IsSelectedLayerEditable => selectedLayerEditable;
    public bool IsRuntimeEditing => editingContext?.IsRuntime == true;
    private bool canEditMap => selectedLayerEditable && editingContext?.IsEditable == true;
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
        endMapGesture();
        gameData = nextGameData;
        ConfigureEditingContext(new ProjectMapEditingContext(nextGameData));
        tilesetPaths.Clear();
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

    public void ConfigureEditingContext(IMapEditingContext context)
    {
        if (ReferenceEquals(editingContext, context))
            return;
        CancelInteractions();
        if (editingContext is not null)
            editingContext.Changed -= onMapDataChanged;
        editingContext = context;
        editingContext.Changed += onMapDataChanged;
        setPendingActor(null);
        setSelectedActor(null, null, true, true);
        if (CurrentMapKey is not null)
            refreshMap(CurrentMapKey, context.ReadMapSnapshot(CurrentMapKey));
    }

    public void CancelInteractions()
    {
        cancelMapGesture();
        capturedPointer?.Capture(null);
        capturedPointer = null;
        hoverGrid = null;
        flushPendingBrushLayers();
        InvalidateVisual();
    }

    private void capturePointer(IPointer pointer)
    {
        capturedPointer = pointer;
        pointer.Capture(this);
    }

    public MapPanelViewportState CaptureViewport()
        => new(tileSize, continuousTileSize, hostScrollViewer?.Offset ?? default);

    public void RestoreViewport(MapPanelViewportState state)
    {
        pendingMapZoomAnchor = null;
        tileSize = Math.Clamp(state.TileSize, MinTileSize, MaxTileSize);
        continuousTileSize = Math.Clamp(state.ContinuousTileSize, MinTileSize, MaxTileSize);
        disposeMapRenderCaches();
        InvalidateMeasure();
        InvalidateVisual();
        Dispatcher.UIThread.Post(() =>
        {
            if (hostScrollViewer is not null)
                hostScrollViewer.Offset = state.Offset;
        }, DispatcherPriority.Loaded);
    }

    public void refreshMap(string? mapKey, JsonObject? mapData)
    {
        cancelMapGesture();
        CurrentMapKey = mapKey;
        CurrentMapData = mapData?.DeepClone() as JsonObject;
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
        cancelMapGesture();
        EditMode = mode;
        rectangleStart = null;
        tileBrushDragging = false;
        flushPendingBrushLayers();
        lightMoveDragging = false;
        lightRadiusDragging = false;
        actorMoveIndex = null;
        actorMoveLayer = null;
        movingRuntimeActorId = null;
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
            || getActorList(layerName) is not JsonArray actors
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

    public void updateSelectedLight(JsonObject lightData)
    {
        if (IsRuntimeEditing || editingContext?.IsEditable != true)
            return;
        if (selectedLightIndex is not int index || CurrentMapData?["lights"] is not JsonArray lights || index < 0 || index >= lights.Count || lights[index] is not JsonObject light)
            return;
        JsonObject next = (JsonObject)lightData.DeepClone();
        if (JsonNode.DeepEquals(light, next))
            return;
        if (gameData is null || CurrentMapKey is null || !gameData.UpdateMapLight(CurrentMapKey, index, light, next))
            return;
        LightDataChanged?.Invoke(this, new LightDataChangedEventArgs(CurrentMapKey ?? string.Empty, index, next));
        InvalidateVisual();
    }

    public void setPendingActor(string? blueprintReference)
    {
        string? nextPendingActor = IsRuntimeEditing || string.IsNullOrWhiteSpace(blueprintReference)
            ? null
            : blueprintReference.Trim();
        if (string.Equals(pendingActor, nextPendingActor, StringComparison.Ordinal))
            return;
        pendingActor = nextPendingActor;
        invalidatePendingActorRenderState();
        InvalidateVisual();
    }

}
