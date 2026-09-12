using Avalonia.Media.Imaging;
using System;
using System.Threading;

namespace Ludork.Services;

public sealed class EditorThumbnailLease : IDisposable
{
    private Action? release;
    private readonly Func<EditorThumbnailLease> retain;

    internal EditorThumbnailLease(Bitmap bitmap, string sourceVersion, Action release, Func<EditorThumbnailLease> retain)
    {
        Bitmap = bitmap;
        SourceVersion = sourceVersion;
        this.release = release;
        this.retain = retain;
    }

    public Bitmap Bitmap { get; }
    internal string SourceVersion { get; }

    internal EditorThumbnailLease Retain()
    {
        ObjectDisposedException.ThrowIf(Volatile.Read(ref release) is null, this);
        return retain();
    }

    public void Dispose() => Interlocked.Exchange(ref release, null)?.Invoke();
}
