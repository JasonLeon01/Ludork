using Avalonia.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.IO;

namespace Ludork.Views;

public partial class FilePreviewDialog : Window
{
    private EditorThumbnailLease? image;
    private bool closed;

    public FilePreviewDialog()
    {
        InitializeComponent();
        PreviewStatus.Text = LocaleService.Get("LOADING");
        Closed += onClosed;
    }

    public FilePreviewDialog(string path, EditorThumbnailService thumbnails) : this()
    {
        Title = Path.GetFileName(path);
        Control? previewContent = Content as Control;
        _ = new DeferredWindowInitializer(this, async cancellationToken =>
        {
            Content = previewContent;
            EditorThumbnailLease? loaded = await thumbnails.AcquireAsync(path, 0, cancellationToken);
            if (closed || cancellationToken.IsCancellationRequested)
            {
                loaded?.Dispose();
                return;
            }
            image = loaded;
            PreviewImage.Source = image?.Bitmap;
            PreviewStatus.Text = image is null ? LocaleService.Get("ERROR") : string.Empty;
            PreviewStatus.IsVisible = image is null;
        });
    }

    private void onClosed(object? sender, EventArgs args)
    {
        closed = true;
        PreviewImage.Source = null;
        image?.Dispose();
        image = null;
        Closed -= onClosed;
    }
}
