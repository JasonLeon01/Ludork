using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.VisualTree;
using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Plugins.OfficialRandomMap.UI;

internal sealed class RandomMapCanvas : Control, IDisposable
{
    private const int MinimumCellSize = 12;
    private const int MaximumCellSize = 96;
    private const int CellSizeStep = 4;
    private static readonly IBrush BackgroundBrush =
        PluginTheme.Brush("Surface");
    private static readonly IBrush EmptyBrush =
        PluginTheme.Brush("Surface");
    private static readonly IBrush AutoTileBrush =
        new SolidColorBrush(Color.FromArgb(145, 66, 133, 244));
    private static readonly IBrush MissingActorBrush =
        new SolidColorBrush(Color.FromArgb(160, 0, 120, 255));
    private static readonly Pen MissingActorPen =
        new(new SolidColorBrush(Color.FromArgb(220, 255, 255, 255)), 1);
    private static readonly IBrush ActorOriginBrush =
        new SolidColorBrush(Color.FromArgb(220, 0, 255, 0));
    private static readonly IBrush MarkerBrush =
        new SolidColorBrush(Color.Parse("#fbbc04"));
    private static readonly Pen MarkerPen =
        new(PluginTheme.Brush("Surface"), 2);
    private static readonly Pen GridPen =
        new(new SolidColorBrush(Color.FromArgb(65, 255, 255, 255)), 1);

    private readonly IMapEditorHost host;
    private readonly Dictionary<string, Bitmap> bitmapCache =
        new(StringComparer.Ordinal);
    private readonly Dictionary<string, ActorMaterialPreview> actorMaterialPreviews =
        [];
    private readonly Dictionary<string, PluginTilesetSnapshot> tilesetCache =
        new(StringComparer.Ordinal);
    private readonly HashSet<string> missingTilesets =
        new(StringComparer.Ordinal);
    private readonly HashSet<string> unavailableImages =
        new(StringComparer.Ordinal);
    private readonly HashSet<(int X, int Y)> markers = [];
    private readonly EditorZoomInput zoomInput = new();
    private ScrollViewer? hostScrollViewer;
    private PluginMapSnapshot? snapshot;
    private string? selectedLayerName;
    private int cellSize = 32;
    private double continuousCellSize = 32;
    private bool markerMode;
    private bool selectionMode;
    private MapRegion region;
    private (int X, int Y)? selectionStart;
    private MapRegion? pendingRegion;
    private IPointer? selectionPointer;
    private readonly EditorZoomAnchor pendingMapZoomAnchor = new();

    public RandomMapCanvas(IMapEditorHost host)
    {
        this.host = host;
        MinWidth = 540;
        MinHeight = 360;
        Focusable = true;
        PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
        AttachedToVisualTree += (_, _) => { foreach (ActorMaterialPreview preview in actorMaterialPreviews.Values) preview.IsActive = true; };
        DetachedFromVisualTree += (_, _) => { foreach (ActorMaterialPreview preview in actorMaterialPreviews.Values) preview.IsActive = false; };
    }

    public event EventHandler<MapMarkerEventArgs>? MarkerToggled;
    public event EventHandler<MapRegionEventArgs>? RegionChanged;

    public void SetRegion(MapRegion nextRegion, bool nextSelectionMode)
    {
        selectionPointer?.Capture(null);
        selectionPointer = null;
        region = nextRegion;
        selectionMode = nextSelectionMode;
        selectionStart = null;
        pendingRegion = null;
        InvalidateVisual();
    }

    public void SetMap(
        PluginMapSnapshot? nextSnapshot,
        string? nextSelectedLayerName,
        IEnumerable<(int X, int Y)> nextMarkers,
        bool nextMarkerMode)
    {
        if (!ReferenceEquals(snapshot, nextSnapshot))
        {
            foreach (ActorMaterialPreview preview in actorMaterialPreviews.Values)
                preview.Dispose();
            actorMaterialPreviews.Clear();
        }
        snapshot = nextSnapshot;
        selectedLayerName = nextSelectedLayerName;
        markerMode = nextMarkerMode;
        markers.Clear();
        markers.UnionWith(nextMarkers);
        updateContentSize();
        InvalidateVisual();
    }

    public void SetSelectedLayer(string? layerName)
    {
        selectedLayerName = layerName;
        InvalidateVisual();
    }

