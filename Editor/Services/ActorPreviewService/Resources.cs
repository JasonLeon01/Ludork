using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Ludork.Plugin.Avalonia;
using System;
using System.IO;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using System.Linq;
using System.Runtime.InteropServices;

namespace Ludork.Services;

public sealed partial class ActorPreviewService
{
    private async Task<SourceTexture?> getSourceTextureAsync(string assetPath, bool forceValidation, CancellationToken cancellationToken)
    {
        if (disposed)
            return null;
        sourceTextureAccessOrder++;
        sourceTextures.TryGetValue(assetPath, out CachedSourceTexture? cached);
        if (cached is not null)
        {
            cached.LastUsed = sourceTextureAccessOrder;
            if (!forceValidation && clock.Elapsed < cached.ValidateAfter)
                return cached.Texture;
        }
        if (!forceValidation && sourceFailures.TryGetValue(assetPath, out TimeSpan retryAt) && clock.Elapsed < retryAt)
            return null;
        if (!sourceLoads.TryGetValue(assetPath, out SourceLoad? load))
        {
            load = new SourceLoad(CancellationTokenSource.CreateLinkedTokenSource(lifetime.Token));
            sourceLoads.Add(assetPath, load);
            load.Task = loadSourceAsync(assetPath, cached, load);
        }
        load.Users++;
        try
        {
            return await load.Task.WaitAsync(cancellationToken);
        }
        finally
        {
            load.Users--;
            if (load.Users == 0 && !load.Task.IsCompleted)
            {
                if (sourceLoads.TryGetValue(assetPath, out SourceLoad? current) && ReferenceEquals(current, load))
                    sourceLoads.Remove(assetPath);
                load.Cancellation.Cancel();
            }
        }
    }

    private async Task<SourceTexture?> loadSourceAsync(string assetPath, CachedSourceTexture? cached, SourceLoad load)
    {
        CancellationToken token = load.Cancellation.Token;
        bool succeeded = false;
        try
        {
            SourceFile? file = await Task.Run(() => readSourceFile(assetPath), token);
            if (file is null)
                return null;
            if (cached is not null && cached.Texture.LastWriteTimeUtc == file.ModifiedAt && cached.Texture.Length == file.Length)
            {
                cached.ValidateAfter = clock.Elapsed + TimeSpan.FromSeconds(1);
                succeeded = true;
                return cached.Texture;
            }
            using EditorThumbnailLease? lease = await thumbnails.AcquireAsync(file.Path, 0, token);
            if (lease is null)
                return null;
            SourceTexture loaded = await Task.Run(() => SourceTexture.Load(lease.Bitmap, file.ModifiedAt, file.Length, token), token);
            token.ThrowIfCancellationRequested();
            if (disposed)
                return null;
            if (sourceTextures.Remove(assetPath, out CachedSourceTexture? previous))
                sourceTextureBytes -= previous.Texture.Bytes;
            sourceTextures[assetPath] = new CachedSourceTexture(loaded, ++sourceTextureAccessOrder,
                clock.Elapsed + TimeSpan.FromSeconds(1));
            sourceTextureBytes += loaded.Bytes;
            sourceFailures.Remove(assetPath);
            trimSourceTextures(assetPath);
            succeeded = true;
            return loaded;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException
            or ArgumentException or NotSupportedException or ExternalException)
        {
            return null;
        }
        finally
        {
            if (sourceLoads.TryGetValue(assetPath, out SourceLoad? current) && ReferenceEquals(current, load))
                sourceLoads.Remove(assetPath);
            if (!disposed && !token.IsCancellationRequested && !succeeded)
            {
                if (sourceTextures.Remove(assetPath, out CachedSourceTexture? stale))
                    sourceTextureBytes -= stale.Texture.Bytes;
                sourceFailures[assetPath] = clock.Elapsed + TimeSpan.FromSeconds(1);
                if (sourceFailures.Count > MaximumSourceTextures)
                    sourceFailures.Remove(sourceFailures.First().Key);
            }
            load.Cancellation.Dispose();
        }
    }

    private SourceFile? readSourceFile(string assetPath)
    {
        if (!GameAssetPath.TryResolveExistingFile(projectPath, assetPath, out string path))
            return null;
        FileInfo info = new(path);
        return info.Exists ? new SourceFile(path, info.LastWriteTimeUtc, info.Length) : null;
    }

    private void trimSourceTextures(string retainedPath)
    {
        if (sourceTextures.Count <= MaximumSourceTextures
            && sourceTextureBytes <= MaximumSourceTextureBytes)
        {
            return;
        }
        foreach (string path in sourceTextures
                     .Where(entry => !string.Equals(entry.Key, retainedPath, StringComparison.Ordinal))
                     .OrderBy(entry => entry.Value.LastUsed)
                     .Select(entry => entry.Key)
                     .ToArray())
        {
            if (sourceTextures.Count <= MaximumSourceTextures
                && sourceTextureBytes <= MaximumSourceTextureBytes)
            {
                break;
            }
            CachedSourceTexture removed = sourceTextures[path];
            sourceTextures.Remove(path);
            sourceTextureBytes -= removed.Texture.Bytes;
        }
    }

    private sealed class CachedSourceTexture(
        SourceTexture texture,
        long lastUsed,
        TimeSpan validateAfter)
    {
        public SourceTexture Texture { get; } = texture;
        public long LastUsed { get; set; } = lastUsed;
        public TimeSpan ValidateAfter { get; set; } = validateAfter;
    }

    private sealed class SourceLoad(CancellationTokenSource cancellation)
    {
        public CancellationTokenSource Cancellation { get; } = cancellation;
        public Task<SourceTexture?> Task { get; set; } = null!;
        public int Users { get; set; }
    }

    private sealed record SourceFile(string Path, DateTime ModifiedAt, long Length);

    private sealed class SourceTexture
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
        public long Bytes => pixels.LongLength;
        public DateTime LastWriteTimeUtc { get; }
        public long Length { get; }

        public static SourceTexture Load(Bitmap bitmap, DateTime lastWriteTimeUtc, long length, CancellationToken cancellationToken)
        {
            cancellationToken.ThrowIfCancellationRequested();
            using WriteableBitmap buffer = new(
                bitmap.PixelSize,
                bitmap.Dpi,
                PixelFormat.Bgra8888,
                AlphaFormat.Unpremul);
            using ILockedFramebuffer frame = buffer.Lock();
            bitmap.CopyPixels(frame);
            cancellationToken.ThrowIfCancellationRequested();
            byte[] pixels = new byte[frame.RowBytes * frame.Size.Height];
            Marshal.Copy(frame.Address, pixels, 0, pixels.Length);
            int width = frame.Size.Width;
            int height = frame.Size.Height;
            int stride = frame.RowBytes;
            return new SourceTexture(width, height, stride, pixels, lastWriteTimeUtc, length);
        }

        public byte[] Copy(PixelRect rect, double hue, CancellationToken cancellationToken)
        {
            byte[] result = new byte[rect.Width * rect.Height * 4];
            for (int y = 0; y < rect.Height; y += 1)
            {
                cancellationToken.ThrowIfCancellationRequested();
                int sourceOffset = (rect.Y + y) * Stride + rect.X * 4;
                int targetOffset = y * rect.Width * 4;
                Buffer.BlockCopy(pixels, sourceOffset, result, targetOffset, rect.Width * 4);
            }
            EditorBitmapEffects.ApplyHueShiftBgra(result, rect.Width, rect.Height, rect.Width * 4, hue, cancellationToken);
            return result;
        }
    }
}
