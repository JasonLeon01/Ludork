using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.ViewModels;

public sealed class ActorQueueItemViewModel : ViewModelBase, IDisposable
{
    private readonly BlueprintPreviewService previews;
    private readonly IImage placeholder;
    private readonly Dictionary<object, int> presentations = new(ReferenceEqualityComparer.Instance);
    private ResolvedBlueprintClass resolved;
    private long visualRevision;
    private CancellationTokenSource? request;
    private EditorThumbnailLease? thumbnail;
    private ActorPreviewLease? previewLease;
    private IImage? icon;
    private long previewFrameRevision;
    private int presentationSize;
    private bool isFavorite;
    private bool isRecent;
    private bool disposed;

    public ActorQueueItemViewModel(
        string blueprintReference,
        IImage placeholder,
        ResolvedBlueprintClass resolved,
        BlueprintPreviewService previews,
        long visualRevision,
        bool isFavorite,
        bool isRecent)
    {
        BlueprintReference = blueprintReference;
        this.placeholder = placeholder;
        this.resolved = resolved;
        this.previews = previews;
        this.visualRevision = visualRevision;
        this.isFavorite = isFavorite;
        this.isRecent = isRecent;
        icon = placeholder;
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
        set
        {
            if (SetProperty(ref isFavorite, value))
                OnPropertyChanged(nameof(FavoriteLabel));
        }
    }
    public string FavoriteLabel => LocaleService.Get(IsFavorite ? "REMOVE_FROM_FAVOURITES" : "ADD_TO_FAVOURITES");
    public bool IsRecent
    {
        get => isRecent;
        set => SetProperty(ref isRecent, value);
    }

    public void SetPreviewActive(object consumer, bool active, int pixelSize)
    {
        if (disposed)
            return;
        if (active)
            presentations[consumer] = Math.Max(1, pixelSize);
        else
            presentations.Remove(consumer);
        int size = presentations.Values.DefaultIfEmpty(48).Max();
        if (presentations.Count == 0 || presentationSize != size)
            releasePreview();
        presentationSize = size;
        if (presentations.Count != 0 && request is null)
            startPreview();
    }

    public void UpdateSource(ResolvedBlueprintClass next, long revision)
    {
        bool unchanged = resolved.ResolverRevision == next.ResolverRevision
            && resolved.MetadataRevision == next.MetadataRevision && visualRevision == revision;
        resolved = next;
        visualRevision = revision;
        if (unchanged || disposed)
            return;
        releasePreview();
        if (presentations.Count != 0)
            startPreview();
    }

    public void DeactivatePreviews()
    {
        presentations.Clear();
        releasePreview();
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        DeactivatePreviews();
        Icon = null;
    }

    private void startPreview()
    {
        request = new CancellationTokenSource();
        CancellationToken token = request.Token;
        ResolvedBlueprintClass source = resolved;
        int size = presentationSize;
        Dispatcher.UIThread.Post(() => _ = loadPreviewAsync(source, size, token), DispatcherPriority.Background);
    }

    private async Task loadPreviewAsync(ResolvedBlueprintClass source, int size, CancellationToken token)
    {
        EditorThumbnailLease? loaded = null;
        try
        {
            token.ThrowIfCancellationRequested();
            ActorVisualDescriptor? visual;
            (loaded, visual) = await previews.LoadPreviewAsync(source, BlueprintReference, size, token);
            token.ThrowIfCancellationRequested();
            if (disposed || presentations.Count == 0)
                return;
            thumbnail = loaded;
            loaded = null;
            if (visual is { RequiresPreviewService: true })
            {
                previewLease = previews.ActorPreviews.Acquire(visual, size, true);
                previewLease.FrameChanged += onPreviewFrameChanged;
            }
            updatePreviewFrame();
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested)
        {
        }
        catch (IOException)
        {
        }
        finally
        {
            loaded?.Dispose();
        }
    }

    private void releasePreview()
    {
        request?.Cancel();
        request?.Dispose();
        request = null;
        if (previewLease is not null)
        {
            previewLease.FrameChanged -= onPreviewFrameChanged;
            previewLease.Dispose();
            previewLease = null;
        }
        Icon = placeholder;
        thumbnail?.Dispose();
        thumbnail = null;
    }

    private void onPreviewFrameChanged(object? sender, EventArgs args)
    {
        if (Dispatcher.UIThread.CheckAccess())
            updatePreviewFrame();
        else
            Dispatcher.UIThread.Post(updatePreviewFrame);
    }

    private void updatePreviewFrame()
    {
        if (disposed || presentations.Count == 0)
            return;
        Icon = previewLease?.Frame ?? thumbnail?.Bitmap ?? placeholder;
        PreviewFrameRevision++;
    }
}
