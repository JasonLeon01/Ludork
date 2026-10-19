using Avalonia.Controls;
using Avalonia.Input;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class FilePreviewDialog : Window
{
    private EditorThumbnailLease? image;
    private bool closed;
    private MediaPreview? media;
    private readonly string? sourcePath;
    private readonly EditorThumbnailService? thumbnails;
    private CancellationTokenSource? imageRequest;

    public FilePreviewDialog()
    {
        InitializeComponent();
        PreviewStatus.Text = LocaleService.Get("LOADING");
        Closed += onClosed;
        KeyDown += (_, args) =>
        {
            if (args.Key == Key.Escape)
            {
                args.Handled = true;
                Close();
            }
        };
    }

    public FilePreviewDialog(string path, EditorThumbnailService thumbnails) : this()
    {
        sourcePath = Path.GetFullPath(path);
        this.thumbnails = thumbnails;
        thumbnails.SourcesInvalidated += onSourcesInvalidated;
        Title = Path.GetFileName(path);
        if (MediaPreview.CanPreview(path))
        {
            media = new MediaPreview();
            media.CloseRequested += (_, _) => Close();
            media.Load(path);
            Content = media;
            return;
        }
        Control? previewContent = Content as Control;
        _ = new DeferredWindowInitializer(this, async cancellationToken =>
        {
            Content = previewContent;
            await loadImageAsync(cancellationToken);
        });
    }

    private async Task loadImageAsync(CancellationToken cancellationToken = default)
    {
        if (closed || thumbnails is null || sourcePath is null)
            return;
        imageRequest?.Cancel();
        imageRequest?.Dispose();
        imageRequest = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        CancellationToken token = imageRequest.Token;
        try
        {
            EditorThumbnailLease? loaded = await thumbnails.AcquireAsync(sourcePath, 0, token);
            if (closed || token.IsCancellationRequested)
            {
                loaded?.Dispose();
                return;
            }
            EditorThumbnailLease? previous = image;
            image = loaded;
            PreviewImage.Source = image?.Bitmap;
            previous?.Dispose();
            PreviewStatus.Text = image is null ? LocaleService.Get("ERROR") : string.Empty;
            PreviewStatus.IsVisible = image is null;
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested)
        {
        }
    }

    private async void onSourcesInvalidated(object? sender, IReadOnlySet<string> paths)
    {
        if (closed || sourcePath is null || !paths.Contains(sourcePath))
            return;
        if (media is not null)
            media.Load(sourcePath);
        else
            await loadImageAsync();
    }

    private void onClosed(object? sender, EventArgs args)
    {
        closed = true;
        if (thumbnails is not null)
            thumbnails.SourcesInvalidated -= onSourcesInvalidated;
        imageRequest?.Cancel();
        imageRequest?.Dispose();
        imageRequest = null;
        media?.Clear();
        media = null;
        PreviewImage.Source = null;
        image?.Dispose();
        image = null;
        Closed -= onClosed;
    }
}
