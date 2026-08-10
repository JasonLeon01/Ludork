using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using System;
using System.IO;

namespace Ludork.Controls;

public sealed class AutoTilePreview : Control
{
    public static readonly StyledProperty<string?> ImagePathProperty =
        AvaloniaProperty.Register<AutoTilePreview, string?>(nameof(ImagePath));

    public static readonly StyledProperty<int> CellSizeProperty =
        AvaloniaProperty.Register<AutoTilePreview, int>(nameof(CellSize), 32);

    private static readonly IBrush EmptyBrush = new SolidColorBrush(Color.FromRgb(60, 60, 60));
    private Bitmap? bitmap;

    public string? ImagePath
    {
        get => GetValue(ImagePathProperty);
        set => SetValue(ImagePathProperty, value);
    }

    public int CellSize
    {
        get => GetValue(CellSizeProperty);
        set => SetValue(CellSizeProperty, value);
    }

    public override void Render(DrawingContext context)
    {
        int size = Math.Max(1, CellSize);
        Rect bounds = new Rect(0, 0, size, size);
        context.FillRectangle(EmptyBrush, bounds);
        if (bitmap is null)
            return;

        int cropWidth = Math.Min(size, bitmap.PixelSize.Width);
        int cropHeight = Math.Min(size, bitmap.PixelSize.Height);
        if (cropWidth <= 0 || cropHeight <= 0)
            return;

        double scale = Math.Min((double)size / cropWidth, (double)size / cropHeight);
        double destinationWidth = cropWidth * scale;
        double destinationHeight = cropHeight * scale;
        Rect destination = new Rect(
            (size - destinationWidth) / 2,
            (size - destinationHeight) / 2,
            destinationWidth,
            destinationHeight);
        context.DrawImage(bitmap, new Rect(0, 0, cropWidth, cropHeight), destination);
    }

    protected override Size MeasureOverride(Size availableSize)
    {
        int size = Math.Max(1, CellSize);
        return new Size(size, size);
    }

    protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change)
    {
        base.OnPropertyChanged(change);
        if (change.Property == ImagePathProperty)
            loadBitmap();
        else if (change.Property == CellSizeProperty)
        {
            InvalidateMeasure();
            InvalidateVisual();
        }
    }

    private void loadBitmap()
    {
        bitmap?.Dispose();
        bitmap = null;
        if (!string.IsNullOrWhiteSpace(ImagePath) && File.Exists(ImagePath))
        {
            bitmap = new Bitmap(ImagePath);
        }
        InvalidateVisual();
    }
}
