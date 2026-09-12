using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace Ludork.Services;

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
        PublicationRevision++;
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
        PublicationRevision++;
        owner.release(this);
        ownedFrame?.Dispose();
        ownedFrame = null;
        frame = null;
    }

    internal long PublicationRevision { get; private set; }
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
        PublicationRevision++;
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

    internal WriteableBitmap PrepareFallback(byte[] pixels, int width, int height,
        ActorVisualDescriptor descriptor, CancellationToken cancellationToken)
    {
        if (presentationSize > 0)
            (pixels, width, height) = projectPresentation(pixels, width, height, descriptor, presentationSize, cancellationToken);
        cancellationToken.ThrowIfCancellationRequested();
        WriteableBitmap bitmap = new(new PixelSize(width, height), new Vector(96, 96),
            PixelFormat.Bgra8888, AlphaFormat.Unpremul);
        try
        {
            using ILockedFramebuffer locked = bitmap.Lock();
            if (locked.RowBytes == width * 4)
                Marshal.Copy(pixels, 0, locked.Address, pixels.Length);
            else
            {
                for (int y = 0; y < height; y++)
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    Marshal.Copy(pixels, y * width * 4, IntPtr.Add(locked.Address, y * locked.RowBytes), width * 4);
                }
            }
            cancellationToken.ThrowIfCancellationRequested();
            return bitmap;
        }
        catch
        {
            bitmap.Dispose();
            throw;
        }
    }

    internal void PublishFallback(WriteableBitmap bitmap, PixelRect textureRect, string? shaderError)
    {
        PublicationRevision++;
        WriteableBitmap? previous = ownedFrame;
        ownedFrame = bitmap;
        frame = bitmap;
        frameAlphaFormat = AlphaFormat.Unpremul;
        SourceRect = new PixelRect(0, 0, bitmap.PixelSize.Width, bitmap.PixelSize.Height);
        renderedTextureRect = textureRect;
        nativeRenderDirty = false;
        ShaderError = shaderError;
        FrameChanged?.Invoke(this, EventArgs.Empty);
        previous?.Dispose();
    }

    internal void publishAtlas(
        Bitmap atlas,
        PixelRect atlasRect,
        PixelRect textureRect,
        string? shaderError)
    {
        if (disposed || presentationSize != 0)
            return;
        PublicationRevision++;
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
        PublicationRevision++;
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
        PublicationRevision++;
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
        int size,
        CancellationToken cancellationToken = default)
    {
        double scaleX = descriptor.Scale.X;
        double scaleY = descriptor.Scale.Y;
        if (Math.Abs(scaleX) < 0.000001 || Math.Abs(scaleY) < 0.000001)
            return (new byte[4], 1, 1);
        double absoluteScaleX = Math.Abs(scaleX);
        double absoluteScaleY = Math.Abs(scaleY);
        int scaledWidth = Math.Max(1, (int)(sourceWidth * absoluteScaleX));
        int scaledHeight = Math.Max(1, (int)(sourceHeight * absoluteScaleY));
        int maxDimension = Math.Max(scaledWidth, scaledHeight);
        double fit = maxDimension > size ? size / (double)maxDimension : 1;
        int width = Math.Max(1, (int)Math.Round(scaledWidth * fit));
        int height = Math.Max(1, (int)Math.Round(scaledHeight * fit));
        byte[] pixels = new byte[width * height * 4];
        for (int y = 0; y < height; y++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            int scaledY = Math.Clamp((int)Math.Floor(y / fit), 0, scaledHeight - 1);
            int sourceY = (int)Math.Floor(scaledY / absoluteScaleY + descriptor.Origin.Y);
            if (scaleY < 0)
                sourceY = sourceHeight - 1 - sourceY;
            if (sourceY < 0 || sourceY >= sourceHeight)
                continue;
            for (int x = 0; x < width; x++)
            {
                int scaledX = Math.Clamp((int)Math.Floor(x / fit), 0, scaledWidth - 1);
                int sourceX = (int)Math.Floor(scaledX / absoluteScaleX + descriptor.Origin.X);
                if (scaleX < 0)
                    sourceX = sourceWidth - 1 - sourceX;
                if (sourceX < 0 || sourceX >= sourceWidth)
                    continue;
                Buffer.BlockCopy(source, (sourceY * sourceWidth + sourceX) * 4, pixels, (y * width + x) * 4, 4);
            }
        }
        return (pixels, width, height);
    }
}
