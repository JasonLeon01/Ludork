using Avalonia.Media.Imaging;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class EditorThumbnailService : IDisposable
{
    private const long IdleByteLimit = 64L * 1024 * 1024;
    private readonly object sync = new();
    private readonly Dictionary<CacheKey, CacheEntry> entries = [];
    private readonly SemaphoreSlim workers = new(2, 2);
    private readonly CancellationTokenSource lifetime = new();
    private long sequence;
    private bool disposed;

    public Task<EditorThumbnailLease?> AcquireAsync(
        string path,
        int pixelWidth,
        CancellationToken cancellationToken = default)
    {
        return AcquireAsync(path, pixelWidth, string.Empty, () =>
        {
            using FileStream stream = File.OpenRead(path);
            return pixelWidth > 0 ? Bitmap.DecodeToWidth(stream, pixelWidth) : new Bitmap(stream);
        }, cancellationToken);
    }

    public Task<EditorThumbnailLease?> AcquireAsync(
        string path,
        int pixelWidth,
        string variant,
        Func<Bitmap?> decode,
        CancellationToken cancellationToken = default)
    {
        return acquireAsync(path, pixelWidth, variant, decode, cancellationToken, null);
    }

    internal Task<EditorThumbnailLease?> AcquireDerivedAsync(string path, int pixelWidth, string variant,
        EditorThumbnailLease source, Func<Bitmap, Bitmap?> decode, CancellationToken cancellationToken)
    {
        EditorThumbnailLease retained = source.Retain();
        return acquireAsync(path, pixelWidth, variant + ":" + source.SourceVersion,
            () => decode(retained.Bitmap), cancellationToken, retained);
    }

    private async Task<EditorThumbnailLease?> acquireAsync(string path, int pixelWidth, string variant,
        Func<Bitmap?> decode, CancellationToken cancellationToken, IDisposable? decodeResource)
    {
        bool transferred = false;
        try
        {
            CancellationToken token;
            lock (sync)
            {
                ObjectDisposedException.ThrowIf(disposed, this);
                token = lifetime.Token;
            }
            using CancellationTokenSource request = CancellationTokenSource.CreateLinkedTokenSource(token, cancellationToken);
            CacheKey? key = await Task.Run(() => readKey(path, pixelWidth, variant), request.Token).ConfigureAwait(false);
            request.Token.ThrowIfCancellationRequested();
            if (key is null)
                return null;
            CacheEntry entry;
            lock (sync)
            {
                request.Token.ThrowIfCancellationRequested();
                if (!entries.TryGetValue(key.Value, out entry!))
                {
                    entry = new CacheEntry(key.Value, CancellationTokenSource.CreateLinkedTokenSource(token));
                    entry.DecodeResource = decodeResource;
                    transferred = true;
                    entries.Add(key.Value, entry);
                    entry.Loading = Task.Run(() => loadAsync(entry, decode, entry.Request.Token));
                }
                entry.Users++;
                entry.LastUse = ++sequence;
            }
            try
            {
                Bitmap? bitmap = await entry.Loading.WaitAsync(request.Token).ConfigureAwait(false);
                request.Token.ThrowIfCancellationRequested();
                if (bitmap is not null)
                    return new EditorThumbnailLease(bitmap, entry.SourceVersion, () => release(entry), () => retain(entry));
            }
            catch
            {
                release(entry);
                throw;
            }
            release(entry);
            return null;
        }
        finally
        {
            if (!transferred)
                decodeResource?.Dispose();
        }
    }

    public void Dispose()
    {
        lock (sync)
        {
            if (disposed)
                return;
            disposed = true;
            lifetime.Cancel();
            foreach (CacheEntry entry in entries.Values.ToArray())
            {
                if (entry.Users == 0 && entry.Completed)
                    remove(entry);
            }
        }
    }

    private async Task<Bitmap?> loadAsync(CacheEntry entry, Func<Bitmap?> decode, CancellationToken token)
    {
        Bitmap? bitmap = null;
        bool entered = false;
        try
        {
            await workers.WaitAsync(token).ConfigureAwait(false);
            entered = true;
            lock (sync)
                entry.Started = true;
            token.ThrowIfCancellationRequested();
            bitmap = decode();
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException
            or ArgumentException or NotSupportedException or ExternalException)
        {
            bitmap = null;
        }
        finally
        {
            if (entered)
                workers.Release();
            entry.DecodeResource?.Dispose();
            entry.DecodeResource = null;
            lock (sync)
            {
                entry.Bitmap = bitmap;
                entry.Completed = true;
                if (!entries.TryGetValue(entry.Key, out CacheEntry? current) || !ReferenceEquals(current, entry))
                    bitmap?.Dispose();
                else if (entry.Users == 0)
                    trim();
                entry.Request.Dispose();
            }
        }
        return bitmap;
    }

    private void release(CacheEntry entry)
    {
        lock (sync)
        {
            entry.Users--;
            entry.LastUse = ++sequence;
            if (entry.Users == 0)
            {
                if (!entry.Started && !entry.Completed)
                {
                    entry.Request.Cancel();
                    remove(entry);
                }
                trim();
            }
        }
    }

    private EditorThumbnailLease retain(CacheEntry entry)
    {
        lock (sync)
        {
            ObjectDisposedException.ThrowIf(entry.Users == 0 || entry.Bitmap is null, this);
            entry.Users++;
            return new EditorThumbnailLease(entry.Bitmap, entry.SourceVersion, () => release(entry), () => retain(entry));
        }
    }

    private void trim()
    {
        CacheEntry[] idle = entries.Values.Where(entry => entry.Users == 0 && entry.Completed)
            .OrderBy(entry => entry.LastUse).ToArray();
        long bytes = idle.Sum(entry => entry.Bytes);
        foreach (CacheEntry entry in idle)
        {
            if (disposed || entry.Bitmap is null || bytes > IdleByteLimit)
            {
                bytes -= entry.Bytes;
                remove(entry);
            }
        }
    }

    private void remove(CacheEntry entry)
    {
        if (entries.TryGetValue(entry.Key, out CacheEntry? current) && ReferenceEquals(current, entry))
            entries.Remove(entry.Key);
        entry.Bitmap?.Dispose();
        entry.Bitmap = null;
    }

    private static CacheKey? readKey(string path, int pixelWidth, string variant)
    {
        try
        {
            FileInfo info = new(path);
            return info.Exists
                ? new CacheKey(info.FullName, pixelWidth, variant, info.Length, info.LastWriteTimeUtc.Ticks)
                : null;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or ArgumentException)
        {
            return null;
        }
    }

    private readonly record struct CacheKey(string Path, int Width, string Variant, long Length, long ModifiedAt);

    private sealed class CacheEntry(CacheKey key, CancellationTokenSource request)
    {
        public CacheKey Key { get; } = key;
        public string SourceVersion => $"{Key.ModifiedAt}:{Key.Length}";
        public CancellationTokenSource Request { get; } = request;
        public Task<Bitmap?> Loading { get; set; } = null!;
        public Bitmap? Bitmap { get; set; }
        public bool Completed { get; set; }
        public bool Started { get; set; }
        public IDisposable? DecodeResource { get; set; }
        public int Users { get; set; }
        public long LastUse { get; set; }
        public long Bytes => Bitmap is null ? 0 : (long)Bitmap.PixelSize.Width * Bitmap.PixelSize.Height * 4;
    }
}
