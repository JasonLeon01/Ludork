using Avalonia.Controls;
using Avalonia.Input;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.IO;

namespace Ludork.Views;

public partial class FilePreviewDialog : Window
{
    private EditorThumbnailLease? image;
    private bool closed;
    private MediaPreview? media;

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
        media?.Clear();
        media = null;
        PreviewImage.Source = null;
        image?.Dispose();
        image = null;
        Closed -= onClosed;
    }
}
