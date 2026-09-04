using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Threading;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed record ActorVisualDescriptor(
    string BlueprintReference,
    string TexturePath,
    PixelSize TextureSize,
    PixelRect BaseTextureRect,
    string ShaderPath,
    double Hue,
    Vector Translation,
    Vector Scale,
    Vector Origin,
    double Rotation,
    bool IsCharacter,
    bool Animated,
    double SwitchInterval,
    int FrameCount)
{
    public bool RequiresRealtimePreview => Animated || ShaderPath.Length != 0;
    public bool RequiresNativePreview => ShaderPath.Length != 0 || Math.Abs(Hue % 360) > 0.001;
    public bool RequiresPreviewService => Animated || RequiresNativePreview;

    public PixelRect GetTextureRect(TimeSpan elapsed)
    {
        if (!Animated || FrameCount <= 1)
            return BaseTextureRect;
        int frame = (int)(elapsed.TotalSeconds / Math.Max(0.001, SwitchInterval)) % FrameCount;
        int x = (BaseTextureRect.X + frame * BaseTextureRect.Width) % TextureSize.Width;
        return new PixelRect(x, BaseTextureRect.Y, BaseTextureRect.Width, BaseTextureRect.Height);
    }
}

public sealed record ActorPreviewAtlasPage(int Width, int Height, int Stride, byte[] Pixels);

public sealed record ActorPreviewAtlasItem(
    string Id,
    int Page,
    PixelRect Rect,
    bool ShaderError,
    string Error);

public sealed record ActorPreviewBatchFrame(
    long Generation,
    IReadOnlyList<ActorPreviewAtlasPage> Pages,
    IReadOnlyList<ActorPreviewAtlasItem> Items);

public sealed class ActorPreviewLease : IDisposable
{
    private readonly ActorPreviewService owner;
    private Bitmap? frame;
    private WriteableBitmap? ownedFrame;
    private AlphaFormat? frameAlphaFormat;
    private bool isActive = true;
    private bool disposed;
    private bool nativeRenderDirty;
    private PixelRect renderedTextureRect;
    private readonly int presentationSize;
    private readonly bool staticFrame;

    internal ActorPreviewLease(
        ActorPreviewService owner,
        ActorVisualDescriptor descriptor,
        int presentationSize,
        bool active,
        bool staticFrame)
    {
        this.owner = owner;
        Descriptor = descriptor;
        this.presentationSize = Math.Max(0, presentationSize);
        this.staticFrame = staticFrame;
        isActive = active;
        nativeRenderDirty = descriptor.RequiresNativePreview;
        SourceRect = new PixelRect(0, 0, descriptor.BaseTextureRect.Width, descriptor.BaseTextureRect.Height);
    }

    public event EventHandler? FrameChanged;

    public ActorVisualDescriptor Descriptor { get; private set; }
    public Bitmap? Frame => frame;
    public PixelRect SourceRect { get; private set; }
    public string? ShaderError { get; private set; }
    public bool IsActive
    {
        get => isActive;
        set
        {
            if (disposed || isActive == value)
                return;
            isActive = value;
            owner.onLeaseActivityChanged(this);
        }
    }

    public void UpdateDescriptor(ActorVisualDescriptor descriptor)
    {
        if (disposed || Descriptor == descriptor)
            return;
        Descriptor = descriptor;
        renderedTextureRect = default;
        nativeRenderDirty = descriptor.RequiresNativePreview;
        ShaderError = null;
        owner.onLeaseDescriptorChanged(this);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        owner.release(this);
        ownedFrame?.Dispose();
        ownedFrame = null;
        frame = null;
    }

    internal bool IsDisposed => disposed;
    internal bool IsStatic => staticFrame;
    internal bool NeedsNativeRender => nativeRenderDirty;
    internal PixelRect RenderedTextureRect => renderedTextureRect;

    internal PixelRect getTextureRect(TimeSpan elapsed)
    {
        return Descriptor.GetTextureRect(staticFrame ? TimeSpan.Zero : elapsed);
    }

