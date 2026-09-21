using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Services;
using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.ViewModels;

public sealed class FileExplorerEntryViewModel : ViewModelBase, IDisposable
{
    private readonly EditorThumbnailService thumbnails;
    private readonly BlueprintPreviewService previews;
    private readonly IImage placeholder;
    private readonly bool image;
    private readonly Dictionary<object, (bool Visible, int Size)> presentations = [];
    private CancellationTokenSource? request;
    private EditorThumbnailLease? thumbnail;
    private ActorPreviewLease? previewLease;
    private ActorVisualDescriptor? visualDescriptor;
    private IImage? icon;
    private long previewFrameRevision;
    private bool disposed;
    private bool isModified;
    private bool visible;
    private bool preload;
    private int presentationSize;
    private string? blueprintReference;
    private string revision = string.Empty;
    private Task<DataFileInfo?>? dataInfo;

    public FileExplorerEntryViewModel(string fullPath, bool isDirectory, IImage placeholder,
        EditorThumbnailService thumbnails, BlueprintPreviewService previews)
    {
        FullPath = fullPath;
        IsDirectory = isDirectory;
        this.placeholder = placeholder;
        this.thumbnails = thumbnails;
        this.previews = previews;
        icon = placeholder;
        image = Path.GetExtension(fullPath).ToLowerInvariant() is ".png" or ".jpg" or ".jpeg" or ".bmp" or ".gif" or ".webp";
    }

    public string FullPath { get; private set; }
    public string Name => Path.GetFileName(FullPath);
    public bool IsDirectory { get; }
    public IImage? Icon { get => icon; private set => SetProperty(ref icon, value); }
    public long PreviewFrameRevision { get => previewFrameRevision; private set => SetProperty(ref previewFrameRevision, value); }
    public bool IsModified { get => isModified; set => SetProperty(ref isModified, value); }

    public Task<DataFileInfo?> ReadInfoAsync(ProjectDataStore gameData) => dataInfo ??= gameData.TryLoadDataFileAsync(FullPath);

    public void UpdatePath(string path)
    {
        if (FullPath == path)
            return;
        FullPath = path;
        dataInfo = null;
        OnPropertyChanged(nameof(FullPath));
        OnPropertyChanged(nameof(Name));
        releasePreview();
        if (preload)
            startPreview();
    }

    public void UpdateSource(string version, string? reference)
    {
        if (revision == version && blueprintReference == reference)
            return;
        revision = version;
        dataInfo = null;
        blueprintReference = reference;
        releasePreview();
        if (preload)
            startPreview();
    }

    public void SetPresentation(object consumer, bool isVisible, bool shouldPreload, int pixelSize)
    {
        if (disposed)
            return;
        if (shouldPreload)
            presentations[consumer] = (isVisible, pixelSize);
        else
            presentations.Remove(consumer);
        visible = presentations.Values.Any(value => value.Visible);
        preload = presentations.Count > 0;
        pixelSize = presentations.Values.Select(value => value.Size).DefaultIfEmpty(pixelSize).Max();
        if (!preload || presentationSize != pixelSize)
            releasePreview();
        presentationSize = pixelSize;
        if (preload && request is null)
            startPreview();
        ensureActorPreview();
        if (previewLease is not null)
            previewLease.IsActive = visible;
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        presentations.Clear();
        releasePreview();
    }

    private void startPreview()
    {
        if (IsDirectory || !image && blueprintReference is null || request is not null)
            return;
        request = new CancellationTokenSource();
        _ = loadPreviewAsync(request.Token);
    }

    private async Task loadPreviewAsync(CancellationToken token)
    {
        EditorThumbnailLease? loaded = null;
        try
        {
            ActorVisualDescriptor? visual = null;
            if (image)
                loaded = await thumbnails.AcquireAsync(FullPath, presentationSize, token);
            else if (blueprintReference is not null)
                (loaded, visual) = await previews.LoadPreviewAsync(blueprintReference, presentationSize, token);
            token.ThrowIfCancellationRequested();
            thumbnail = loaded;
            loaded = null;
            visualDescriptor = visual;
            ensureActorPreview();
            updateFrame();
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
        visualDescriptor = null;
    }

    private void ensureActorPreview()
    {
        if (visible && previewLease is null && visualDescriptor is { RequiresPreviewService: true })
        {
            previewLease = previews.ActorPreviews.Acquire(visualDescriptor, presentationSize, true);
            previewLease.FrameChanged += onPreviewFrameChanged;
        }
    }

    private void onPreviewFrameChanged(object? sender, EventArgs args)
    {
        if (Dispatcher.UIThread.CheckAccess())
            updateFrame();
        else
            Dispatcher.UIThread.Post(updateFrame);
    }

    private void updateFrame()
    {
        if (disposed)
            return;
        Icon = previewLease?.Frame ?? thumbnail?.Bitmap ?? placeholder;
        PreviewFrameRevision++;
    }
}
