using Avalonia;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class WorldMapActorPreviewRenderer : IDisposable
{
    private const int MaximumVisuals = 256;
    private const int MaximumResolvedActors = 2048;
    private static readonly IBrush MissingActorBrush = new SolidColorBrush(Color.FromArgb(190, 48, 112, 176));
    private static readonly IBrush PendingActorBrush = new SolidColorBrush(Color.FromArgb(150, 100, 100, 100));
    private static readonly Pen ActorErrorPen = new(new SolidColorBrush(Color.FromArgb(230, 235, 70, 70)), 1);
    private readonly BlueprintPreviewService previewService;
    private readonly Dictionary<ActorSourceKey, ResolvedActorVisual> resolvedActors = [];
    private readonly Dictionary<ActorPreviewFrameKey, CachedActorVisual> visuals = [];
    private readonly HashSet<ActorPreviewFrameKey> pinnedVisuals = [];
    private readonly Dictionary<ActorSourceKey, (JsonObject Map, JsonObject Actor)> pendingActors = [];
    private readonly Queue<ActorSourceKey> actorQueue = [];
    private readonly Dictionary<ActorPreviewFrameKey, ActorVisualDescriptor> pendingVisuals = [];
    private readonly Queue<ActorPreviewFrameKey> visualQueue = [];
    private long accessOrder;
    private bool previewChangePending;
    private bool trimPending;
    private bool workScheduled;
    private bool disposed;

    public WorldMapActorPreviewRenderer(BlueprintPreviewService previewService)
    {
        this.previewService = previewService;
        previewService.VisualsInvalidated += onVisualsInvalidated;
    }

    public event EventHandler? PreviewChanged;

    public void DrawActorGroup(
        DrawingContext context,
        string mapKey,
        JsonObject map,
        JsonArray? actors,
        Point origin,
        double cellSize,
        Rect clip)
    {
        if (disposed || actors is null || cellSize <= 0)
            return;
        for (int index = 0; index < actors.Count; index += 1)
        {
            if (actors[index] is not JsonObject actor
                || actor["position"] is not JsonArray { Count: >= 2 } position
                || !WorldMapPreviewRenderer.tryGetInt(position[0], out int gridX)
                || !WorldMapPreviewRenderer.tryGetInt(position[1], out int gridY))
            {
                continue;
            }
            ActorVisualDescriptor? descriptor = resolveActor(mapKey, map, actor, index, out bool pending);
            if (descriptor is null)
            {
                drawPlaceholder(context, origin, cellSize, gridX, gridY, clip, pending ? PendingActorBrush : MissingActorBrush, null);
                continue;
            }
            Rect destination = new(
                -descriptor.Origin.X,
                -descriptor.Origin.Y,
                descriptor.BaseTextureRect.Width,
                descriptor.BaseTextureRect.Height);
            Matrix transform = createActorTransform(origin, cellSize, gridX, gridY, descriptor);
            Rect transformedBounds = destination.TransformToAABB(transform);
            if (!transformedBounds.Intersects(clip))
                continue;
            ActorPreviewLease? lease = getVisual(descriptor);
            if (lease is null || lease.Frame is not Bitmap image)
            {
                drawPlaceholder(
                    context,
                    origin,
                    cellSize,
                    gridX,
                    gridY,
                    clip,
                    lease?.ShaderError is null ? PendingActorBrush : MissingActorBrush,
                    lease?.ShaderError is null ? null : ActorErrorPen);
                continue;
            }
            PixelRect sourceRect = lease.SourceRect;
            Rect source = new(sourceRect.X, sourceRect.Y, sourceRect.Width, sourceRect.Height);
            destination = new(
                -descriptor.Origin.X,
                -descriptor.Origin.Y,
                source.Width,
                source.Height);
            using (context.PushTransform(transform))
            {
                using (context.PushOpacity(descriptor.MapPreviewOpacity))
                    context.DrawImage(image, source, destination);
                if (lease.ShaderError is not null)
                    context.DrawRectangle(null, ActorErrorPen, destination);
            }
        }
    }

    public void InvalidateMap(string? mapKey)
    {
        if (mapKey is null)
        {
            resolvedActors.Clear();
            ClearPendingWork();
            return;
        }
        foreach (ActorSourceKey key in pendingActors.Keys
                     .Where(key => string.Equals(key.MapKey, mapKey, StringComparison.Ordinal)).ToArray())
            pendingActors.Remove(key);
        foreach (ActorSourceKey key in resolvedActors.Keys
                     .Where(key => string.Equals(key.MapKey, mapKey, StringComparison.Ordinal))
                     .ToArray())
        {
            resolvedActors.Remove(key);
        }
    }

    public void InvalidateResources() => onVisualsInvalidated(this, EventArgs.Empty);

    public void ClearPendingWork()
    {
        pendingActors.Clear();
        actorQueue.Clear();
        pendingVisuals.Clear();
        visualQueue.Clear();
    }

    public void TrimCache()
    {
        if (disposed || trimPending)
            return;
        trimPending = true;
        Dispatcher.UIThread.Post(trimCache, DispatcherPriority.Background);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        ClearPendingWork();
        previewService.VisualsInvalidated -= onVisualsInvalidated;
        clearVisuals();
        resolvedActors.Clear();
        pinnedVisuals.Clear();
    }

    private ActorVisualDescriptor? resolveActor(
        string mapKey,
        JsonObject map,
        JsonObject actor,
        int index,
        out bool pending)
    {
        string reference = actor["bp"]?.GetValue<string>() ?? string.Empty;
        string tag = actor["tag"]?.GetValue<string>() ?? string.Empty;
        ActorSourceKey key = new(mapKey, reference, tag, tag.Length == 0 ? index : -1);
        accessOrder += 1;
        if (resolvedActors.TryGetValue(key, out ResolvedActorVisual? cached))
        {
            pending = false;
            cached.LastUsed = accessOrder;
            return cached.Descriptor;
        }
        pending = true;
        if (pendingActors.Count < MaximumResolvedActors && pendingActors.TryAdd(key, (map, actor)))
        {
            actorQueue.Enqueue(key);
            scheduleWork();
        }
        return null;
    }

    private ActorPreviewLease? getVisual(ActorVisualDescriptor descriptor)
    {
        ActorPreviewFrameKey key = new(
            descriptor.TexturePath,
            descriptor.BaseTextureRect,
            descriptor.ShaderPath,
            descriptor.Hue);
        pinnedVisuals.Add(key);
        accessOrder += 1;
        if (visuals.TryGetValue(key, out CachedActorVisual? cached))
        {
            cached.LastUsed = accessOrder;
            return cached.Lease;
        }
        if (pendingVisuals.Count < MaximumVisuals && pendingVisuals.TryAdd(key, descriptor))
        {
            visualQueue.Enqueue(key);
            scheduleWork();
        }
        return null;
    }

    private void scheduleWork()
    {
        if (disposed || workScheduled)
            return;
        workScheduled = true;
        Dispatcher.UIThread.Post(preparePreviews, DispatcherPriority.Background);
    }

    private void preparePreviews()
    {
        workScheduled = false;
        if (disposed)
            return;
        Stopwatch budget = Stopwatch.StartNew();
        using (IDisposable resolutionBatch = previewService.BeginResolutionBatch())
        {
            while (actorQueue.Count != 0 || visualQueue.Count != 0)
            {
                if (actorQueue.TryDequeue(out ActorSourceKey actorKey)
                    && pendingActors.Remove(actorKey, out (JsonObject Map, JsonObject Actor) source))
                {
                    ActorVisualDescriptor? descriptor;
                    try
                    {
                        descriptor = previewService.tryResolveMapActorVisual(source.Map, source.Actor);
                    }
                    catch (IOException)
                    {
                        descriptor = null;
                    }
                    resolvedActors[actorKey] = new ResolvedActorVisual(descriptor, ++accessOrder);
                }
                if (visualQueue.TryDequeue(out ActorPreviewFrameKey visualKey)
                    && pendingVisuals.Remove(visualKey, out ActorVisualDescriptor? visual))
                {
                    ActorPreviewLease lease = previewService.ActorPreviews.AcquireStatic(visual);
                    lease.FrameChanged += onFrameChanged;
                    visuals[visualKey] = new CachedActorVisual(lease, ++accessOrder);
                }
                if (budget.Elapsed.TotalMilliseconds >= 4)
                    break;
            }
        }
        schedulePreviewChanged();
        if (actorQueue.Count != 0 || visualQueue.Count != 0)
            scheduleWork();
    }

    private static Matrix createActorTransform(
        Point origin,
        double cellSize,
        int gridX,
        int gridY,
        ActorVisualDescriptor descriptor)
    {
        double displayScale = cellSize / EngineConstants.CellSize;
        double radians = descriptor.Rotation * Math.PI / 180.0;
        double cos = Math.Cos(radians);
        double sin = Math.Sin(radians);
        double scaleX = descriptor.Scale.X * displayScale;
        double scaleY = descriptor.Scale.Y * displayScale;
        return new Matrix(
            cos * scaleX,
            sin * scaleX,
            -sin * scaleY,
            cos * scaleY,
            origin.X + gridX * cellSize + descriptor.Translation.X * displayScale,
            origin.Y + gridY * cellSize + descriptor.Translation.Y * displayScale);
    }

    private static void drawPlaceholder(
        DrawingContext context,
        Point origin,
        double cellSize,
        int gridX,
        int gridY,
        Rect clip,
        IBrush brush,
        Pen? pen)
    {
        double inset = Math.Min(cellSize * 0.15, 2);
        Rect destination = new(
            origin.X + gridX * cellSize + inset,
            origin.Y + gridY * cellSize + inset,
            Math.Max(1, cellSize - inset * 2),
            Math.Max(1, cellSize - inset * 2));
        if (destination.Intersects(clip))
            context.DrawRectangle(brush, pen, destination);
    }

    private void onFrameChanged(object? sender, EventArgs args)
    {
        schedulePreviewChanged();
    }

    private void onVisualsInvalidated(object? sender, EventArgs args)
    {
        ClearPendingWork();
        resolvedActors.Clear();
        pinnedVisuals.Clear();
        clearVisuals();
        schedulePreviewChanged();
    }

    private void trimCache()
    {
        trimPending = false;
        if (disposed)
            return;
        trimResolvedActors();
        if (visuals.Count > MaximumVisuals)
        {
            int removeCount = visuals.Count - MaximumVisuals;
            foreach (ActorPreviewFrameKey key in visuals
                         .Where(entry => !pinnedVisuals.Contains(entry.Key))
                         .OrderBy(entry => entry.Value.LastUsed)
                         .Select(entry => entry.Key)
                         .Take(removeCount)
                         .ToArray())
            {
                disposeVisual(key);
            }
        }
        pinnedVisuals.Clear();
    }

    private void schedulePreviewChanged()
    {
        if (disposed || previewChangePending)
            return;
        previewChangePending = true;
        Dispatcher.UIThread.Post(
            () =>
            {
                previewChangePending = false;
                if (!disposed)
                    PreviewChanged?.Invoke(this, EventArgs.Empty);
            },
            DispatcherPriority.Background);
    }

    private void clearVisuals()
    {
        foreach (CachedActorVisual visual in visuals.Values)
        {
            visual.Lease.FrameChanged -= onFrameChanged;
            visual.Lease.Dispose();
        }
        visuals.Clear();
    }

    private void disposeVisual(ActorPreviewFrameKey key)
    {
        if (!visuals.Remove(key, out CachedActorVisual? visual))
            return;
        visual.Lease.FrameChanged -= onFrameChanged;
        visual.Lease.Dispose();
    }

    private void trimResolvedActors()
    {
        if (resolvedActors.Count <= MaximumResolvedActors)
            return;
        foreach (ActorSourceKey key in resolvedActors
                     .OrderBy(entry => entry.Value.LastUsed)
                     .Select(entry => entry.Key)
                     .Take(resolvedActors.Count - MaximumResolvedActors)
                     .ToArray())
        {
            resolvedActors.Remove(key);
        }
    }

    private readonly record struct ActorSourceKey(
        string MapKey,
        string BlueprintReference,
        string Tag,
        int Index);

    private readonly record struct ActorPreviewFrameKey(
        string TexturePath,
        PixelRect TextureRect,
        string ShaderPath,
        double Hue);
    private sealed class ResolvedActorVisual(
        ActorVisualDescriptor? descriptor,
        long lastUsed)
    {
        public ActorVisualDescriptor? Descriptor { get; } = descriptor;
        public long LastUsed { get; set; } = lastUsed;
    }

    private sealed class CachedActorVisual(
        ActorPreviewLease lease,
        long lastUsed)
    {
        public ActorPreviewLease Lease { get; } = lease;
        public long LastUsed { get; set; } = lastUsed;
    }
}