    internal void markNativeRenderDirty()
    {
        if (!disposed && staticFrame && Descriptor.RequiresNativePreview)
            nativeRenderDirty = true;
    }

    internal void publish(
        PixelRect textureRect,
        int width,
        int height,
        int sourceStride,
        byte[] sourcePixels,
        int sourceX,
        int sourceY,
        bool premultiplied,
        string? shaderError)
    {
        if (disposed || width <= 0 || height <= 0)
            return;
        byte[] publishedPixels = copyPixels(
            width,
            height,
            sourceStride,
            sourcePixels,
            sourceX,
            sourceY);
        sourceStride = width * 4;
        sourceX = 0;
        sourceY = 0;
        if (presentationSize > 0)
        {
            (publishedPixels, width, height) = projectPresentation(
                publishedPixels,
                width,
                height,
                Descriptor,
                presentationSize);
            sourceStride = width * 4;
            sourceX = 0;
            sourceY = 0;
        }
        AlphaFormat alphaFormat = premultiplied ? AlphaFormat.Premul : AlphaFormat.Unpremul;
        if (ownedFrame is null || ownedFrame.PixelSize.Width != width || ownedFrame.PixelSize.Height != height
            || frameAlphaFormat != alphaFormat)
        {
            ownedFrame?.Dispose();
            ownedFrame = new WriteableBitmap(
                new PixelSize(width, height),
                new Vector(96, 96),
                PixelFormat.Bgra8888,
                alphaFormat);
            frameAlphaFormat = alphaFormat;
        }
        frame = ownedFrame;
        using (ILockedFramebuffer locked = ownedFrame.Lock())
        {
            byte[] target = new byte[locked.RowBytes * height];
            for (int y = 0; y < height; y += 1)
            {
                int sourceOffset = (sourceY + y) * sourceStride + sourceX * 4;
                int targetOffset = y * locked.RowBytes;
                Buffer.BlockCopy(publishedPixels, sourceOffset, target, targetOffset, width * 4);
            }
            Marshal.Copy(target, 0, locked.Address, target.Length);
        }
        SourceRect = new PixelRect(0, 0, width, height);
        renderedTextureRect = textureRect;
        nativeRenderDirty = false;
        ShaderError = shaderError;
        FrameChanged?.Invoke(this, EventArgs.Empty);
    }

    internal void publishAtlas(
        Bitmap atlas,
        PixelRect atlasRect,
        PixelRect textureRect,
        string? shaderError)
    {
        if (disposed || presentationSize != 0)
            return;
        ownedFrame?.Dispose();
        ownedFrame = null;
        frameAlphaFormat = null;
        frame = atlas;
        SourceRect = atlasRect;
        renderedTextureRect = textureRect;
        nativeRenderDirty = false;
        ShaderError = shaderError;
        FrameChanged?.Invoke(this, EventArgs.Empty);
    }

    internal bool UsesAtlas => presentationSize == 0 && !staticFrame;

    internal void clearFrame(
        PixelRect textureRect,
        string? shaderError = null,
        bool nativeRenderPending = false)
    {
        if (disposed)
            return;
        bool changed = frame is not null || !string.Equals(ShaderError, shaderError, StringComparison.Ordinal);
        ownedFrame?.Dispose();
        ownedFrame = null;
        frameAlphaFormat = null;
        frame = null;
        SourceRect = new PixelRect(0, 0, textureRect.Width, textureRect.Height);
        renderedTextureRect = textureRect;
        nativeRenderDirty = nativeRenderPending;
        ShaderError = shaderError;
        if (changed)
            FrameChanged?.Invoke(this, EventArgs.Empty);
    }

    internal void publishRenderError(PixelRect textureRect, string shaderError)
    {
        if (disposed)
            return;
        bool changed = !string.Equals(ShaderError, shaderError, StringComparison.Ordinal);
        renderedTextureRect = textureRect;
        nativeRenderDirty = false;
        ShaderError = shaderError;
        if (changed)
            FrameChanged?.Invoke(this, EventArgs.Empty);
    }

