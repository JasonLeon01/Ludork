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
    private Bitmap? getTileset(string? key)
    {
        if (gameData is null || string.IsNullOrWhiteSpace(key))
            return null;
        if (!tilesetPaths.TryGetValue(key, out string? fileName))
        {
            fileName = gameData.Assets.TilesetData.TryGetValue(key, out TilesetSnapshot? data)
                ? data.FileName : null;
            tilesetPaths[key] = fileName;
        }
        return loadBitmap(fileName);
    }

    private Bitmap? getActorBitmap(string? assetPath)
    {
        if (gameData is null || string.IsNullOrWhiteSpace(assetPath))
            return null;
        return loadBitmap(assetPath);
    }

    private Bitmap? loadBitmap(string? assetPath)
    {
        if (gameData is null
            || string.IsNullOrWhiteSpace(assetPath)
            || !GameAssetPath.TryResolveExistingFile(
                gameData.ProjectPath,
                assetPath,
                out string filePath))
        {
            return null;
        }
        string cacheKey = assetPath;
        FileInfo file = new(filePath);
        if (bitmapCache.TryGetValue(cacheKey, out CachedBitmap cached))
        {
            if (file.Exists
                && cached.ModifiedAt == file.LastWriteTimeUtc
                && cached.Length == file.Length)
            {
                return cached.Image;
            }
            retiredBitmaps.Add(cached.Image);
            retireHueImages(cacheKey);
            bitmapCache.Remove(cacheKey);
        }
        if (!file.Exists)
            return null;
        Bitmap image = new(filePath);
        bitmapCache[cacheKey] = new CachedBitmap(
            file.LastWriteTimeUtc,
            file.Length,
            image);
        return image;
    }

    private Bitmap getHueImage(string assetPath, Bitmap source, double hue)
    {
        string cacheKey = assetPath + "|" + (hue % 360).ToString("F3", CultureInfo.InvariantCulture);
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

    private void retireHueImages(string assetPath)
    {
        string prefix = assetPath + "|";
        foreach (string key in hueCache.Keys
            .Where(key => key.StartsWith(prefix, StringComparison.Ordinal))
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
        if (IsRuntimeEditing || editingContext?.IsEditable != true)
            return;
        if (getMapBasePosition(position, width, height) is not { } basePosition)
            return;
        MenuItem add = new() { Header = LocaleService.Get("NEW_LIGHT_SOURCE") };
        add.Click += (_, _) =>
        {
            if (IsRuntimeEditing || editingContext?.IsEditable != true || gameData is null || CurrentMapKey is null)
                return;
            int? index = gameData.Maps.AddMapLight(CurrentMapKey, basePosition.X, basePosition.Y);
            if (index is not null)
                setSelectedLightIndex(index);
            InvalidateVisual();
        };
        MenuItem delete = new() { Header = LocaleService.Get("DELETE"), IsEnabled = selectedLightIndex is not null };
        delete.Click += (_, _) => deleteSelectedLight();
        new ContextMenu { ItemsSource = new object[] { add, delete } }.Open(this);
    }

    private void deleteSelectedLight()
    {
        if (IsRuntimeEditing || editingContext?.IsEditable != true)
            return;
        if (selectedLightIndex is not int index
            || CurrentMapDocument?.Lights is not IReadOnlyList<MapLightSnapshot> lights
            || index < 0
            || index >= lights.Count)
        {
            return;
        }
        if (gameData is null || CurrentMapKey is null || lights[index] is not MapLightSnapshot light
            || !gameData.Maps.DeleteMapLight(CurrentMapKey, index, light))
            return;
        setSelectedLightIndex(null);
        InvalidateVisual();
    }

    private int? hitTestLight(Point position)
    {
        if (CurrentMapDocument?.Lights is not IReadOnlyList<MapLightSnapshot> lights)
            return null;
        int? best = null;
        double bestDistance = double.MaxValue;
        for (int index = 0; index < lights.Count; index++)
        {
            if (lights[index] is not MapLightSnapshot light || !tryGetLight(light, out Point center, out double radius))
                continue;
            double distance = getDistance(position, center);
            if (distance <= radius && (distance < bestDistance
                || LightActorSelectionEnabled && distance == bestDistance))
            {
                best = index;
                bestDistance = distance;
            }
        }
        return best;
    }

    private void setSelectedLightIndex(int? index)
    {
        MapLightSnapshot? light = null;
        if (index is int value && CurrentMapDocument?.Lights is IReadOnlyList<MapLightSnapshot> lights && value >= 0 && value < lights.Count)
            light = lights[value];
        int? nextIndex = light is null ? null : index;
        if (selectedLightIndex == nextIndex)
            return;
        if (propertyWheelTarget is not null)
            endMapGesture();
        selectedLightIndex = nextIndex;
        LightSelectionChanged?.Invoke(this, new LightSelectionChangedEventArgs(CurrentMapKey ?? string.Empty, selectedLightIndex, light?.ToJson()));
        InvalidateVisual();
    }

    private int? hitTestActor(string layerName, Point mapPosition, HashSet<MapActorSnapshot>? allowedActors = null)
    {
        ensureActorRenderStates();
        if (!actorRenderStates.TryGetValue(layerName, out List<ActorRenderState>? actors))
            return null;
        for (int index = actors.Count - 1; index >= 0; index--)
        {
            ActorRenderState actor = actors[index];
            if (allowedActors is not null && !allowedActors.Contains(actor.Actor))
                continue;
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
        if (getActorList(layerName) is not IReadOnlyList<MapActorSnapshot> actors)
            return false;
        foreach (MapActorSnapshot actor in actors)
        {
            if (tryGetActorPosition(actor, out int x, out int y) && x == grid.X && y == grid.Y)
                return true;
        }
        return false;
    }

    private MapActorSnapshot? getSelectedActor()
    {
        return selectedActorLayer is not null && selectedActorIndex is int index && getActorList(selectedActorLayer) is IReadOnlyList<MapActorSnapshot> actors && index >= 0 && index < actors.Count
            ? actors[index]
            : null;
    }

    private IReadOnlyList<MapActorSnapshot>? getActorList(string layerName)
    {
        return CurrentMapDocument?.Actors.GetValueOrDefault(layerName);
    }

    private int getTilesetColumnCount(MapLayerSnapshot layer)
    {
        Bitmap? tileset = getTileset(layer.Tileset);
        return tileset is null ? 1 : Math.Max(1, tileset.PixelSize.Width / SourceTileSize);
    }

    private bool tryGetMapSize(out int width, out int height)
    {
        width = CurrentMapDocument?.Width ?? 0;
        height = CurrentMapDocument?.Height ?? 0;
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

    private MapLayerSnapshot? getLayer(string name) => CurrentMapDocument?.Layers.GetValueOrDefault(name);

    private MapLightSnapshot? getLight(int index) => CurrentMapDocument is not null && index >= 0 && index < CurrentMapDocument.Lights.Count
        ? CurrentMapDocument.Lights[index] : null;

    private static bool isLayerVisible(MapLayerSnapshot layer) => layer.Visible;

    private static bool tryGetInt(JsonNode? value, out int result) => int.TryParse(value?.ToString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out result);

    private static bool tryGetActorPosition(MapActorSnapshot actor, out int x, out int y) => actor.TryGetGridPosition(out x, out y);

    private static bool tryGetLight(MapLightSnapshot light, out Point center, out double radius)
    {
        center = new Point(light.Position.X, light.Position.Y);
        radius = light.Radius;
        return light.HasPosition && radius > 0;
    }

    private static Color getLightFill(MapLightSnapshot light)
    {
        MapColour colour = light.Colour;
        return getLightFill(Color.FromArgb(colour.A, colour.R, colour.G, colour.B));
    }

    private static Color getLightFill(Color colour)
    {
        return Color.FromArgb((byte)Math.Clamp((int)(colour.A * 0.15), 12, 80), colour.R, colour.G, colour.B);
    }

    private static double getDouble(JsonNode? node, double fallback)
    {
        return double.TryParse(node?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ? value : fallback;
    }

    private void endMapGesture()
    {
        propertyWheelTimer.Stop();
        propertyWheelTarget = null;
        if (mapEditGesture != 0)
            gameData?.EndHistoryGesture(mapEditGesture);
        mapEditGesture = 0;
    }

    private void cancelMapGesture()
    {
        endMapGesture();
        actorPropertyDrag = null;
        rectangleStart = null;
        tileBrushDragging = false;
        lightMoveDragging = false;
        lightRadiusDragging = false;
        actorMoveIndex = null;
        actorMoveLayer = null;
        movingRuntimeActorId = null;
    }

    private void onMapDataChanged(object? sender, MapPreviewChangedEventArgs args)
    {
        if (gameData is null || CurrentMapKey is null || args.MapKey is not null
            && !string.Equals(CurrentMapKey, args.MapKey, StringComparison.Ordinal))
            return;
        if (args.Edit is not null && CurrentMapDocument is not null)
        {
            MapDocumentCodec.ApplyEdits(CurrentMapDocument, args.Edit);
            if (!IsRuntimeEditing && args.Edit.Edits.Any(edit => edit.Kind != JsonDataEdit.Operation.Set && edit.Path[0] is "actors" or "lights"))
            {
                cancelMapGesture();
                if (EditMode == MapEditMode.Light)
                    setSelectedActor(null, null, true);
            }
            foreach (string layer in args.Edit.Layers)
                scheduleBrushLayerRefresh(layer);
            if (args.Edit.ChangesActors)
            {
                if (IsRuntimeEditing)
                    reconcileRuntimeActorSelection();
                invalidateActorRenderStates();
                reconcileLightActorSelection();
                ActorDataChanged?.Invoke(this, EventArgs.Empty);
            }
        }
        else
        {
            if (args.ReloadData)
            {
                cancelMapGesture();
                CurrentMapDocument = editingContext?.ReadMapDocument(CurrentMapKey);
                if (EditMode == MapEditMode.Light)
                    setSelectedActor(null, null, true);
                if (IsRuntimeEditing)
                    reconcileRuntimeActorSelection();
            }
            disposeMapRenderCaches();
            invalidateActorRenderStates();
            tilesetPaths.Clear();
            autoTileRenderer?.Dispose();
            autoTileRenderer = new AutoTileRenderer(gameData);
            InvalidateMeasure();
        }
        InvalidateVisual();
    }

    private void invalidateActorRenderStates()
    {
        invalidateActorLightRenderStates();
        disposeActorPreviewLeases();
        actorRenderStates.Clear();
        actorRenderStatesDirty = true;
        animationStateDirty = true;
    }

    private void invalidatePendingActorRenderState()
    {
        pendingActorRequest?.Cancel();
        pendingActorRequest?.Dispose();
        pendingActorRequest = null;
        disposeActorPreviewLease(pendingActorRenderState?.PreviewLease);
        pendingActorRenderState = null;
        pendingActorImage?.Dispose();
        pendingActorImage = null;
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
        cancelMapGesture();
        if (gameData is not null)
            gameData.Documents.ContentChanged -= onActorLightSourcesChanged;
        if (editingContext is not null)
            editingContext.Changed -= onMapDataChanged;
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
        MapActorSnapshot actor,
        Bitmap? image,
        Rect baseSource,
        Vector translation,
        Vector scale,
        Vector origin,
        double rotation,
        double mapPreviewOpacity,
        bool animated,
        double interval,
        int frameCount,
        ActorPreviewLease? previewLease)
    {
        public MapActorSnapshot Actor { get; } = actor;
        public Bitmap? Image { get; } = image;
        public Rect BaseSource { get; } = baseSource;
        public Vector Translation { get; } = translation;
        public Vector Scale { get; } = scale;
        public Vector Origin { get; } = origin;
        public double Rotation { get; } = rotation;
        public double MapPreviewOpacity { get; } = mapPreviewOpacity;
        public bool Animated { get; } = animated;
        public double Interval { get; } = interval;
        public int FrameCount { get; } = frameCount;
        public ActorPreviewLease? PreviewLease { get; } = previewLease;

        public static ActorRenderState Missing(MapActorSnapshot actor)
        {
            return new ActorRenderState(
                actor,
                null,
                default,
                Vector.Zero,
                new Vector(1, 1),
                Vector.Zero,
                0,
                1,
                false,
                0.2,
                1,
                null);
        }
    }

    private readonly record struct CacheGeometry(int MapWidth, int MapHeight, int TileSize, double RenderScale, Rect Viewport);
    private readonly record struct MapZoomAnchor(double MapX, double MapY, Point ViewportPoint);
}