    public void SetMarkers(
        IEnumerable<(int X, int Y)> nextMarkers,
        bool nextMarkerMode)
    {
        markerMode = nextMarkerMode;
        markers.Clear();
        markers.UnionWith(nextMarkers);
        InvalidateVisual();
    }

    public override void Render(DrawingContext context)
    {
        context.FillRectangle(BackgroundBrush, Bounds);
        if (snapshot is null || snapshot.Width <= 0 || snapshot.Height <= 0)
            return;
        Rect mapRect = getMapRect();
        Rect visibleMapRect = getVisibleLocalMapRect(mapRect);
        if (visibleMapRect.Width <= 0 || visibleMapRect.Height <= 0)
            return;
        using (context.PushClip(mapRect))
        using (context.PushTransform(
                   Matrix.CreateTranslation(mapRect.X, mapRect.Y)))
        {
            Rect localMapRect = new(
                0,
                0,
                snapshot.Width * (double)cellSize,
                snapshot.Height * (double)cellSize);
            context.FillRectangle(EmptyBrush, localMapRect);
            foreach (PluginMapLayerSnapshot layer in snapshot.Layers)
            {
                double opacity = selectedLayerName is null
                    || string.Equals(
                        layer.Name,
                        selectedLayerName,
                        StringComparison.Ordinal)
                    ? 1.0
                    : 0.5;
                using (context.PushOpacity(opacity))
                    drawLayer(context, layer, visibleMapRect);
            }
            drawSelection(context, localMapRect);
            drawGrid(context, visibleMapRect);
            drawMarkers(context, visibleMapRect);
        }
    }

    protected override Size MeasureOverride(Size availableSize)
    {
        if (snapshot is null)
            return new Size(MinWidth, MinHeight);
        return new Size(
            Math.Max(MinWidth, snapshot.Width * (double)cellSize),
            Math.Max(MinHeight, snapshot.Height * (double)cellSize));
    }

    protected override void OnAttachedToVisualTree(
        VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        LayoutUpdated += onLayoutUpdated;
        bindHostScrollViewer(this.FindAncestorOfType<ScrollViewer>());
    }

