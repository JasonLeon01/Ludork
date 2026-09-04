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
    private Bitmap? getTileset(string? key)
    {
        if (gameData is null || string.IsNullOrWhiteSpace(key) || !gameData.TilesetData.TryGetValue(key, out JsonObject? data))
            return null;
        string? fileName = data["fileName"]?.GetValue<string>();
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
        if (getMapBasePosition(position, width, height) is not { } basePosition)
            return;
        MenuItem add = new() { Header = LocaleService.Get("NEW_LIGHT_SOURCE") };
        add.Click += (_, _) =>
        {
            recordMapHistorySnapshot();
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
        recordMapHistorySnapshot();
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
        return gameData is null || CurrentMapKey is null
            ? string.Empty
            : MapTagService.CreateDefault(gameData, CurrentMapKey, reference, grid.X, grid.Y);
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
        recordMapHistorySnapshot();
        mapEditSnapshotRecorded = true;
    }

    private void recordMapHistorySnapshot()
    {
        if (gameData is null)
            return;
        if (CurrentMapKey is null)
            gameData.RecordSnapshot();
        else
            gameData.RecordMapSnapshot(CurrentMapKey);
    }

    private void markActorDataModified()
    {
        if (gameData is not null && CurrentMapKey is not null)
            gameData.NotifyMapActorsChanged(CurrentMapKey);
        markMapModified();
        ActorDataChanged?.Invoke(this, EventArgs.Empty);
    }

    private void markMapModified()
    {
        if (gameData is null)
            return;
        if (CurrentMapKey is not null)
            gameData.NotifyMapContentChanged(CurrentMapKey);
        gameData.refreshModifiedState();
    }

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
