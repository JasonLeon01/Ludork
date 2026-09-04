using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.VisualTree;
using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;

namespace Ludork.Plugins.OfficialRandomMap.UI;

internal sealed class RandomMapCanvas : Control, IDisposable
{
    private const int MinimumCellSize = 12;
    private const int MaximumCellSize = 96;
    private const int CellSizeStep = 4;
    private const int SourceCellSize = 32;
    private const int CharacterSheetColumns = 4;
    private const int CharacterSheetRows = 4;
    private static readonly IBrush BackgroundBrush =
        new SolidColorBrush(Color.Parse("#262626"));
    private static readonly IBrush EmptyBrush =
        new SolidColorBrush(Color.Parse("#202124"));
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
        new(new SolidColorBrush(Color.Parse("#202124")), 2);
    private static readonly Pen GridPen =
        new(new SolidColorBrush(Color.FromArgb(65, 255, 255, 255)), 1);

    private readonly IMapEditorHost host;
    private readonly Dictionary<string, Bitmap> bitmapCache =
        new(StringComparer.Ordinal);
    private readonly Dictionary<ActorHueBitmapCacheKey, Bitmap> actorHueBitmapCache =
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
    private MapZoomAnchor? pendingMapZoomAnchor;

    public RandomMapCanvas(IMapEditorHost host)
    {
        this.host = host;
        MinWidth = 540;
        MinHeight = 360;
        Focusable = true;
        PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
    }

    public event EventHandler<MapMarkerEventArgs>? MarkerToggled;