    private static byte[] copyPixels(
        int width,
        int height,
        int sourceStride,
        byte[] source,
        int sourceX,
        int sourceY)
    {
        byte[] result = new byte[width * height * 4];
        for (int y = 0; y < height; y += 1)
        {
            int sourceOffset = (sourceY + y) * sourceStride + sourceX * 4;
            Buffer.BlockCopy(source, sourceOffset, result, y * width * 4, width * 4);
        }
        return result;
    }

    private static (byte[] pixels, int width, int height) projectPresentation(
        byte[] source,
        int sourceWidth,
        int sourceHeight,
        ActorVisualDescriptor descriptor,
        int size)
    {
        double scaleX = descriptor.Scale.X;
        double scaleY = descriptor.Scale.Y;
        if (Math.Abs(scaleX) < 0.000001 || Math.Abs(scaleY) < 0.000001)
            return (new byte[4], 1, 1);
        double absoluteScaleX = Math.Abs(scaleX);
        double absoluteScaleY = Math.Abs(scaleY);
        int width = Math.Max(1, (int)(sourceWidth * absoluteScaleX));
        int height = Math.Max(1, (int)(sourceHeight * absoluteScaleY));
        byte[] scaled = new byte[width * height * 4];
        for (int y = 0; y < height; y += 1)
        {
            for (int x = 0; x < width; x += 1)
            {
                int sourceX = (int)Math.Floor(x / absoluteScaleX + descriptor.Origin.X);
                int sourceY = (int)Math.Floor(y / absoluteScaleY + descriptor.Origin.Y);
                if (scaleX < 0)
                    sourceX = sourceWidth - 1 - sourceX;
                if (scaleY < 0)
                    sourceY = sourceHeight - 1 - sourceY;
                if (sourceX < 0 || sourceY < 0 || sourceX >= sourceWidth || sourceY >= sourceHeight)
                    continue;
                Buffer.BlockCopy(
                    source,
                    (sourceY * sourceWidth + sourceX) * 4,
                    scaled,
                    (y * width + x) * 4,
                    4);
            }
        }
        int maxDimension = Math.Max(width, height);
        if (maxDimension <= size)
            return (scaled, width, height);
        double fit = size / (double)maxDimension;
        int fittedWidth = Math.Max(1, (int)Math.Round(width * fit));
        int fittedHeight = Math.Max(1, (int)Math.Round(height * fit));
        byte[] fitted = new byte[fittedWidth * fittedHeight * 4];
        for (int y = 0; y < fittedHeight; y += 1)
        {
            for (int x = 0; x < fittedWidth; x += 1)
            {
                int sourceX = Math.Clamp((int)Math.Floor(x / fit), 0, width - 1);
                int sourceY = Math.Clamp((int)Math.Floor(y / fit), 0, height - 1);
                Buffer.BlockCopy(
                    scaled,
                    (sourceY * width + sourceX) * 4,
                    fitted,
                    (y * fittedWidth + x) * 4,
                    4);
            }
        }
        return (fitted, fittedWidth, fittedHeight);
    }
}

public sealed partial class ActorPreviewService : IDisposable
{
    private const int MaximumSourceTextures = 128;
    private const long MaximumSourceTextureBytes = 128L * 1024L * 1024L;
    private readonly PreviewHostConnection connection;
    private readonly string projectPath;
    private readonly DispatcherTimer timer;
    private readonly Stopwatch clock = Stopwatch.StartNew();
    private readonly CancellationTokenSource lifetime = new();
    private readonly HashSet<ActorPreviewLease> leases = [];
    private readonly Dictionary<string, CachedSourceTexture> sourceTextures = new(StringComparer.Ordinal);
    private readonly HashSet<WriteableBitmap> retiredAtlasPages = [];
    private List<WriteableBitmap> atlasPages = [];
    private long generation;
    private long sourceTextureAccessOrder;
    private long sourceTextureBytes;
    private bool inFlight;
    private bool refreshPending;
    private bool disposed;
    private Task? renderTask;
    private DateTime nextConnectionAttempt = DateTime.MinValue;

