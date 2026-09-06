using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Services;
using System;
using System.Linq;

namespace Ludork.ViewModels;

public sealed class ActorQueueItemViewModel : ViewModelBase, IDisposable
{
    private readonly ActorPreviewService actorPreviewService;
    private IImage? fallback;
    private ActorVisualDescriptor? descriptor;
    private ActorPreviewLease? previewLease;
    private IImage? icon;
    private long previewFrameRevision;
    private bool previewActive;
    private bool isFavorite;
    private bool isRecent;
    private bool disposed;

    public ActorQueueItemViewModel(
        string blueprintReference,
        IImage? fallback,
        ActorVisualDescriptor? descriptor,
        ActorPreviewService actorPreviewService,
        bool isFavorite,
        bool isRecent)
    {
        BlueprintReference = blueprintReference;
        this.fallback = fallback;
        this.descriptor = descriptor;
        this.actorPreviewService = actorPreviewService;
        this.isFavorite = isFavorite;
        this.isRecent = isRecent;
        icon = fallback;
        Key = blueprintReference["Data.Blueprints.".Length..].Replace('.', '/');
        DisplayName = Key.Split('/').LastOrDefault() ?? Key;
        string[] pathParts = Key.Split('/', StringSplitOptions.RemoveEmptyEntries);
        Category = pathParts.Length > 1 ? pathParts[0] : LocaleService.Get("UNCATEGORISED");
    }

    public string BlueprintReference { get; }
    public string Key { get; }
    public string DisplayName { get; }
    public string Category { get; }
    public IImage? Icon
    {
        get => icon;
        private set => SetProperty(ref icon, value);
    }
    public long PreviewFrameRevision
    {
        get => previewFrameRevision;
        private set => SetProperty(ref previewFrameRevision, value);
    }
    public bool IsFavorite
    {
        get => isFavorite;
        set => SetProperty(ref isFavorite, value);
    }
    public bool IsRecent
    {
        get => isRecent;
        set => SetProperty(ref isRecent, value);
    }
    public bool IsPreviewActive
    {
        get => previewActive;
        set
        {
            if (previewActive == value || disposed)
                return;
            previewActive = value;
            if (value)
                ensurePreviewLease();
            if (previewLease is not null)
                previewLease.IsActive = value;
        }
    }

    public void UpdatePreview(ActorVisualDescriptor? nextDescriptor, IImage? nextFallback)
    {
        IImage? previousFallback = fallback;
        fallback = nextFallback;
        descriptor = nextDescriptor;
        releasePreviewLease();
        Icon = fallback;
        if (previewActive)
            ensurePreviewLease();
        if (!ReferenceEquals(previousFallback, nextFallback))
            (previousFallback as IDisposable)?.Dispose();
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        releasePreviewLease();
        (fallback as IDisposable)?.Dispose();
        fallback = null;
        Icon = null;
    }

    private void ensurePreviewLease()
    {
        if (previewLease is not null || descriptor is not { RequiresPreviewService: true } visual)
            return;
        previewLease = actorPreviewService.Acquire(visual, 48, previewActive);
        previewLease.FrameChanged += onPreviewFrameChanged;
        updateIcon();
    }

    private void releasePreviewLease()
    {
        if (previewLease is null)
            return;
        previewLease.FrameChanged -= onPreviewFrameChanged;
        previewLease.Dispose();
        previewLease = null;
    }

    private void onPreviewFrameChanged(object? sender, EventArgs args)
    {
        if (Dispatcher.UIThread.CheckAccess())
        {
            updatePreviewFrame();
            return;
        }
        Dispatcher.UIThread.Post(updatePreviewFrame);
    }

    private void updatePreviewFrame()
    {
        if (disposed)
            return;
        updateIcon();
        PreviewFrameRevision += 1;
    }

    private void updateIcon()
    {
        if (disposed)
            return;
        Icon = previewLease?.Frame ?? fallback;
    }
}
