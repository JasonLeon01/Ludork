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

}

