using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Ludork.Services;
using System.Runtime.InteropServices;

namespace Ludork.Controls;

public sealed class ParticlePreviewSurface : Border
{
    private readonly Image image = new() { Stretch = Stretch.Uniform };
    private WriteableBitmap? bitmap;

    public ParticlePreviewSurface()
    {
        Child = image;
        Background = new SolidColorBrush(Color.Parse("#161B22"));
        ClipToBounds = true;
        MinWidth = 360;
        MinHeight = 240;
    }

    public void SetFrame(ParticlePreviewFrame frame)
    {
        WriteableBitmap next = new(new PixelSize(frame.Width, frame.Height), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Premul);
        using (ILockedFramebuffer buffer = next.Lock())
        {
            for (int row = 0; row < frame.Height; row++)
                Marshal.Copy(frame.Pixels, row * frame.Stride, buffer.Address + row * buffer.RowBytes, frame.Width * 4);
        }
        WriteableBitmap? previous = bitmap;
        bitmap = next;
        image.Source = bitmap;
        previous?.Dispose();
    }

    public void Clear()
    {
        image.Source = null;
        bitmap?.Dispose();
        bitmap = null;
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        Clear();
        base.OnDetachedFromVisualTree(args);
    }
}
