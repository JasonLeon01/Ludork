using Avalonia;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed partial class WorldMapPreviewRenderer : IDisposable
{
    private static readonly IBrush MissingTilesetBrush = new SolidColorBrush(Color.FromArgb(90, 90, 120, 150));
    private static readonly IBrush ActorMarkerBrush = new SolidColorBrush(Color.FromArgb(220, 255, 196, 64));
    private static readonly Pen ActorMarkerPen = new(new SolidColorBrush(Color.FromArgb(230, 40, 30, 10)), 1);
    private readonly GameDataService gameData;
    private readonly AutoTileRenderer autoTileRenderer;
    private readonly WorldMapActorPreviewRenderer? actorRenderer;
    private readonly Dictionary<PreviewChunkKey, CachedPreviewChunk> previewCache = [];
    private readonly Dictionary<PreviewChunkKey, PendingPreviewChunk> pendingChunks = [];
    private readonly Queue<PreviewChunkKey> chunkQueue = [];
    private long previewAccessOrder;
    private long previewCacheBytes;
    private bool chunkWorkScheduled;
    private bool disposed;

    public WorldMapPreviewRenderer(
        GameDataService gameData,
        BlueprintPreviewService? previewService = null)
    {
        this.gameData = gameData;
        autoTileRenderer = new AutoTileRenderer(gameData, key => getResourceImage(true, key));
        if (previewService is not null)
        {
            actorRenderer = new WorldMapActorPreviewRenderer(previewService);
            actorRenderer.PreviewChanged += onActorPreviewChanged;
        }
        gameData.MapPreviewChanged += onMapPreviewChanged;
        initializeResourceWatcher();
    }

    public event EventHandler? PreviewChanged;

    public void DrawMap(
        DrawingContext context,
        string mapKey,
        JsonObject map,
        Point origin,
        double cellSize,
        Rect clip)
    {
        JsonObject? layers = map["layers"] as JsonObject;
        HashSet<string> renderedActorGroups = new(StringComparer.Ordinal);
        IReadOnlyList<string> layerOrder = layers is null ? [] : getLayerOrder(map, layers);
        foreach (string layerName in layerOrder)
        {
            renderedActorGroups.Add(layerName);
            DrawMapLayer(context, mapKey, map, layerName, origin, cellSize, clip);
        }
        DrawRemainingActors(context, mapKey, map, renderedActorGroups, origin, cellSize, clip);
        CompleteFrame();
    }

    public void DrawMapLayer(
        DrawingContext context,
        string mapKey,
        JsonObject map,
        string layerName,
        Point origin,
        double cellSize,
        Rect clip)
    {
        if (!tryGetPreviewViewport(map, origin, cellSize, clip, out PreviewViewport viewport))
            return;
        JsonObject? layer = map["layers"]?[layerName] as JsonObject;
        if (layer?["visible"]?.GetValue<bool?>() == false)
            return;
        if (layer is not null)
        {
            int firstChunkX = viewport.MinX / viewport.ChunkCellCount;
            int firstChunkY = viewport.MinY / viewport.ChunkCellCount;
            int lastChunkX = (viewport.MaxX - 1) / viewport.ChunkCellCount;
            int lastChunkY = (viewport.MaxY - 1) / viewport.ChunkCellCount;
            for (int chunkY = firstChunkY; chunkY <= lastChunkY; chunkY += 1)
            {
                for (int chunkX = firstChunkX; chunkX <= lastChunkX; chunkX += 1)
                {
                    int chunkMinX = chunkX * viewport.ChunkCellCount;
                    int chunkMinY = chunkY * viewport.ChunkCellCount;
                    int chunkMaxX = Math.Min(viewport.Width, chunkMinX + viewport.ChunkCellCount);
                    int chunkMaxY = Math.Min(viewport.Height, chunkMinY + viewport.ChunkCellCount);
                    PreviewChunkKey key = new(
                        mapKey,
                        layerName,
                        viewport.ScaleBucket,
                        viewport.ChunkCellCount,
                        chunkX,
                        chunkY);
                    CachedPreviewChunk? chunk = getPreviewChunk(
                        key,
                        layer,
                        viewport.RenderCellSize,
                        chunkMinX,
                        chunkMinY,
                        chunkMaxX,
                        chunkMaxY);
                    if (chunk is null)
                        continue;
                    Rect source = new(0, 0, chunk.Bitmap.PixelSize.Width, chunk.Bitmap.PixelSize.Height);
                    Rect destination = new(
                        origin.X + chunkMinX * cellSize,
                        origin.Y + chunkMinY * cellSize,
                        (chunkMaxX - chunkMinX) * cellSize,
                        (chunkMaxY - chunkMinY) * cellSize);
                    context.DrawImage(chunk.Bitmap, source, destination);
                }
            }
        }
        drawActorGroup(
            context,
            mapKey,
            map,
            map["actors"]?[layerName] as JsonArray,
            origin,
            cellSize,
            clip);
    }

    public void DrawRemainingActors(
        DrawingContext context,
        string mapKey,
        JsonObject map,
        IReadOnlySet<string> renderedActorGroups,
        Point origin,
        double cellSize,
        Rect clip)
    {
        if (map["actors"] is not JsonObject actorGroups)
            return;
        foreach (KeyValuePair<string, JsonNode?> group in actorGroups)
        {
            if (renderedActorGroups.Contains(group.Key) || group.Value is not JsonArray actors)
                continue;
            drawActorGroup(context, mapKey, map, actors, origin, cellSize, clip);
        }
    }

    public void CompleteFrame()
    {
        actorRenderer?.TrimCache();
    }

    public void ClearPendingWork()
    {
        actorRenderer?.ClearPendingWork();
        pendingChunks.Clear();
        chunkQueue.Clear();
    }

    public void ResetView()
    {
        ClearPendingWork();
        clearResourceCache();
        actorRenderer?.InvalidateMap(null);
    }

    public void TrimMapCache(IReadOnlyCollection<string> pinnedMapKeys)
    {
        gameData.TrimWorldChildCache(pinnedMapKeys);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        disposeResourceWatcher();
        ClearPendingWork();
        gameData.MapPreviewChanged -= onMapPreviewChanged;
        if (actorRenderer is not null)
        {
            actorRenderer.PreviewChanged -= onActorPreviewChanged;
            actorRenderer.Dispose();
        }
        autoTileRenderer.Dispose();
        clearPreviewCache();
    }

    private static bool tryGetPreviewViewport(
        JsonObject map,
        Point origin,
        double cellSize,
        Rect clip,
        out PreviewViewport viewport)
    {
        int width = getInt(map["width"]);
        int height = getInt(map["height"]);
        if (width <= 0 || height <= 0 || cellSize <= 0)
        {
            viewport = default;
            return false;
        }
        Rect mapBounds = new(origin, new Size(width * cellSize, height * cellSize));
        Rect visible = mapBounds.Intersect(clip);
        if (visible.Width <= 0 || visible.Height <= 0)
        {
            viewport = default;
            return false;
        }
        int minX = Math.Clamp((int)Math.Floor((visible.Left - origin.X) / cellSize), 0, width - 1);
        int minY = Math.Clamp((int)Math.Floor((visible.Top - origin.Y) / cellSize), 0, height - 1);
        int maxX = Math.Clamp((int)Math.Ceiling((visible.Right - origin.X) / cellSize), minX + 1, width);
        int maxY = Math.Clamp((int)Math.Ceiling((visible.Bottom - origin.Y) / cellSize), minY + 1, height);
        double renderCellSize = Math.Clamp(Math.Round(cellSize * 2) / 2, 0.5, 96);
        int chunkCellCount = Math.Clamp((int)Math.Floor(384 / renderCellSize), 1, 32);
        viewport = new PreviewViewport(
            width,
            height,
            minX,
            minY,
            maxX,
            maxY,
            renderCellSize,
            chunkCellCount,
            (int)Math.Round(renderCellSize * 2));
        return true;
    }

    private CachedPreviewChunk? getPreviewChunk(
        PreviewChunkKey key,
        JsonObject layer,
        double cellSize,
        int minX,
        int minY,
        int maxX,
        int maxY)
    {
        previewAccessOrder += 1;
        if (previewCache.TryGetValue(key, out CachedPreviewChunk? cached))
        {
            cached.LastUsed = previewAccessOrder;
            return cached;
        }
        if (!disposed && pendingChunks.Count < 128 && !pendingChunks.ContainsKey(key))
        {
            pendingChunks[key] = new PendingPreviewChunk(layer, cellSize, minX, minY, maxX, maxY);
            chunkQueue.Enqueue(key);
            scheduleChunkWork();
        }
        return null;
    }

    private void scheduleChunkWork()
    {
        if (disposed || chunkWorkScheduled || chunkQueue.Count == 0)
            return;
        chunkWorkScheduled = true;
        Dispatcher.UIThread.Post(buildPendingChunks, DispatcherPriority.Background);
    }

    private void buildPendingChunks()
    {
        chunkWorkScheduled = false;
        if (disposed)
            return;
        Stopwatch budget = Stopwatch.StartNew();
        bool changed = false;
        while (chunkQueue.TryDequeue(out PreviewChunkKey key))
        {
            if (!pendingChunks.Remove(key, out PendingPreviewChunk? request))
                continue;
            CachedPreviewChunk chunk = createPreviewChunk(request);
            previewCache[key] = chunk;
            previewCacheBytes += chunk.Bytes;
            changed = true;
            if (budget.Elapsed.TotalMilliseconds >= 4)
                break;
        }
        trimPreviewCache();
        releaseRetiredResources();
        if (changed)
            PreviewChanged?.Invoke(this, EventArgs.Empty);
        scheduleChunkWork();
    }

    private CachedPreviewChunk createPreviewChunk(PendingPreviewChunk request)
    {
        (JsonObject layer, double cellSize, int minX, int minY, int maxX, int maxY) = request;
        int width = Math.Max(1, (int)Math.Ceiling((maxX - minX) * cellSize));
        int height = Math.Max(1, (int)Math.Ceiling((maxY - minY) * cellSize));
        RenderTargetBitmap bitmap = new(new PixelSize(width, height), new Vector(96, 96));
        using (DrawingContext context = bitmap.CreateDrawingContext())
        {
            Point origin = new(-minX * cellSize, -minY * cellSize);
            drawLayer(context, layer, origin, cellSize, minX, minY, maxX, maxY);
        }
        return new CachedPreviewChunk(bitmap, (long)width * height * 4, ++previewAccessOrder);
    }

    private void trimPreviewCache()
    {
        const int maximumChunks = 4096;
        const long maximumBytes = 128L * 1024L * 1024L;
        if (previewCache.Count <= maximumChunks && previewCacheBytes <= maximumBytes)
            return;
        foreach (PreviewChunkKey key in previewCache
                     .OrderBy(item => item.Value.LastUsed)
                     .Select(item => item.Key)
                     .ToArray())
        {
            if (previewCache.Count <= maximumChunks && previewCacheBytes <= maximumBytes)
                break;
            CachedPreviewChunk chunk = previewCache[key];
            previewCache.Remove(key);
            previewCacheBytes -= chunk.Bytes;
            chunk.Bitmap.Dispose();
        }
    }

    private void onMapPreviewChanged(object? sender, MapPreviewChangedEventArgs args)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => onMapPreviewChanged(sender, args));
            return;
        }
        if (disposed)
            return;
        actorRenderer?.InvalidateMap(args.MapKey);
        if (args.MapKey is null)
        {
            clearResourceCache();
            ClearPendingWork();
            clearPreviewCache();
        }
        else
        {
            foreach (PreviewChunkKey key in pendingChunks.Keys
                         .Where(key => string.Equals(key.MapKey, args.MapKey, StringComparison.Ordinal)).ToArray())
                pendingChunks.Remove(key);
            foreach (PreviewChunkKey key in previewCache.Keys
                         .Where(key => string.Equals(key.MapKey, args.MapKey, StringComparison.Ordinal))
                         .ToArray())
            {
                CachedPreviewChunk chunk = previewCache[key];
                previewCache.Remove(key);
                previewCacheBytes -= chunk.Bytes;
                chunk.Bitmap.Dispose();
            }
        }
        PreviewChanged?.Invoke(this, EventArgs.Empty);
    }

    private void onActorPreviewChanged(object? sender, EventArgs args)
    {
        PreviewChanged?.Invoke(this, EventArgs.Empty);
    }

    private void clearPreviewCache()
    {
        foreach (CachedPreviewChunk chunk in previewCache.Values)
            chunk.Bitmap.Dispose();
        previewCache.Clear();
        previewCacheBytes = 0;
    }

    private void drawLayer(
        DrawingContext context,
        JsonObject layer,
        Point origin,
        double cellSize,
        int minX,
        int minY,
        int maxX,
        int maxY)
    {
        if (layer["visible"]?.GetValue<bool?>() == false)
            return;
        JsonArray? tiles = layer["tiles"] as JsonArray;
        JsonArray? autoTiles = layer["autoTiles"] as JsonArray;
        Bitmap? tileset = getResourceImage(false, getString(layer["layerTileset"]));
        int sourceTileSize = Math.Max(1, gameData.getCellSize());
        for (int y = minY; y < maxY; y++)
        {
            JsonArray? tileRow = getRow(tiles, y);
            JsonArray? autoTileRow = getRow(autoTiles, y);
            for (int x = minX; x < maxX; x++)
            {
                Rect destination = new(
                    origin.X + x * cellSize,
                    origin.Y + y * cellSize,
                    cellSize,
                    cellSize);
                string? autoTileKey = getString(getValue(autoTileRow, x));
                if (!string.IsNullOrWhiteSpace(autoTileKey) && autoTiles is not null)
                {
                    autoTileRenderer.drawTile(context, autoTileKey, autoTiles, x, y, destination, 0);
                    continue;
                }
                if (!tryGetInt(getValue(tileRow, x), out int tileNumber) || tileNumber < 0)
                    continue;
                if (tileset is null)
                {
                    context.FillRectangle(MissingTilesetBrush, destination);
                    continue;
                }
                int columns = Math.Max(1, tileset.PixelSize.Width / sourceTileSize);
                int sourceX = tileNumber % columns * sourceTileSize;
                int sourceY = tileNumber / columns * sourceTileSize;
                Rect source = new(sourceX, sourceY, sourceTileSize, sourceTileSize);
                if (source.Right <= tileset.PixelSize.Width && source.Bottom <= tileset.PixelSize.Height)
                    context.DrawImage(tileset, source, destination);
            }
        }
    }

    private void drawActorGroup(
        DrawingContext context,
        string mapKey,
        JsonObject map,
        JsonArray? actors,
        Point origin,
        double cellSize,
        Rect clip)
    {
        if (actorRenderer is not null)
        {
            actorRenderer.DrawActorGroup(context, mapKey, map, actors, origin, cellSize, clip);
            return;
        }
        if (actors is null)
            return;
        double markerSize = Math.Clamp(cellSize * 0.42, 2, 9);
        foreach (JsonNode? node in actors)
        {
            if (node is not JsonObject actor
                || actor["position"] is not JsonArray { Count: >= 2 } position
                || !tryGetInt(position[0], out int x)
                || !tryGetInt(position[1], out int y))
            {
                continue;
            }
            Point center = new(
                origin.X + (x + 0.5) * cellSize,
                origin.Y + (y + 0.5) * cellSize);
            if (!clip.Contains(center))
                continue;
            context.DrawEllipse(
                ActorMarkerBrush,
                ActorMarkerPen,
                center,
                markerSize / 2,
                markerSize / 2);
        }
    }

    private static IReadOnlyList<string> getLayerOrder(JsonObject map, JsonObject layers)
    {
        List<string> result = [];
        if (map["layerOrder"] is JsonArray order)
        {
            foreach (JsonNode? node in order)
            {
                string? name = getString(node);
                if (!string.IsNullOrWhiteSpace(name) && layers.ContainsKey(name) && !result.Contains(name, StringComparer.Ordinal))
                    result.Add(name);
            }
        }
        foreach (string name in layers.Select(entry => entry.Key))
        {
            if (!result.Contains(name, StringComparer.Ordinal))
                result.Add(name);
        }
        return result;
    }

    private static JsonArray? getRow(JsonArray? grid, int index)
    {
        return grid is not null && index >= 0 && index < grid.Count
            ? grid[index] as JsonArray
            : null;
    }

    private static JsonNode? getValue(JsonArray? row, int index)
    {
        return row is not null && index >= 0 && index < row.Count ? row[index] : null;
    }

    private static int getInt(JsonNode? value)
    {
        return tryGetInt(value, out int result) ? result : 0;
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    internal static bool tryGetInt(JsonNode? value, out int result)
    {
        if (value is JsonValue scalar)
        {
            if (scalar.TryGetValue(out int integer))
            {
                result = integer;
                return true;
            }
            if (scalar.TryGetValue(out long longValue))
            {
                result = (int)Math.Clamp(longValue, int.MinValue, int.MaxValue);
                return true;
            }
            if (scalar.TryGetValue(out double number) && double.IsFinite(number))
            {
                result = (int)Math.Clamp(number, int.MinValue, int.MaxValue);
                return true;
            }
        }
        return int.TryParse(value?.ToString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out result);
    }

    private readonly record struct PreviewChunkKey(
        string MapKey,
        string LayerName,
        int ScaleBucket,
        int ChunkCellCount,
        int X,
        int Y);

    private sealed record PendingPreviewChunk(JsonObject Layer, double CellSize, int MinX, int MinY, int MaxX, int MaxY);

    private readonly record struct PreviewViewport(
        int Width,
        int Height,
        int MinX,
        int MinY,
        int MaxX,
        int MaxY,
        double RenderCellSize,
        int ChunkCellCount,
        int ScaleBucket);

    private sealed class CachedPreviewChunk(
        RenderTargetBitmap bitmap,
        long bytes,
        long lastUsed)
    {
        public RenderTargetBitmap Bitmap { get; } = bitmap;
        public long Bytes { get; } = bytes;
        public long LastUsed { get; set; } = lastUsed;
    }
}
