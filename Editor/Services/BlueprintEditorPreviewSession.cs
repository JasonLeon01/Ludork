using Avalonia.Media.Imaging;
using Ludork.Models;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal sealed class BlueprintEditorPreviewSession : IDisposable
{
    private readonly BlueprintPreviewService previews;
    private ActorPreviewLease? lease;
    private EditorThumbnailLease? thumbnail;
    private CancellationTokenSource? request;
    private ActorVisualDescriptor? publishedDescriptor;
    private ResolvedBlueprintClass? lastResolved;
    private bool descriptorPublished;
    private bool visible;
    private bool readOnly;
    private bool disposed;

    public BlueprintEditorPreviewSession(BlueprintPreviewService previews)
    {
        this.previews = previews;
        previews.Runtime.ResourcesInvalidated += onResourcesInvalidated;
    }

    public event EventHandler? FrameChanged;
    public Bitmap? Frame => lease?.Frame ?? thumbnail?.Bitmap;

    public bool IsVisible
    {
        get => visible;
        set
        {
            visible = value;
            updateActivity();
        }
    }

    public bool IsReadOnly
    {
        get => readOnly;
        set
        {
            readOnly = value;
            updateActivity();
        }
    }

    public async Task RefreshAsync(ResolvedBlueprintClass resolved, bool suppressInvalidation)
    {
        if (disposed)
            return;
        lastResolved = resolved;
        request?.Cancel();
        request?.Dispose();
        request = new CancellationTokenSource();
        CancellationToken cancellationToken = request.Token;
        try
        {
            (EditorThumbnailLease? next, ActorVisualDescriptor? descriptor) =
                await previews.LoadPreviewAsync(resolved, resolved.ClassReference, 480, cancellationToken);
            if (disposed || cancellationToken.IsCancellationRequested)
            {
                next?.Dispose();
                return;
            }
            if (!suppressInvalidation && descriptorPublished && !Equals(publishedDescriptor, descriptor))
                previews.InvalidateVisuals();
            publishedDescriptor = descriptor;
            descriptorPublished = true;
            if (readOnly)
            {
                next?.Dispose();
                updateActivity();
                return;
            }
            if (descriptor is not { RequiresPreviewService: true })
                releaseLease();
            else if (lease is null)
            {
                lease = previews.ActorPreviews.Acquire(descriptor, 480, visible);
                lease.FrameChanged += onFrameChanged;
            }
            else
                lease.UpdateDescriptor(descriptor);
            updateActivity();
            EditorThumbnailLease? previous = thumbnail;
            thumbnail = next;
            FrameChanged?.Invoke(this, EventArgs.Empty);
            previous?.Dispose();
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        previews.Runtime.ResourcesInvalidated -= onResourcesInvalidated;
        request?.Cancel();
        request?.Dispose();
        request = null;
        releaseLease();
        thumbnail?.Dispose();
        thumbnail = null;
    }

    private void updateActivity()
    {
        if (lease is not null)
            lease.IsActive = !disposed && visible && !readOnly;
    }

    private async void onResourcesInvalidated(object? sender, EventArgs args)
    {
        if (lastResolved is not null)
            await RefreshAsync(lastResolved, true);
    }

    private void onFrameChanged(object? sender, EventArgs args)
    {
        if (!disposed)
            FrameChanged?.Invoke(this, args);
    }

    private void releaseLease()
    {
        if (lease is null)
            return;
        lease.FrameChanged -= onFrameChanged;
        lease.Dispose();
        lease = null;
    }
}
