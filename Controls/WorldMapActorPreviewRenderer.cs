using Avalonia;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class WorldMapActorPreviewRenderer : IDisposable
{
    private const double SourceCellSize = 32;
    private const int MaximumVisuals = 256;
    private const int MaximumResolvedActors = 2048;
    private const int MaximumResourceStamps = 512;
    private static readonly IBrush MissingActorBrush = new SolidColorBrush(Color.FromArgb(190, 48, 112, 176));
    private static readonly IBrush PendingActorBrush = new SolidColorBrush(Color.FromArgb(150, 100, 100, 100));
    private static readonly Pen ActorErrorPen = new(new SolidColorBrush(Color.FromArgb(230, 235, 70, 70)), 1);
    private readonly string projectPath;
    private readonly BlueprintPreviewService previewService;
    private readonly Dictionary<ActorSourceKey, ResolvedActorVisual> resolvedActors = [];
    private readonly Dictionary<ActorVisualCacheKey, CachedActorVisual> visuals = [];
    private readonly Dictionary<ActorPreviewFrameKey, CachedResourceStamp> resourceStamps = [];
    private readonly HashSet<ActorVisualCacheKey> pinnedVisuals = [];
    private long accessOrder;
    private bool previewChangePending;
    private bool trimPending;
    private bool disposed;

    public WorldMapActorPreviewRenderer(
        string projectPath,
        BlueprintPreviewService previewService)
    {
        this.projectPath = projectPath;
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
        using IDisposable resolutionBatch = previewService.BeginResolutionBatch();
        for (int index = 0; index < actors.Count; index += 1)
        {
            if (actors[index] is not JsonObject actor
                || actor["position"] is not JsonArray { Count: >= 2 } position
                || !WorldMapPreviewRenderer.tryGetInt(position[0], out int gridX)
                || !WorldMapPreviewRenderer.tryGetInt(position[1], out int gridY))
            {
                continue;
            }
            ActorVisualDescriptor? descriptor = resolveActor(mapKey, map, actor, index);
            if (descriptor is null)
            {
                drawPlaceholder(context, origin, cellSize, gridX, gridY, clip, MissingActorBrush, null);
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
            ActorPreviewLease lease = getVisual(descriptor);
            if (lease.Frame is not Bitmap image)
            {
                drawPlaceholder(
                    context,
                    origin,
                    cellSize,
                    gridX,
                    gridY,
                    clip,
                    lease.ShaderError is null ? PendingActorBrush : MissingActorBrush,
                    lease.ShaderError is null ? null : ActorErrorPen);
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
            return;
        }
        foreach (ActorSourceKey key in resolvedActors.Keys
                     .Where(key => string.Equals(key.MapKey, mapKey, StringComparison.Ordinal))
                     .ToArray())
        {
            resolvedActors.Remove(key);
        }
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
        previewService.VisualsInvalidated -= onVisualsInvalidated;
        clearVisuals();
        resolvedActors.Clear();
        resourceStamps.Clear();
        pinnedVisuals.Clear();
    }

    private ActorVisualDescriptor? resolveActor(
        string mapKey,
        JsonObject map,
        JsonObject actor,
        int index)
    {
        string reference = actor["bp"]?.GetValue<string>() ?? string.Empty;
        string tag = actor["tag"]?.GetValue<string>() ?? string.Empty;
        ActorSourceKey key = new(mapKey, reference, tag, tag.Length == 0 ? index : -1);
        accessOrder += 1;
        if (resolvedActors.TryGetValue(key, out ResolvedActorVisual? cached))
        {
            cached.LastUsed = accessOrder;
            return cached.Descriptor;
        }
        ActorVisualDescriptor? descriptor = previewService.tryResolveMapActorVisual(map, actor);
        resolvedActors[key] = new ResolvedActorVisual(descriptor, accessOrder);
        return descriptor;
    }

    private ActorPreviewLease getVisual(ActorVisualDescriptor descriptor)
    {
        ActorPreviewFrameKey frame = new(
            descriptor.TexturePath,
            descriptor.BaseTextureRect,
            descriptor.ShaderPath,
            descriptor.Hue);
        ActorResourceStamp stamp = getResourceStamp(frame);
        ActorVisualCacheKey key = new(frame, stamp);
        pinnedVisuals.Add(key);
        accessOrder += 1;
        if (visuals.TryGetValue(key, out CachedActorVisual? cached))
        {
            cached.LastUsed = accessOrder;
            return cached.Lease;
        }
        foreach (ActorVisualCacheKey staleKey in visuals.Keys
                     .Where(candidate => candidate.Frame == frame && candidate.Stamp != stamp)
                     .ToArray())
        {
            disposeVisual(staleKey);
        }
        ActorPreviewLease lease = previewService.ActorPreviews.AcquireStatic(descriptor);
        lease.FrameChanged += onFrameChanged;
        visuals[key] = new CachedActorVisual(lease, accessOrder);
        return lease;
    }

    private ActorResourceStamp getResourceStamp(ActorPreviewFrameKey frame)
    {
        DateTime now = DateTime.UtcNow;
        accessOrder += 1;
        if (resourceStamps.TryGetValue(frame, out CachedResourceStamp? cached)
            && now < cached.NextCheck)
        {
            cached.LastUsed = accessOrder;
            return cached.Stamp;
        }
        FileStamp texture = getFileStamp(frame.TexturePath);
        FileStamp shader = default;
        if (frame.ShaderPath.Length != 0)
        {
            string shaderPath;
            if (Path.IsPathRooted(frame.ShaderPath))
                shaderPath = frame.ShaderPath;
            else if (frame.ShaderPath.StartsWith("Assets/", StringComparison.OrdinalIgnoreCase)
                || frame.ShaderPath.StartsWith("Assets\\", StringComparison.OrdinalIgnoreCase))
            {
                shaderPath = Path.Combine(projectPath, frame.ShaderPath);
            }
            else
                shaderPath = Path.Combine(projectPath, "Assets", "Shaders", frame.ShaderPath);
            shader = getFileStamp(shaderPath);
            if (!shader.Exists)
                shader = getFileStamp(shaderPath + "c");
        }
        ActorResourceStamp stamp = new(texture, shader);
        resourceStamps[frame] = new CachedResourceStamp(stamp, now.AddSeconds(1), accessOrder);
        return stamp;
    }

    private static FileStamp getFileStamp(string path)
    {
        FileInfo file = new(path);
        return file.Exists
            ? new FileStamp(true, file.Length, file.LastWriteTimeUtc.Ticks)
            : default;
    }

    private static Matrix createActorTransform(
        Point origin,
        double cellSize,
        int gridX,
        int gridY,
        ActorVisualDescriptor descriptor)
    {
        double displayScale = cellSize / SourceCellSize;
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
        resolvedActors.Clear();
        resourceStamps.Clear();
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
            foreach (ActorVisualCacheKey key in visuals
                         .Where(entry => !pinnedVisuals.Contains(entry.Key))
                         .OrderBy(entry => entry.Value.LastUsed)
                         .Select(entry => entry.Key)
                         .Take(removeCount)
                         .ToArray())
            {
                disposeVisual(key);
            }
        }
        trimResourceStamps();
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

    private void disposeVisual(ActorVisualCacheKey key)
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

    private void trimResourceStamps()
    {
        if (resourceStamps.Count <= MaximumResourceStamps)
            return;
        foreach (ActorPreviewFrameKey key in resourceStamps
                     .OrderBy(entry => entry.Value.LastUsed)
                     .Select(entry => entry.Key)
                     .Take(resourceStamps.Count - MaximumResourceStamps)
                     .ToArray())
        {
            resourceStamps.Remove(key);
        }
    }

    private readonly record struct ActorSourceKey(
        string MapKey,
        string BlueprintReference,
        string Tag,
        int Index);

    private readonly record struct FileStamp(bool Exists, long Length, long ModifiedAt);
    private readonly record struct ActorResourceStamp(FileStamp Texture, FileStamp Shader);
    private readonly record struct ActorPreviewFrameKey(
        string TexturePath,
        PixelRect TextureRect,
        string ShaderPath,
        double Hue);
    private readonly record struct ActorVisualCacheKey(
        ActorPreviewFrameKey Frame,
        ActorResourceStamp Stamp);
    private sealed class CachedResourceStamp(
        ActorResourceStamp stamp,
        DateTime nextCheck,
        long lastUsed)
    {
        public ActorResourceStamp Stamp { get; } = stamp;
        public DateTime NextCheck { get; } = nextCheck;
        public long LastUsed { get; set; } = lastUsed;
    }

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