    public ActorPreviewService(string projectPath)
    {
        this.projectPath = Path.GetFullPath(projectPath);
        connection = new PreviewHostConnection(this.projectPath);
        connection.StateChanged += onConnectionStateChanged;
        timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(1000.0 / 30.0) };
        timer.Tick += onTimerTick;
    }

    public event EventHandler? StatusChanged;

    public TimeSpan Elapsed => clock.Elapsed;
    public bool IsAvailable => connection.IsReady && connection.Capabilities.Contains("actor");
    public string StatusMessage => connection.IsReady && !connection.Capabilities.Contains("actor")
        ? "UiPreviewHost does not support actor preview."
        : connection.StatusMessage;

    public ActorPreviewLease Acquire(ActorVisualDescriptor descriptor)
    {
        return Acquire(descriptor, 0, true);
    }

    public ActorPreviewLease Acquire(ActorVisualDescriptor descriptor, int presentationSize)
    {
        return Acquire(descriptor, presentationSize, true);
    }

    public ActorPreviewLease Acquire(
        ActorVisualDescriptor descriptor,
        int presentationSize,
        bool active)
    {
        return acquire(descriptor, presentationSize, active, false);
    }

    internal ActorPreviewLease AcquireStatic(ActorVisualDescriptor descriptor)
    {
        return acquire(descriptor, 0, true, true);
    }

    private ActorPreviewLease acquire(
        ActorVisualDescriptor descriptor,
        int presentationSize,
        bool active,
        bool staticFrame)
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        ActorPreviewLease lease = new(this, descriptor, presentationSize, active, staticFrame);
        leases.Add(lease);
        if (!lease.UsesAtlas)
        {
            PixelRect textureRect = lease.getTextureRect(clock.Elapsed);
            if (!staticFrame || descriptor.ShaderPath.Length == 0)
                updateFallback(lease, textureRect, true);
        }
        ensureTimerState();
        if (active)
            Refresh();
        return lease;
    }

    public void Refresh()
    {
        if (disposed)
            return;
        ensureTimerState();
        if (inFlight)
        {
            refreshPending = true;
            return;
        }
        queueRender();
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        lifetime.Cancel();
        Task? pendingRender = renderTask;
        timer.Stop();
        timer.Tick -= onTimerTick;
        connection.StateChanged -= onConnectionStateChanged;
        foreach (ActorPreviewLease lease in leases.ToArray())
            lease.Dispose();
        leases.Clear();
        foreach (CachedSourceTexture source in sourceTextures.Values)
            source.Texture.Dispose();
        sourceTextures.Clear();
        sourceTextureBytes = 0;
        foreach (WriteableBitmap page in atlasPages)
            page.Dispose();
        atlasPages.Clear();
        foreach (WriteableBitmap page in retiredAtlasPages)
            page.Dispose();
        retiredAtlasPages.Clear();
        connection.Dispose();
        if (pendingRender is null || pendingRender.IsCompleted)
        {
            lifetime.Dispose();
        }
        else
        {
            _ = pendingRender.ContinueWith(
                _ => lifetime.Dispose(),
                CancellationToken.None,
                TaskContinuationOptions.ExecuteSynchronously,
                TaskScheduler.Default);
        }
    }

    internal void release(ActorPreviewLease lease)
    {
        leases.Remove(lease);
        ensureTimerState();
    }

    internal void onLeaseActivityChanged(ActorPreviewLease lease)
    {
        if (lease.UsesAtlas)
            lease.clearFrame(lease.getTextureRect(clock.Elapsed));
        ensureTimerState();
        Refresh();
    }

    internal void onLeaseDescriptorChanged(ActorPreviewLease lease)
    {
        PixelRect textureRect = lease.getTextureRect(clock.Elapsed);
        if (lease.UsesAtlas)
            lease.clearFrame(textureRect);
        else if (!lease.IsStatic || lease.Descriptor.ShaderPath.Length == 0)
            updateFallback(lease, textureRect, true);
        ensureTimerState();
        Refresh();
    }

    private void onTimerTick(object? sender, EventArgs args)
    {
        Refresh();
    }

    private void queueRender()
    {
        if (!disposed)
            renderTask = renderNextFrameAsync();
    }

    private async Task renderNextFrameAsync()
    {
        if (disposed || inFlight)
            return;
        List<ActorPreviewLease> active = leases.Where(lease => lease.IsActive && !lease.IsDisposed).ToList();
        if (active.Count == 0)
            return;
        inFlight = true;
        List<ActorPreviewLease> realtimeNative = [];
        List<ActorPreviewLease> staticNative = [];
        CancellationToken cancellationToken = lifetime.Token;
        try
        {
            TimeSpan elapsed = clock.Elapsed;
            foreach (ActorPreviewLease lease in active)
            {
                PixelRect textureRect = lease.getTextureRect(elapsed);
                if (lease.Descriptor.RequiresNativePreview)
                {
                    if (!lease.IsStatic)
                        realtimeNative.Add(lease);
                    else if (lease.NeedsNativeRender)
                        staticNative.Add(lease);
                }
                else if (lease.RenderedTextureRect != textureRect)
                    updateFallback(lease, textureRect, true);
            }
            if (realtimeNative.Count == 0 && staticNative.Count == 0)
                return;
            if (!IsAvailable && DateTime.UtcNow >= nextConnectionAttempt)
            {
                nextConnectionAttempt = DateTime.UtcNow.AddSeconds(1);
                await connection.StartAsync(cancellationToken);
            }
            if (disposed || cancellationToken.IsCancellationRequested)
                return;
            if (!IsAvailable)
            {
                foreach (ActorPreviewLease lease in realtimeNative.Concat(staticNative))
                    publishUnavailableFallback(lease, elapsed);
                return;
            }
            if (staticNative.Count != 0)
            {
                ActorPreviewBatchFrame? staticFrame = await requestFrameAsync(
                    staticNative,
                    TimeSpan.Zero,
                    cancellationToken);
                if (!disposed && !cancellationToken.IsCancellationRequested && staticFrame is not null)
                    publishFrame(staticFrame, staticNative, TimeSpan.Zero);
            }
            if (realtimeNative.Count != 0)
            {
                ActorPreviewBatchFrame? realtimeFrame = await requestFrameAsync(
                    realtimeNative,
                    elapsed,
                    cancellationToken);
                if (!disposed && !cancellationToken.IsCancellationRequested && realtimeFrame is not null)
                    publishFrame(realtimeFrame, realtimeNative, elapsed);
            }
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
        }
        catch (Exception exception) when (PreviewHostConnection.IsProtocolException(exception))
        {
            if (disposed)
                return;
            TimeSpan elapsed = clock.Elapsed;
            foreach (ActorPreviewLease lease in realtimeNative.Concat(staticNative))
                publishUnavailableFallback(lease, elapsed);
            StatusChanged?.Invoke(this, EventArgs.Empty);
        }
        finally
        {
            inFlight = false;
            if (!disposed && refreshPending)
            {
                refreshPending = false;
                queueRender();
            }
        }
    }

    private bool updateFallback(
        ActorPreviewLease lease,
        PixelRect textureRect,
        bool applyHue,
        string? shaderError = null,
        bool nativeRenderPending = false)
    {
        if (disposed || lease.IsDisposed)
            return false;
        if (lease.UsesAtlas)
        {
            lease.clearFrame(textureRect);
            return false;
        }
        SourceTexture? source = getSourceTexture(lease.Descriptor.TexturePath);
        if (source is null
            || textureRect.X < 0 || textureRect.Y < 0
            || textureRect.Right > source.Width || textureRect.Bottom > source.Height)
        {
            return false;
        }
        byte[] pixels = source.Copy(textureRect, applyHue ? normalizeHue(lease.Descriptor.Hue) : 0);
        lease.publish(
            textureRect,
            textureRect.Width,
            textureRect.Height,
            textureRect.Width * 4,
            pixels,
            0,
            0,
            false,
            shaderError);
        if (nativeRenderPending)
            lease.markNativeRenderDirty();
        return true;
    }

    private void publishUnavailableFallback(ActorPreviewLease lease, TimeSpan elapsed)
    {
        PixelRect textureRect = lease.getTextureRect(elapsed);
        if (lease.IsStatic && lease.Descriptor.ShaderPath.Length != 0)
        {
            if (lease.Frame is not null)
            {
                lease.markNativeRenderDirty();
                return;
            }
            string message = string.IsNullOrWhiteSpace(StatusMessage)
                ? lease.ShaderError ?? "Actor shader preview is unavailable."
                : StatusMessage;
            if (!updateFallback(lease, textureRect, true, message, true))
                lease.clearFrame(textureRect, message, true);
            return;
        }
        if (lease.RenderedTextureRect != textureRect)
            updateFallback(lease, textureRect, true);
    }

    private void ensureTimerState()
    {
        bool realtime = !disposed && leases.Any(lease => lease.IsActive && !lease.IsDisposed
            && !lease.IsStatic
            && (lease.Descriptor.Animated
                || IsAvailable && lease.Descriptor.ShaderPath.Length != 0));
        bool retryConnection = !disposed && !IsAvailable
            && leases.Any(lease => lease.IsActive && !lease.IsDisposed
                && lease.Descriptor.RequiresNativePreview
                && (!lease.IsStatic || lease.NeedsNativeRender));
        bool shouldRun = realtime || retryConnection;
        timer.Interval = realtime
            ? TimeSpan.FromMilliseconds(1000.0 / 30.0)
            : TimeSpan.FromSeconds(1);
        if (shouldRun && !timer.IsEnabled)
            timer.Start();
        else if (!shouldRun && timer.IsEnabled)
            timer.Stop();
    }

    private void onConnectionStateChanged(object? sender, EventArgs args)
    {
        if (disposed)
            return;
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => onConnectionStateChanged(sender, args));
            return;
        }
        if (!IsAvailable)
        {
            TimeSpan elapsed = clock.Elapsed;
            foreach (ActorPreviewLease lease in leases.Where(lease => lease.IsActive && !lease.IsDisposed
                && lease.Descriptor.RequiresNativePreview))
            {
                publishUnavailableFallback(lease, elapsed);
            }
        }
        else
        {
            bool refreshStatic = false;
            foreach (ActorPreviewLease lease in leases.Where(lease => lease.IsActive && !lease.IsDisposed
                && lease.IsStatic && lease.Descriptor.RequiresNativePreview))
            {
                lease.markNativeRenderDirty();
                refreshStatic = true;
            }
            if (refreshStatic)
                Refresh();
        }
        ensureTimerState();
        StatusChanged?.Invoke(this, EventArgs.Empty);
    }

    private static string createRequestId(ActorVisualDescriptor descriptor, PixelRect rect)
    {
        return string.Join("|",
            descriptor.TexturePath,
            rect.X.ToString(CultureInfo.InvariantCulture),
            rect.Y.ToString(CultureInfo.InvariantCulture),
            rect.Width.ToString(CultureInfo.InvariantCulture),
            rect.Height.ToString(CultureInfo.InvariantCulture),
            descriptor.ShaderPath,
            normalizeHue(descriptor.Hue).ToString("R", CultureInfo.InvariantCulture));
    }

    private static double normalizeHue(double hue)
    {
        double result = hue % 360;
        return result < 0 ? result + 360 : result;
    }

    private static string getString(JsonObject value, string propertyName, string fallback = "")
    {
        return value[propertyName]?.GetValue<string>() ?? fallback;
    }

}
