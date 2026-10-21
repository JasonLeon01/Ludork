using Avalonia;
using Avalonia.Media.Imaging;
using System;
using System.Diagnostics;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class MapPreviewLease : IDisposable
{
    private readonly MapPreviewService owner;
    internal MapPreviewLease(MapPreviewService owner, bool thumbnail)
    {
        this.owner = owner;
        IsThumbnail = thumbnail;
    }

    internal string Id { get; } = Guid.NewGuid().ToString("N");
    internal string SceneKey => Id + "/" + identity;
    internal bool IsThumbnail { get; }
    internal bool Pending { get; set; }
    internal bool PriorityPending { get; set; } = true;
    internal double RetryAfter { get; set; }
    internal bool Disposed { get; private set; }
    internal long Generation { get; private set; }
    internal long Epoch { get; private set; }
    internal long PublishedSequence { get; set; }
    private string identity = string.Empty;
    internal long Revision { get; private set; }
    internal JsonObject? Scene { get; private set; }
    internal Rect View { get; private set; }
    internal PixelSize Size { get; private set; }
    internal MapPreviewOptions Options { get; private set; } = new();
    internal string SelectedLayer { get; private set; } = string.Empty;
    public Bitmap? Frame { get; private set; }
    public Rect FrameView { get; private set; }
    public string Error { get; private set; } = string.Empty;
    public MapPreviewFrameTiming? LastFrameTiming { get; internal set; }
    internal long ChangedAt { get; private set; } = Stopwatch.GetTimestamp();
    internal long? SelectionStartedAt { get; set; }
    internal double SceneUpdateMilliseconds { get; private set; }
    private bool changeRecorded;
    private double? lastParticleFrame;
    private bool resetParticleMotion = true;
    public bool IsActive { get; set; }
    public bool IsInteracting { get; set; }
    internal Func<bool>? Visible { get; set; }
    internal bool CanRender => IsActive && !Disposed && Visible?.Invoke() != false;
    public event EventHandler? FrameChanged;

    public void Request(JsonObject scene, long revision, Rect view, PixelSize size, MapPreviewOptions options, string? selectedLayer = null, string? sceneIdentity = null)
    {
        if (Disposed || size.Width <= 0 || size.Height <= 0 || view.Width <= 0 || view.Height <= 0)
            return;
        bool changed = !ReferenceEquals(Scene, scene) || Revision != revision || View != view || Size != size
            || Options != options || SelectedLayer != (selectedLayer ?? string.Empty);
        if (identity != (sceneIdentity ?? string.Empty) || Options != options || SelectedLayer != (selectedLayer ?? string.Empty))
        {
            Epoch++;
            changed = true;
        }
        if (identity != (sceneIdentity ?? string.Empty))
        {
            PriorityPending = true;
            SuspendParticles();
        }
        identity = sceneIdentity ?? string.Empty;
        Scene = scene;
        Revision = revision;
        View = view;
        Size = size;
        Options = options;
        SelectedLayer = selectedLayer ?? string.Empty;
        IsActive = true;
        if (changed)
        {
            if (!changeRecorded)
                ChangedAt = Stopwatch.GetTimestamp();
            changeRecorded = false;
            Generation++;
            Pending = true;
            RetryAfter = 0;
        }
        owner.Schedule(this, changed);
    }

    internal void RecordSceneUpdate(long started)
    {
        ChangedAt = started;
        changeRecorded = true;
        SceneUpdateMilliseconds = Stopwatch.GetElapsedTime(started).TotalMilliseconds;
    }

    internal void RecordSelection(long started)
    {
        SelectionStartedAt = started;
        PriorityPending = true;
    }

    internal (double Delta, bool ResetMotion) ParticleFrame(double now, bool realtime)
    {
        bool advance = realtime && !IsThumbnail && CanRender;
        double delta = advance && lastParticleFrame is double previous
            ? Math.Clamp(now - previous, 0, 0.25) : 0;
        bool reset = resetParticleMotion || !advance;
        lastParticleFrame = advance ? now : null;
        resetParticleMotion = !advance;
        return (delta, reset);
    }

    internal void SuspendParticles()
    {
        lastParticleFrame = null;
        resetParticleMotion = true;
    }

    internal void Publish(Bitmap? bitmap, Rect view, string error)
    {
        Bitmap? old = Frame;
        Frame = bitmap;
        FrameView = view;
        Error = error;
        FrameChanged?.Invoke(this, EventArgs.Empty);
        old?.Dispose();
    }

    internal void Invalidate(bool incompatible = true)
    {
        if (incompatible)
            Epoch++;
        Generation++;
        Pending = true;
        RetryAfter = 0;
    }

    public void Dispose()
    {
        if (Disposed)
            return;
        Disposed = true;
        owner.Release(this);
        Frame?.Dispose();
        Frame = null;
    }
}