    protected override void OnDetachedFromVisualTree(
        VisualTreeAttachmentEventArgs args)
    {
        LayoutUpdated -= onLayoutUpdated;
        pendingMapZoomAnchor.Clear();
        bindHostScrollViewer(null);
        base.OnDetachedFromVisualTree(args);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        if (snapshot is null || (!selectionMode && !markerMode))
            return;
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed || !getMapRect().Contains(point.Position))
            return;
        (int x, int y) = getCell(point.Position);
        if (selectionMode)
        {
            selectionStart = (x, y);
            pendingRegion = MapRegion.FromPoints(x, y, x, y);
            selectionPointer = args.Pointer;
            args.Pointer.Capture(this);
            Focus();
            InvalidateVisual();
        }
        else if (region.Contains(x, y))
        {
            MarkerToggled?.Invoke(this, new MapMarkerEventArgs(x, y));
        }
        args.Handled = true;
    }

    protected override void OnPointerMoved(PointerEventArgs args)
    {
        base.OnPointerMoved(args);
        if (selectionStart is not { } start)
            return;
        (int x, int y) = getCell(args.GetPosition(this));
        pendingRegion = MapRegion.FromPoints(start.X, start.Y, x, y);
        InvalidateVisual();
        args.Handled = true;
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs args)
    {
        base.OnPointerReleased(args);
        if (selectionStart is not { } start || args.InitialPressMouseButton != MouseButton.Left)
            return;
        (int x, int y) = getCell(args.GetPosition(this));
        MapRegion selected = MapRegion.FromPoints(start.X, start.Y, x, y);
        selectionStart = null;
        pendingRegion = null;
        selectionPointer = null;
        args.Pointer.Capture(null);
        RegionChanged?.Invoke(this, new MapRegionEventArgs(selected));
        InvalidateVisual();
        args.Handled = true;
    }

    protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs args)
    {
        base.OnPointerCaptureLost(args);
        selectionStart = null;
        pendingRegion = null;
        selectionPointer = null;
        InvalidateVisual();
    }

    private (int X, int Y) getCell(Point point)
    {
        Rect mapRect = getMapRect();
        return (
            Math.Clamp((int)Math.Floor((point.X - mapRect.X) / cellSize), 0, snapshot!.Width - 1),
            Math.Clamp((int)Math.Floor((point.Y - mapRect.Y) / cellSize), 0, snapshot!.Height - 1));
    }

    private void drawSelection(DrawingContext context, Rect mapRect)
    {
        MapRegion selected = pendingRegion ?? region;
        if (selected.Width <= 0 || selected.Height <= 0)
            return;
        Rect selection = new(selected.X * (double)cellSize, selected.Y * (double)cellSize,
            selected.Width * (double)cellSize, selected.Height * (double)cellSize);
        IBrush shade = new SolidColorBrush(Color.FromArgb(85, 0, 0, 0));
        context.FillRectangle(shade, new Rect(0, 0, mapRect.Width, selection.Top));
        context.FillRectangle(shade, new Rect(0, selection.Bottom, mapRect.Width, mapRect.Height - selection.Bottom));
        context.FillRectangle(shade, new Rect(0, selection.Top, selection.Left, selection.Height));
        context.FillRectangle(shade, new Rect(selection.Right, selection.Top, mapRect.Width - selection.Right, selection.Height));
        context.DrawRectangle(null, new Pen(MarkerBrush, 2), selection.Deflate(1));
    }

    protected override void OnPointerWheelChanged(PointerWheelEventArgs args)
    {
        base.OnPointerWheelChanged(args);
        if (zoomInput.ShouldSuppressWheel())
        {
            args.Handled = true;
            return;
        }
        if (!EditorZoomInput.ShouldZoomWheel(args.KeyModifiers, false))
            return;
        int direction = args.Delta.Y > 0 ? 1 : args.Delta.Y < 0 ? -1 : 0;
        if (direction == 0)
            return;
        setCellSize(
            cellSize + direction * CellSizeStep,
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
        double nextCellSize = EditorZoomInput.ScaleByFactor(
            continuousCellSize,
            EditorZoomInput.GetMagnifyFactor(args.Delta.Y),
            MinimumCellSize,
            MaximumCellSize);
        setCellSize(
            nextCellSize,
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

    private void setCellSize(
        double nextContinuousCellSize,
        Point contentPoint,
        Point viewportPoint)
    {
        continuousCellSize = Math.Clamp(
            nextContinuousCellSize,
            MinimumCellSize,
            MaximumCellSize);
        int nextCellSize = Math.Clamp(
            (int)Math.Round(
                continuousCellSize,
                MidpointRounding.AwayFromZero),
            MinimumCellSize,
            MaximumCellSize);
        if (nextCellSize == cellSize)
            return;
        if (snapshot is not null && snapshot.Width > 0 && snapshot.Height > 0)
        {
            Rect mapRect = getMapRect();
            pendingMapZoomAnchor.Capture(contentPoint, viewportPoint, mapRect.Position, cellSize);
        }
        else
        {
            pendingMapZoomAnchor.Clear();
        }
        cellSize = nextCellSize;
        updateContentSize();
        InvalidateVisual();
    }

    private void onLayoutUpdated(object? sender, EventArgs args)
    {
        if (!pendingMapZoomAnchor.IsPending)
            return;
        applyMapZoomAnchor();
    }

    private void applyMapZoomAnchor()
    {
        if (snapshot is null)
        {
            pendingMapZoomAnchor.Clear();
            return;
        }
        pendingMapZoomAnchor.Apply(hostScrollViewer, getMapRect().Position, cellSize);
    }

    public void Dispose()
    {
        LayoutUpdated -= onLayoutUpdated;
        pendingMapZoomAnchor.Clear();
        bindHostScrollViewer(null);
        foreach (ActorMaterialPreview preview in actorMaterialPreviews.Values)
            preview.Dispose();
        actorMaterialPreviews.Clear();
        foreach (Bitmap bitmap in bitmapCache.Values)
            bitmap.Dispose();
        bitmapCache.Clear();
        tilesetCache.Clear();
        missingTilesets.Clear();
        unavailableImages.Clear();
    }

    private void drawLayer(
        DrawingContext context,
        PluginMapLayerSnapshot layer,
        Rect visibleMapRect)
    {
        if (snapshot is null)
            return;
        PluginTilesetSnapshot? tileset = getTileset(layer.TilesetKey);
        Bitmap? bitmap = getBitmap(tileset);
        int columns = bitmap is null || tileset is null
            ? 1
            : Math.Max(
                1,
                bitmap.PixelSize.Width / Math.Max(1, tileset.TileWidth));
        int startX = Math.Clamp(
            (int)Math.Floor(visibleMapRect.Left / cellSize),
            0,
            snapshot.Width);
        int endX = Math.Clamp(
            (int)Math.Ceiling(visibleMapRect.Right / cellSize),
            0,
            snapshot.Width);
        int startY = Math.Clamp(
            (int)Math.Floor(visibleMapRect.Top / cellSize),
            0,
            snapshot.Height);
        int endY = Math.Clamp(
            (int)Math.Ceiling(visibleMapRect.Bottom / cellSize),
            0,
            snapshot.Height);
        for (int y = startY; y < endY; y++)
        {
            IReadOnlyList<int?>? tileRow =
                y < layer.Tiles.Count ? layer.Tiles[y] : null;
            IReadOnlyList<string?>? autoTileRow =
                y < layer.AutoTiles.Count ? layer.AutoTiles[y] : null;
            for (int x = startX; x < endX; x++)
            {
                Rect destination = new(
                    x * (double)cellSize,
                    y * (double)cellSize,
                    cellSize,
                    cellSize);
                string? autoTile = autoTileRow is not null
                    && x < autoTileRow.Count
                    ? autoTileRow[x]
                    : null;
                if (!string.IsNullOrWhiteSpace(autoTile))
                {
                    context.FillRectangle(AutoTileBrush, destination.Deflate(1));
                    continue;
                }
                int? tile = tileRow is not null && x < tileRow.Count
                    ? tileRow[x]
                    : null;
                if (tile is not int tileNumber
                    || tileNumber < 0
                    || bitmap is null
                    || tileset is null)
                {
                    continue;
                }
                int sourceX =
                    tileNumber % columns * tileset.TileWidth;
                int sourceY =
                    tileNumber / columns * tileset.TileHeight;
                Rect source = new(
                    sourceX,
                    sourceY,
                    tileset.TileWidth,
                    tileset.TileHeight);
                if (source.Right <= bitmap.PixelSize.Width
                    && source.Bottom <= bitmap.PixelSize.Height)
                {
                    context.DrawImage(bitmap, source, destination);
                }
            }
        }
        int sourceCellSize = Math.Max(1, host.CellSize);
        drawActors(
            context,
            layer.Actors,
            visibleMapRect,
            sourceCellSize);
    }

    private void drawActors(
        DrawingContext context,
        IReadOnlyList<PluginMapActorSnapshot> actors,
        Rect visibleMapRect,
        int sourceCellSize)
    {
        foreach (PluginMapActorSnapshot actor in actors)
        {
            Bitmap? sourceBitmap = getBitmap(actor.AssetPath);
            PluginMapActorRectSnapshot sourceRect =
                actor.SourceRect ?? new PluginMapActorRectSnapshot(0, 0, host.CellSize, host.CellSize);
            Rect destination = new(
                -actor.OriginX,
                -actor.OriginY,
                sourceRect.Width,
                sourceRect.Height);
            ActorMaterialPreview? preview = getMaterialPreview(actor);
            Bitmap? drawBitmap = preview?.Bitmap ?? sourceBitmap;
            Rect drawnSource = preview?.Bitmap is not null
                ? new Rect(0, 0, drawBitmap!.PixelSize.Width, drawBitmap.PixelSize.Height)
                : new Rect(sourceRect.X, sourceRect.Y, sourceRect.Width, sourceRect.Height);
            Rect drawnDestination = preview?.Bitmap is not null
                ? new Rect(preview.Offset.X - actor.OriginX, preview.Offset.Y - actor.OriginY, drawnSource.Width, drawnSource.Height)
                : destination;
            Matrix transform = createActorTransform(actor, sourceCellSize);
            Rect transformedBounds = getTransformedBounds(
                drawnDestination,
                transform);
            if (transformedBounds.Intersects(visibleMapRect))
            {
                using (context.PushTransform(transform))
                {
                    if (isValidSourceRect(sourceBitmap, sourceRect))
                    {
                        using (context.PushOpacity(actor.PreviewOpacity))
                            context.DrawImage(
                                drawBitmap!,
                                drawnSource,
                                drawnDestination);
                    }
                    else
                    {
                        context.FillRectangle(
                            MissingActorBrush,
                            destination);
                        context.DrawRectangle(
                            null,
                            MissingActorPen,
                            destination);
                    }
                }
            }
            drawActorOrigin(context, actor, visibleMapRect);
        }
    }

    private void drawActorOrigin(
        DrawingContext context,
        PluginMapActorSnapshot actor,
        Rect visibleMapRect)
    {
        Rect cell = new(
            actor.X * (double)cellSize,
            actor.Y * (double)cellSize,
            cellSize,
            cellSize);
        if (!cell.Intersects(visibleMapRect))
            return;
        Point center = new(
            (actor.X + 0.5) * cellSize,
            (actor.Y + 0.5) * cellSize);
        context.DrawEllipse(
            ActorOriginBrush,
            null,
            center,
            3,
            3);
    }

    private Matrix createActorTransform(
        PluginMapActorSnapshot actor,
        int sourceCellSize)
    {
        double displayScale = cellSize / (double)sourceCellSize;
        double radians = Matrix.ToRadians(actor.Rotation);
        double cosine = Math.Cos(radians);
        double sine = Math.Sin(radians);
        return new Matrix(
            displayScale * cosine * actor.ScaleX,
            displayScale * sine * actor.ScaleX,
            -displayScale * sine * actor.ScaleY,
            displayScale * cosine * actor.ScaleY,
            actor.X * (double)cellSize
                + displayScale * actor.TranslationX,
            actor.Y * (double)cellSize
                + displayScale * actor.TranslationY);
    }

    private static Rect getTransformedBounds(
        Rect bounds,
        Matrix transform)
    {
        Point topLeft = transform.Transform(bounds.TopLeft);
        Point topRight = transform.Transform(bounds.TopRight);
        Point bottomLeft = transform.Transform(bounds.BottomLeft);
        Point bottomRight = transform.Transform(bounds.BottomRight);
        double left = Math.Min(
            Math.Min(topLeft.X, topRight.X),
            Math.Min(bottomLeft.X, bottomRight.X));
        double top = Math.Min(
            Math.Min(topLeft.Y, topRight.Y),
            Math.Min(bottomLeft.Y, bottomRight.Y));
        double right = Math.Max(
            Math.Max(topLeft.X, topRight.X),
            Math.Max(bottomLeft.X, bottomRight.X));
        double bottom = Math.Max(
            Math.Max(topLeft.Y, topRight.Y),
            Math.Max(bottomLeft.Y, bottomRight.Y));
        return new Rect(left, top, right - left, bottom - top);
    }

    private static bool isValidSourceRect(
        Bitmap? bitmap,
        PluginMapActorRectSnapshot sourceRect)
    {
        return bitmap is not null
            && sourceRect.X >= 0
            && sourceRect.Y >= 0
            && sourceRect.Width > 0
            && sourceRect.Height > 0
            && sourceRect.X + sourceRect.Width
                <= bitmap.PixelSize.Width
            && sourceRect.Y + sourceRect.Height
                <= bitmap.PixelSize.Height;
    }

    private void drawGrid(
        DrawingContext context,
        Rect visibleMapRect)
    {
        if (snapshot is null)
            return;
        double width = snapshot.Width * (double)cellSize;
        double height = snapshot.Height * (double)cellSize;
        int startX = Math.Clamp(
            (int)Math.Floor(visibleMapRect.Left / cellSize),
            0,
            snapshot.Width);
        int endX = Math.Clamp(
            (int)Math.Ceiling(visibleMapRect.Right / cellSize),
            0,
            snapshot.Width);
        int startY = Math.Clamp(
            (int)Math.Floor(visibleMapRect.Top / cellSize),
            0,
            snapshot.Height);
        int endY = Math.Clamp(
            (int)Math.Ceiling(visibleMapRect.Bottom / cellSize),
            0,
            snapshot.Height);
        for (int x = startX; x <= endX; x++)
        {
            double position = x * (double)cellSize;
            context.DrawLine(
                GridPen,
                new Point(position, 0),
                new Point(position, height));
        }
        for (int y = startY; y <= endY; y++)
        {
            double position = y * (double)cellSize;
            context.DrawLine(
                GridPen,
                new Point(0, position),
                new Point(width, position));
        }
    }

    private void drawMarkers(
        DrawingContext context,
        Rect visibleMapRect)
    {
        foreach ((int x, int y) in markers.OrderBy(
                     marker => marker.Y * (snapshot?.Width ?? 0) + marker.X))
        {
            Rect cell = new(
                x * (double)cellSize,
                y * (double)cellSize,
                cellSize,
                cellSize);
            if (!cell.Intersects(visibleMapRect))
                continue;
            Point center = new(
                (x + 0.5) * cellSize,
                (y + 0.5) * cellSize);
            double radius = Math.Max(4, cellSize * 0.24);
            context.DrawEllipse(
                MarkerBrush,
                MarkerPen,
                center,
                radius,
                radius);
        }
    }

    private PluginTilesetSnapshot? getTileset(string key)
    {
        if (string.IsNullOrWhiteSpace(key))
            return null;
        if (missingTilesets.Contains(key))
            return null;
        if (tilesetCache.TryGetValue(
                key,
                out PluginTilesetSnapshot? cached))
        {
            return cached;
        }
        PluginTilesetSnapshot tileset;
        try
        {
            tileset = host.ReadTileset(key);
        }
        catch (KeyNotFoundException)
        {
            missingTilesets.Add(key);
            return null;
        }
        tilesetCache[key] = tileset;
        return tileset;
    }

    private Bitmap? getBitmap(PluginTilesetSnapshot? tileset)
    {
        return tileset is null ? null : getBitmap(tileset.AssetPath);
    }

    private Bitmap? getBitmap(string assetPath)
    {
        if (string.IsNullOrWhiteSpace(assetPath))
        {
            return null;
        }
        if (unavailableImages.Contains(assetPath))
            return null;
        if (bitmapCache.TryGetValue(
                assetPath,
                out Bitmap? cached))
        {
            return cached;
        }
        string filePath = host.ResolveAssetFile(assetPath);
        if (filePath.Length == 0)
        {
            unavailableImages.Add(assetPath);
            return null;
        }
        Bitmap bitmap;
        try
        {
            bitmap = new Bitmap(filePath);
        }
        catch (Exception)
        {
            unavailableImages.Add(assetPath);
            return null;
        }
        bitmapCache[assetPath] = bitmap;
        return bitmap;
    }

    private ActorMaterialPreview? getMaterialPreview(PluginMapActorSnapshot actor)
    {
        if (actor.Material.Length == 0)
            return null;
        string key = actor.AssetPath + "|" + actor.SourceRect + "|" + actor.Material + "|" + actor.MaterialParameters.ToJsonString();
        if (!actorMaterialPreviews.TryGetValue(key, out ActorMaterialPreview? preview))
        {
            preview = new ActorMaterialPreview(host.AcquireActorPreview(actor), InvalidateVisual);
            actorMaterialPreviews.Add(key, preview);
        }
        return preview;
    }

    private Rect getMapRect()
    {
        if (snapshot is null)
            return default;
        double width = snapshot.Width * (double)cellSize;
        double height = snapshot.Height * (double)cellSize;
        return new Rect(
            Math.Max(0, (Bounds.Width - width) / 2),
            Math.Max(0, (Bounds.Height - height) / 2),
            width,
            height);
    }

    private Rect getVisibleLocalMapRect(Rect mapRect)
    {
        Rect viewport = hostScrollViewer is null
            ? new Rect(0, 0, Bounds.Width, Bounds.Height)
            : new Rect(
                hostScrollViewer.Offset.X,
                hostScrollViewer.Offset.Y,
                hostScrollViewer.Viewport.Width,
                hostScrollViewer.Viewport.Height);
        double left = Math.Max(viewport.Left, mapRect.Left);
        double top = Math.Max(viewport.Top, mapRect.Top);
        double right = Math.Min(viewport.Right, mapRect.Right);
        double bottom = Math.Min(viewport.Bottom, mapRect.Bottom);
        if (right <= left || bottom <= top)
            return default;
        return new Rect(
            left - mapRect.X,
            top - mapRect.Y,
            right - left,
            bottom - top);
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

    private void onHostScrollChanged(
        object? sender,
        ScrollChangedEventArgs args)
    {
        InvalidateVisual();
    }

    private void updateContentSize()
    {
        InvalidateMeasure();
    }

}
