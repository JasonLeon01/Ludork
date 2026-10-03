using Avalonia.Media.Imaging;
using Avalonia.Threading;
using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal sealed class BlueprintPreviewSession : IDisposable
{
    private readonly BlueprintPreviewService previews;
    private CancellationTokenSource? request;
    private EditorThumbnailLease? thumbnail;
    private ActorPreviewLease? actor;
    private ActorVisualDescriptor? descriptor;
    private int size;
    private bool active;
    private bool disposed;

    public BlueprintPreviewSession(BlueprintPreviewService previews)
    {
        this.previews = previews;
    }

    public event EventHandler? FrameChanged;
    public Bitmap? Frame => actor?.Frame ?? thumbnail?.Bitmap;

    public bool IsActive
    {
        get => active;
        set
        {
            if (disposed)
                return;
            active = value;
            updateActivity();
        }
    }

    public async Task LoadAsync(string blueprintReference, int pixelSize, CancellationToken cancellationToken = default)
    {
        if (disposed)
            return;
        Clear();
        size = pixelSize;
        request = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        CancellationToken token = request.Token;
        EditorThumbnailLease? loaded = null;
        try
        {
            ActorVisualDescriptor? visual;
            (loaded, visual) = await previews.LoadPreviewAsync(blueprintReference, size, token);
            token.ThrowIfCancellationRequested();
            thumbnail = loaded;
            loaded = null;
            descriptor = visual;
            updateActivity();
            publishFrame();
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested)
        {
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
        }
        finally
        {
            loaded?.Dispose();
        }
    }

    public void Clear()
    {
        request?.Cancel();
        request?.Dispose();
        request = null;
        ActorPreviewLease? previousActor = actor;
        EditorThumbnailLease? previousThumbnail = thumbnail;
        if (previousActor is not null)
            previousActor.FrameChanged -= onFrameChanged;
        actor = null;
        thumbnail = null;
        descriptor = null;
        publishFrame();
        previousActor?.Dispose();
        previousThumbnail?.Dispose();
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        Clear();
    }

    private void updateActivity()
    {
        if (active && actor is null && descriptor is { RequiresPreviewService: true })
        {
            actor = previews.ActorPreviews.Acquire(descriptor, size, true);
            actor.FrameChanged += onFrameChanged;
        }
        if (actor is not null)
            actor.IsActive = active;
    }

    private void onFrameChanged(object? sender, EventArgs args)
    {
        if (Dispatcher.UIThread.CheckAccess())
            publishFrame();
        else
            Dispatcher.UIThread.Post(publishFrame);
    }

    private void publishFrame()
    {
        if (!disposed)
            FrameChanged?.Invoke(this, EventArgs.Empty);
    }
}
