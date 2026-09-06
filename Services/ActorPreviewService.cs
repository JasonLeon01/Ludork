using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
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
