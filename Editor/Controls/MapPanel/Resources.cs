using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
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
            || previewService is null
            || string.IsNullOrWhiteSpace(assetPath)
            || !previewService.TryResolveTextureFile(assetPath, out string filePath))
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
            if (new Rect((actor.PreviewLease?.FrameOffset.X ?? 0) - actor.Origin.X, (actor.PreviewLease?.FrameOffset.Y ?? 0) - actor.Origin.Y, source.Width, source.Height).Contains(local))
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

    internal Rect? GetVisibleMapBounds()
    {
        if (!tryGetMapSize(out int width, out int height))
            return null;
        Rect mapRect = getMapRect(width, height);
        Rect visible = getVisibleLocalMapRect(mapRect);
        return visible.Width > 0 && visible.Height > 0
            ? new Rect(mapRect.X + visible.X, mapRect.Y + visible.Y, visible.Width, visible.Height)
            : null;
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

    private void onTileBrushRenderTick(object? sender, EventArgs args)
    {
        flushPendingBrushLayers();
    }

    private void scheduleBrushLayerRefresh(string layerName)
    {
        pendingBrushLayerNames.Add(layerName);
        if (!tileBrushRenderTimer.IsEnabled)
            tileBrushRenderTimer.Start();
    }

    private void flushPendingBrushLayers()
    {
        tileBrushRenderTimer.Stop();
        if (pendingBrushLayerNames.Count == 0)
            return;
        pendingBrushLayerNames.Clear();
        InvalidateVisual();
    }

    private MapLayerSnapshot? getLayer(string name) => CurrentMapDocument?.Layers.GetValueOrDefault(name);

    private static bool isLayerVisible(MapLayerSnapshot layer) => layer.Visible;

    private static bool tryGetInt(JsonNode? value, out int result) => int.TryParse(value?.ToString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out result);

    private static bool tryGetActorPosition(MapActorSnapshot actor, out int x, out int y) => actor.TryGetGridPosition(out x, out y);

    private static double getDouble(JsonNode? node, double fallback)
    {
        return double.TryParse(node?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ? value : fallback;
    }

    private void endMapGesture()
    {
        tilePaintPointer = null;
        tilePaintChanged = false;
        if (nativePreview is not null)
            nativePreview.IsInteracting = false;
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
        actorPointerGesture = null;
        rectangleStart = null;
        tileBrushDragging = false;
        actorMoveIndex = null;
        actorMoveLayer = null;
        movingRuntimeActorId = null;
    }

    private void onMapDataChanged(object? sender, MapPreviewChangedEventArgs args)
    {
        if (gameData is null || CurrentMapKey is null || args.MapKey is not null
            && !string.Equals(CurrentMapKey, args.MapKey, StringComparison.Ordinal))
            return;
        if (actorPointerGesture is not null)
            actorPointerGesture.LastAppliedOffset = null;
        if (args.Edit is not null && CurrentMapDocument is not null)
        {
            long started = Stopwatch.GetTimestamp();
            Dictionary<MapActorReference, int> renamed = captureSelectedActorRenames(args.Edit);
            MapDocumentCodec.ApplyEdits(CurrentMapDocument, args.Edit);
            remapSelectedActorTags(renamed);
            if (nativeScene is not null && previewService?.MapScenes.UpdateActors(nativeScene, CurrentMapKey, CurrentMapDocument.Data, args.Edit) == true)
                nativeSceneRevision++;
            else
                nativeScene = null;
            nativePreview?.RecordSceneUpdate(started);
            if (!IsRuntimeEditing && args.Edit.Edits.Any(edit => edit.Kind != JsonDataEdit.Operation.Set && edit.Path[0] is "actors"))
            {
                cancelMapGesture();
            }
            foreach (string layer in args.Edit.Layers)
                scheduleBrushLayerRefresh(layer);
            if (args.Edit.ChangesActors)
            {
                if (IsRuntimeEditing)
                    reconcileRuntimeActorSelection();
                else if (!args.Edit.ChangesOnlyActorPositions)
                    reconcileProjectActorSelection(renamed.Count != 0);
                updateActorRenderStates(args.Edit);
                ActorDataChanged?.Invoke(this, args);
            }
        }
        else
        {
            nativeScene = null;
            if (args.ReloadData)
            {
                cancelMapGesture();
                CurrentMapDocument = editingContext?.ReadMapDocument(CurrentMapKey);
                if (IsRuntimeEditing)
                    reconcileRuntimeActorSelection();
            }
            disposeMapRenderCaches();
            invalidateActorRenderStates();
            tilesetPaths.Clear();
            if (args.ReloadData)
            {
                if (IsRuntimeEditing)
                    setSelectedActor(selectedActorLayer, selectedActorIndex, true, true);
                else
                    reconcileProjectActorSelection(true);
                ActorDataChanged?.Invoke(this, args);
            }
            InvalidateMeasure();
        }
        InvalidateVisual();
    }

    private void invalidateActorRenderStates()
    {
        disposeActorPreviewLeases();
        actorRenderStates.Clear();
        actorRenderStatesDirty = true;
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
    }

    private void disposeViewportRenderCaches()
    {
        checkerboardRenderCache?.Dispose();
        checkerboardRenderCache = null;
        cacheGeometry = null;
    }

    private void disposeMapRenderCaches()
    {
        tileBrushRenderTimer.Stop();
        pendingBrushLayerNames.Clear();
        disposeViewportRenderCaches();
    }

    private void disposeRenderResources()
    {
        nativePreview?.Dispose();
        nativePreview = null;
        if (previewService is not null)
        {
            previewService.MapPreviews.Invalidated -= onNativePreviewInvalidated;
            previewService.MapPreviews.OptionsChanged -= onNativeFrameChanged;
        }
        cancelMapGesture();
        if (gameData is not null)
        {
            gameData.DataReloaded -= onMapImagesRestored;
            gameData.DataRestored -= onMapImagesRestored;
        }
        if (editingContext is not null)
            editingContext.Changed -= onMapDataChanged;
        disposeMapRenderCaches();
        invalidateActorRenderStates();
        invalidatePendingActorRenderState();
        disposeCachedBitmaps();
    }

    private void disposeCachedBitmaps()
    {
        foreach (CachedBitmap bitmap in bitmapCache.Values)
            bitmap.Image.Dispose();
        foreach (Bitmap bitmap in retiredBitmaps)
            bitmap.Dispose();
        bitmapCache.Clear();
        retiredBitmaps.Clear();
    }

    private void onActorVisualsInvalidated(object? sender, EventArgs args)
    {
        invalidateActorRenderStates();
        invalidatePendingActorRenderState();
        InvalidateVisual();
    }

    private void onMapImagesRestored(object? sender, EventArgs args)
    {
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
}
