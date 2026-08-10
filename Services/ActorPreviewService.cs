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
    private PixelRect renderedTextureRect;
    private readonly int presentationSize;

    internal ActorPreviewLease(
        ActorPreviewService owner,
        ActorVisualDescriptor descriptor,
        int presentationSize,
        bool active)
    {
        this.owner = owner;
        Descriptor = descriptor;
        this.presentationSize = Math.Max(0, presentationSize);
        isActive = active;
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
    internal PixelRect RenderedTextureRect => renderedTextureRect;

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
        ShaderError = shaderError;
        FrameChanged?.Invoke(this, EventArgs.Empty);
    }

    internal bool UsesAtlas => presentationSize == 0;

    internal void clearFrame(PixelRect textureRect)
    {
        if (disposed || presentationSize != 0)
            return;
        bool changed = frame is not null;
        ownedFrame?.Dispose();
        ownedFrame = null;
        frameAlphaFormat = null;
        frame = null;
        SourceRect = new PixelRect(0, 0, textureRect.Width, textureRect.Height);
        renderedTextureRect = textureRect;
        ShaderError = null;
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

public sealed class ActorPreviewService : IDisposable
{
    private readonly PreviewHostConnection connection;
    private readonly DispatcherTimer timer;
    private readonly Stopwatch clock = Stopwatch.StartNew();
    private readonly CancellationTokenSource lifetime = new();
    private readonly HashSet<ActorPreviewLease> leases = [];
    private readonly Dictionary<string, SourceTexture> sourceTextures = new(StringComparer.OrdinalIgnoreCase);
    private readonly HashSet<WriteableBitmap> retiredAtlasPages = [];
    private List<WriteableBitmap> atlasPages = [];
    private long generation;
    private bool inFlight;
    private bool refreshPending;
    private bool disposed;
    private Task? renderTask;
    private DateTime nextConnectionAttempt = DateTime.MinValue;

    public ActorPreviewService(string projectPath)
    {
        connection = new PreviewHostConnection(projectPath);
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
        ObjectDisposedException.ThrowIf(disposed, this);
        ActorPreviewLease lease = new(this, descriptor, presentationSize, active);
        leases.Add(lease);
        ensureTimerState();
        if (!lease.UsesAtlas)
            updateFallback(lease, descriptor.GetTextureRect(clock.Elapsed), true);
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
        foreach (SourceTexture source in sourceTextures.Values)
            source.Dispose();
        sourceTextures.Clear();
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
            lease.clearFrame(lease.Descriptor.GetTextureRect(clock.Elapsed));
        ensureTimerState();
        Refresh();
    }

    internal void onLeaseDescriptorChanged(ActorPreviewLease lease)
    {
        PixelRect textureRect = lease.Descriptor.GetTextureRect(clock.Elapsed);
        if (lease.UsesAtlas)
            lease.clearFrame(textureRect);
        else
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
        List<ActorPreviewLease> native = [];
        CancellationToken cancellationToken = lifetime.Token;
        try
        {
            TimeSpan elapsed = clock.Elapsed;
            foreach (ActorPreviewLease lease in active)
            {
                PixelRect textureRect = lease.Descriptor.GetTextureRect(elapsed);
                if (lease.Descriptor.RequiresNativePreview)
                    native.Add(lease);
                else if (lease.RenderedTextureRect != textureRect)
                    updateFallback(lease, textureRect, true);
            }
            if (native.Count == 0)
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
                foreach (ActorPreviewLease lease in native)
                {
                    PixelRect textureRect = lease.Descriptor.GetTextureRect(elapsed);
                    if (lease.RenderedTextureRect != textureRect)
                        updateFallback(lease, textureRect, true);
                }
                return;
            }
            ActorPreviewBatchFrame? frame = await requestFrameAsync(native, elapsed, cancellationToken);
            if (!disposed && !cancellationToken.IsCancellationRequested && frame is not null)
                publishFrame(frame, native, elapsed);
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
        }
        catch (Exception exception) when (PreviewHostConnection.IsProtocolException(exception))
        {
            if (disposed)
                return;
            TimeSpan elapsed = clock.Elapsed;
            foreach (ActorPreviewLease lease in native)
                updateFallback(lease, lease.Descriptor.GetTextureRect(elapsed), true);
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

    private async Task<ActorPreviewBatchFrame?> requestFrameAsync(
        IReadOnlyList<ActorPreviewLease> active,
        TimeSpan elapsed,
        CancellationToken cancellationToken)
    {
        long requestGeneration = Interlocked.Increment(ref generation);
        JsonArray items = [];
        Dictionary<string, ActorVisualDescriptor> descriptors = new(StringComparer.Ordinal);
        foreach (ActorPreviewLease lease in active)
        {
            ActorVisualDescriptor descriptor = lease.Descriptor;
            PixelRect textureRect = descriptor.GetTextureRect(elapsed);
            string id = createRequestId(descriptor, textureRect);
            if (!descriptors.TryAdd(id, descriptor))
                continue;
            items.Add(new JsonObject
            {
                ["id"] = id,
                ["texturePath"] = descriptor.TexturePath,
                ["textureRect"] = new JsonObject
                {
                    ["x"] = textureRect.X,
                    ["y"] = textureRect.Y,
                    ["width"] = textureRect.Width,
                    ["height"] = textureRect.Height,
                },
                ["shaderPath"] = descriptor.ShaderPath,
                ["hue"] = normalizeHue(descriptor.Hue),
            });
        }
        JsonObject request = new()
        {
            ["type"] = "renderActorBatch",
            ["generation"] = requestGeneration,
            ["time"] = elapsed.TotalSeconds,
            ["items"] = items,
        };
        JsonObject response = await connection.ExchangeAsync(request, cancellationToken);
        cancellationToken.ThrowIfCancellationRequested();
        if (!string.Equals(getString(response, "type"), "actorFrame", StringComparison.Ordinal))
            throw new InvalidDataException(getString(response, "message", "UiPreviewHost returned an invalid actor frame."));
        long responseGeneration = response["generation"]?.GetValue<long>() ?? 0;
        if (responseGeneration != requestGeneration)
            return null;
        if (!string.Equals(getString(response, "pixelFormat"), "Bgra8888Premultiplied", StringComparison.Ordinal))
            throw new InvalidDataException("UiPreviewHost returned an unsupported actor pixel format.");
        List<ActorPreviewAtlasPage> pages = [];
        if (response["pages"] is JsonArray pageData)
        {
            foreach (JsonObject page in pageData.OfType<JsonObject>())
            {
                cancellationToken.ThrowIfCancellationRequested();
                int width = page["width"]?.GetValue<int>() ?? 0;
                int height = page["height"]?.GetValue<int>() ?? 0;
                int stride = page["stride"]?.GetValue<int>() ?? width * 4;
                if (width <= 0 || height <= 0 || stride < width * 4)
                    throw new InvalidDataException("UiPreviewHost returned invalid actor atlas dimensions.");
                pages.Add(new ActorPreviewAtlasPage(
                    width,
                    height,
                    stride,
                    connection.ReadPixels(page, checked(stride * height))));
            }
        }
        List<ActorPreviewAtlasItem> resultItems = [];
        if (response["items"] is JsonArray itemData)
        {
            foreach (JsonObject item in itemData.OfType<JsonObject>())
            {
                resultItems.Add(new ActorPreviewAtlasItem(
                    getString(item, "id"),
                    item["page"]?.GetValue<int>() ?? -1,
                    new PixelRect(
                        item["x"]?.GetValue<int>() ?? 0,
                        item["y"]?.GetValue<int>() ?? 0,
                        item["width"]?.GetValue<int>() ?? 0,
                        item["height"]?.GetValue<int>() ?? 0),
                    item["shaderError"]?.GetValue<bool>() == true,
                    getString(item, "error")));
            }
        }
        return new ActorPreviewBatchFrame(responseGeneration, pages, resultItems);
    }

    private void publishFrame(
        ActorPreviewBatchFrame frame,
        IReadOnlyList<ActorPreviewLease> active,
        TimeSpan elapsed)
    {
        if (disposed || frame.Generation != Interlocked.Read(ref generation))
            return;
        Dictionary<string, ActorPreviewAtlasItem> items = frame.Items.ToDictionary(item => item.Id, StringComparer.Ordinal);
        List<(ActorPreviewLease Lease, ActorPreviewAtlasItem Item, PixelRect TextureRect, ActorPreviewAtlasPage Page)>
            publications = [];
        foreach (ActorPreviewLease lease in active)
        {
            if (lease.IsDisposed || !lease.IsActive)
                continue;
            ActorVisualDescriptor descriptor = lease.Descriptor;
            PixelRect textureRect = descriptor.GetTextureRect(elapsed);
            string id = createRequestId(descriptor, textureRect);
            if (!items.TryGetValue(id, out ActorPreviewAtlasItem? item)
                || item.Page < 0 || item.Page >= frame.Pages.Count
                || item.Rect.Width <= 0 || item.Rect.Height <= 0)
            {
                updateFallback(lease, textureRect, true);
                continue;
            }
            ActorPreviewAtlasPage page = frame.Pages[item.Page];
            if (item.Rect.X < 0 || item.Rect.Y < 0
                || item.Rect.Right > page.Width || item.Rect.Bottom > page.Height)
                throw new InvalidDataException("UiPreviewHost returned an actor atlas item outside its page.");
            publications.Add((lease, item, textureRect, page));
        }
        List<WriteableBitmap> nextAtlasPages = frame.Pages.Select(createAtlasBitmap).ToList();
        List<WriteableBitmap> previousAtlasPages = atlasPages;
        atlasPages = nextAtlasPages;
        foreach ((ActorPreviewLease lease, ActorPreviewAtlasItem item, PixelRect textureRect, ActorPreviewAtlasPage page)
            in publications)
        {
            string? shaderError = item.ShaderError ? item.Error : null;
            if (lease.UsesAtlas)
            {
                lease.publishAtlas(
                    nextAtlasPages[item.Page],
                    item.Rect,
                    textureRect,
                    shaderError);
            }
            else
            {
                lease.publish(
                    textureRect,
                    item.Rect.Width,
                    item.Rect.Height,
                    page.Stride,
                    page.Pixels,
                    item.Rect.X,
                    item.Rect.Y,
                    true,
                    shaderError);
            }
        }
        retireAtlasPages(previousAtlasPages);
    }

    private static WriteableBitmap createAtlasBitmap(ActorPreviewAtlasPage page)
    {
        WriteableBitmap bitmap = new(
            new PixelSize(page.Width, page.Height),
            new Vector(96, 96),
            PixelFormat.Bgra8888,
            AlphaFormat.Premul);
        using (ILockedFramebuffer frame = bitmap.Lock())
        {
            byte[] target = new byte[frame.RowBytes * page.Height];
            for (int y = 0; y < page.Height; y += 1)
            {
                Buffer.BlockCopy(
                    page.Pixels,
                    y * page.Stride,
                    target,
                    y * frame.RowBytes,
                    page.Width * 4);
            }
            Marshal.Copy(target, 0, frame.Address, target.Length);
        }
        return bitmap;
    }

    private void retireAtlasPages(IReadOnlyList<WriteableBitmap> pages)
    {
        if (pages.Count == 0)
            return;
        foreach (WriteableBitmap page in pages)
            retiredAtlasPages.Add(page);
        Dispatcher.UIThread.Post(
            () =>
            {
                foreach (WriteableBitmap page in pages)
                {
                    if (retiredAtlasPages.Remove(page))
                        page.Dispose();
                }
            },
            DispatcherPriority.Background);
    }

    private void updateFallback(ActorPreviewLease lease, PixelRect textureRect, bool applyHue)
    {
        if (disposed || lease.IsDisposed)
            return;
        if (lease.UsesAtlas)
        {
            lease.clearFrame(textureRect);
            return;
        }
        SourceTexture? source = getSourceTexture(lease.Descriptor.TexturePath);
        if (source is null
            || textureRect.X < 0 || textureRect.Y < 0
            || textureRect.Right > source.Width || textureRect.Bottom > source.Height)
        {
            return;
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
            null);
    }

    private SourceTexture? getSourceTexture(string path)
    {
        if (disposed || !File.Exists(path))
            return null;
        FileInfo info = new(path);
        if (sourceTextures.TryGetValue(path, out SourceTexture? source)
            && source.LastWriteTimeUtc == info.LastWriteTimeUtc
            && source.Length == info.Length)
        {
            return source;
        }
        source?.Dispose();
        SourceTexture loaded = SourceTexture.Load(path, info.LastWriteTimeUtc, info.Length);
        sourceTextures[path] = loaded;
        return loaded;
    }

    private void ensureTimerState()
    {
        bool realtime = !disposed && leases.Any(lease => lease.IsActive && !lease.IsDisposed
            && (lease.Descriptor.Animated
                || IsAvailable && lease.Descriptor.ShaderPath.Length != 0));
        bool retryConnection = !disposed && !IsAvailable
            && leases.Any(lease => lease.IsActive && !lease.IsDisposed
                && lease.Descriptor.RequiresNativePreview);
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
                updateFallback(lease, lease.Descriptor.GetTextureRect(elapsed), true);
            }
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

    private sealed class SourceTexture : IDisposable
    {
        private readonly byte[] pixels;

        private SourceTexture(
            int width,
            int height,
            int stride,
            byte[] pixels,
            DateTime lastWriteTimeUtc,
            long length)
        {
            Width = width;
            Height = height;
            Stride = stride;
            this.pixels = pixels;
            LastWriteTimeUtc = lastWriteTimeUtc;
            Length = length;
        }

        public int Width { get; }
        public int Height { get; }
        public int Stride { get; }
        public DateTime LastWriteTimeUtc { get; }
        public long Length { get; }

        public static SourceTexture Load(string path, DateTime lastWriteTimeUtc, long length)
        {
            using Bitmap bitmap = new(path);
            using WriteableBitmap buffer = new(
                bitmap.PixelSize,
                bitmap.Dpi,
                PixelFormat.Bgra8888,
                AlphaFormat.Unpremul);
            using ILockedFramebuffer frame = buffer.Lock();
            bitmap.CopyPixels(frame);
            byte[] pixels = new byte[frame.RowBytes * frame.Size.Height];
            Marshal.Copy(frame.Address, pixels, 0, pixels.Length);
            int width = frame.Size.Width;
            int height = frame.Size.Height;
            int stride = frame.RowBytes;
            return new SourceTexture(width, height, stride, pixels, lastWriteTimeUtc, length);
        }

        public byte[] Copy(PixelRect rect, double hue)
        {
            byte[] result = new byte[rect.Width * rect.Height * 4];
            for (int y = 0; y < rect.Height; y += 1)
            {
                int sourceOffset = (rect.Y + y) * Stride + rect.X * 4;
                int targetOffset = y * rect.Width * 4;
                Buffer.BlockCopy(pixels, sourceOffset, result, targetOffset, rect.Width * 4);
            }
            if (hue > 0.001)
                applyHue(result, hue);
            return result;
        }

        public void Dispose()
        {
        }

        private static void applyHue(byte[] data, double hue)
        {
            float offset = (float)(hue / 360.0);
            for (int index = 0; index < data.Length; index += 4)
            {
                if (data[index + 3] == 0)
                    continue;
                rgbToHsv(data[index + 2], data[index + 1], data[index], out float h, out float s, out float v);
                hsvToRgb((h + offset) % 1f, s, v, out byte r, out byte g, out byte b);
                data[index + 2] = r;
                data[index + 1] = g;
                data[index] = b;
            }
        }

        private static void rgbToHsv(byte r, byte g, byte b, out float h, out float s, out float v)
        {
            float rf = r / 255f;
            float gf = g / 255f;
            float bf = b / 255f;
            float max = Math.Max(rf, Math.Max(gf, bf));
            float min = Math.Min(rf, Math.Min(gf, bf));
            float delta = max - min;
            v = max;
            if (delta <= 0.00001f)
            {
                h = 0;
                s = 0;
                return;
            }
            s = delta / max;
            if (rf >= max)
                h = (gf - bf) / delta % 6f;
            else if (gf >= max)
                h = (bf - rf) / delta + 2f;
            else
                h = (rf - gf) / delta + 4f;
            h /= 6f;
            if (h < 0)
                h += 1f;
        }

        private static void hsvToRgb(float h, float s, float v, out byte r, out byte g, out byte b)
        {
            float c = v * s;
            float x = c * (1 - Math.Abs(h * 6f % 2f - 1));
            float m = v - c;
            float rf;
            float gf;
            float bf;
            int sector = (int)(h * 6f) % 6;
            switch (sector)
            {
                case 0: rf = c; gf = x; bf = 0; break;
                case 1: rf = x; gf = c; bf = 0; break;
                case 2: rf = 0; gf = c; bf = x; break;
                case 3: rf = 0; gf = x; bf = c; break;
                case 4: rf = x; gf = 0; bf = c; break;
                default: rf = c; gf = 0; bf = x; break;
            }
            r = (byte)Math.Clamp((rf + m) * 255f, 0, 255);
            g = (byte)Math.Clamp((gf + m) * 255f, 0, 255);
            b = (byte)Math.Clamp((bf + m) * 255f, 0, 255);
        }
    }
}