    public void SetMap(
        PluginMapSnapshot? nextSnapshot,
        string? nextSelectedLayerName,
        IEnumerable<(int X, int Y)> nextMarkers,
        bool nextMarkerMode)
    {
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
                    : 0.38;
                using (context.PushOpacity(opacity))
                    drawLayer(context, layer, visibleMapRect);
            }
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
        pendingMapZoomAnchor = null;
        bindHostScrollViewer(null);
        base.OnDetachedFromVisualTree(args);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        if (!markerMode || snapshot is null)
            return;
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed)
            return;
        Rect mapRect = getMapRect();
        if (!mapRect.Contains(point.Position))
            return;
        int x = (int)((point.Position.X - mapRect.X) / cellSize);
        int y = (int)((point.Position.Y - mapRect.Y) / cellSize);
        if (x < 0 || y < 0 || x >= snapshot.Width || y >= snapshot.Height)
            return;
        MarkerToggled?.Invoke(this, new MapMarkerEventArgs(x, y));
        args.Handled = true;
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
        MapZoomAnchor? nextAnchor = null;
        if (snapshot is not null && snapshot.Width > 0 && snapshot.Height > 0)
        {
            Rect mapRect = getMapRect();
            nextAnchor = new MapZoomAnchor(
                (contentPoint.X - mapRect.X) / cellSize,
                (contentPoint.Y - mapRect.Y) / cellSize,
                viewportPoint);
        }
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
        cellSize = nextCellSize;
        pendingMapZoomAnchor = nextAnchor;
        updateContentSize();
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
            || snapshot is null)
        {
            pendingMapZoomAnchor = null;
            return;
        }
        pendingMapZoomAnchor = null;
        Rect mapRect = getMapRect();
        Point contentAnchor = new(
            mapRect.X + anchor.MapX * cellSize,
            mapRect.Y + anchor.MapY * cellSize);
        hostScrollViewer.Offset = EditorZoomInput.GetAnchoredOffset(
            contentAnchor,
            anchor.ViewportPoint,
            hostScrollViewer.Extent,
            hostScrollViewer.Viewport);
    }

    public void Dispose()
    {
        LayoutUpdated -= onLayoutUpdated;
        pendingMapZoomAnchor = null;
        bindHostScrollViewer(null);
        foreach (Bitmap bitmap in actorHueBitmapCache.Values)
            bitmap.Dispose();
        actorHueBitmapCache.Clear();
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
        int sourceCellSize = tileset is null
            ? SourceCellSize
            : Math.Max(1, tileset.TileWidth);
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
                getActorSourceRect(actor, sourceBitmap);
            Rect destination = new(
                -actor.OriginX,
                -actor.OriginY,
                sourceRect.Width,
                sourceRect.Height);
            Matrix transform = createActorTransform(actor, sourceCellSize);
            Rect transformedBounds = getTransformedBounds(
                destination,
                transform);
            if (transformedBounds.Intersects(visibleMapRect))
            {
                using (context.PushTransform(transform))
                {
                    if (isValidSourceRect(sourceBitmap, sourceRect))
                    {
                        Bitmap drawBitmap = getHueBitmap(
                            actor.AssetPath,
                            sourceBitmap!,
                            actor.Hue);
                        context.DrawImage(
                            drawBitmap,
                            new Rect(
                                sourceRect.X,
                                sourceRect.Y,
                                sourceRect.Width,
                                sourceRect.Height),
                            destination);
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

    private static PluginMapActorRectSnapshot getActorSourceRect(
        PluginMapActorSnapshot actor,
        Bitmap? bitmap)
    {
        if (actor.IsCharacter && bitmap is not null)
        {
            int width = Math.Max(
                1,
                bitmap.PixelSize.Width / CharacterSheetColumns);
            int height = Math.Max(
                1,
                bitmap.PixelSize.Height / CharacterSheetRows);
            int direction = Math.Clamp(
                actor.Direction,
                0,
                CharacterSheetRows - 1);
            return new PluginMapActorRectSnapshot(
                0,
                direction * height,
                width,
                height);
        }
        return actor.SourceRect
            ?? new PluginMapActorRectSnapshot(
                0,
                0,
                SourceCellSize,
                SourceCellSize);
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

    private Bitmap getHueBitmap(
        string assetPath,
        Bitmap source,
        double hue)
    {
        double normalizedHue = normalizeHue(hue);
        if (normalizedHue <= 0.0001)
            return source;
        ActorHueBitmapCacheKey key = new(assetPath, normalizedHue);
        if (actorHueBitmapCache.TryGetValue(key, out Bitmap? cached))
            return cached;
        Bitmap tinted = applyHue(source, normalizedHue);
        actorHueBitmapCache[key] = tinted;
        return tinted;
    }

    private static double normalizeHue(double hue)
    {
        if (!double.IsFinite(hue))
            return 0;
        double normalized = hue % 360;
        return normalized < 0 ? normalized + 360 : normalized;
    }

    private static Bitmap applyHue(Bitmap source, double hue)
    {
        WriteableBitmap result = new(
            source.PixelSize,
            source.Dpi,
            PixelFormat.Bgra8888,
            AlphaFormat.Unpremul);
        using (ILockedFramebuffer frame = result.Lock())
        {
            source.CopyPixels(frame);
            byte[] pixels = new byte[frame.RowBytes * frame.Size.Height];
            Marshal.Copy(frame.Address, pixels, 0, pixels.Length);
            float hueOffset = (float)(hue / 360);
            for (int y = 0; y < frame.Size.Height; y++)
            {
                for (int x = 0; x < frame.Size.Width; x++)
                {
                    int index = y * frame.RowBytes + x * 4;
                    byte alpha = pixels[index + 3];
                    if (alpha == 0)
                        continue;
                    rgbToHsv(
                        pixels[index + 2],
                        pixels[index + 1],
                        pixels[index],
                        out float pixelHue,
                        out float saturation,
                        out float value);
                    hsvToRgb(
                        (pixelHue + hueOffset) % 1,
                        saturation,
                        value,
                        out byte red,
                        out byte green,
                        out byte blue);
                    pixels[index + 2] = red;
                    pixels[index + 1] = green;
                    pixels[index] = blue;
                }
            }
            Marshal.Copy(pixels, 0, frame.Address, pixels.Length);
        }
        return result;
    }

    private static void rgbToHsv(
        byte red,
        byte green,
        byte blue,
        out float hue,
        out float saturation,
        out float value)
    {
        float redValue = red / 255f;
        float greenValue = green / 255f;
        float blueValue = blue / 255f;
        float maximum = Math.Max(
            redValue,
            Math.Max(greenValue, blueValue));
        float minimum = Math.Min(
            redValue,
            Math.Min(greenValue, blueValue));
        float delta = maximum - minimum;
        value = maximum;
        if (delta <= 0.00001f)
        {
            hue = 0;
            saturation = 0;
            return;
        }
        saturation = delta / maximum;
        if (redValue >= maximum)
            hue = (greenValue - blueValue) / delta % 6;
        else if (greenValue >= maximum)
            hue = (blueValue - redValue) / delta + 2;
        else
            hue = (redValue - greenValue) / delta + 4;
        hue /= 6;
        if (hue < 0)
            hue += 1;
    }

    private static void hsvToRgb(
        float hue,
        float saturation,
        float value,
        out byte red,
        out byte green,
        out byte blue)
    {
        float chroma = value * saturation;
        float intermediate = chroma * (
            1 - Math.Abs(hue * 6 % 2 - 1));
        float minimum = value - chroma;
        float redValue;
        float greenValue;
        float blueValue;
        switch ((int)(hue * 6) % 6)
        {
            case 0:
                redValue = chroma;
                greenValue = intermediate;
                blueValue = 0;
                break;
            case 1:
                redValue = intermediate;
                greenValue = chroma;
                blueValue = 0;
                break;
            case 2:
                redValue = 0;
                greenValue = chroma;
                blueValue = intermediate;
                break;
            case 3:
                redValue = 0;
                greenValue = intermediate;
                blueValue = chroma;
                break;
            case 4:
                redValue = intermediate;
                greenValue = 0;
                blueValue = chroma;
                break;
            default:
                redValue = chroma;
                greenValue = 0;
                blueValue = intermediate;
                break;
        }
        red = (byte)Math.Clamp(
            (redValue + minimum) * 255,
            0,
            255);
        green = (byte)Math.Clamp(
            (greenValue + minimum) * 255,
            0,
            255);
        blue = (byte)Math.Clamp(
            (blueValue + minimum) * 255,
            0,
            255);
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

    private readonly record struct ActorHueBitmapCacheKey(
        string AssetPath,
        double Hue);

    private readonly record struct MapZoomAnchor(
        double MapX,
        double MapY,
        Point ViewportPoint);
}

internal sealed class MapMarkerEventArgs(int x, int y) : EventArgs
{
    public int X { get; } = x;

    public int Y { get; } = y;
}
