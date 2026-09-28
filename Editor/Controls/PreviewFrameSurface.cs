using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using System.Runtime.InteropServices;

namespace Ludork.Controls;

public class PreviewFrameSurface : Border
{
    private readonly Image image = new() { Stretch = Stretch.Uniform };
    private WriteableBitmap? bitmap;

    public PreviewFrameSurface()
    {
        Child = image;
        Background = new SolidColorBrush(Color.Parse("#161B22"));
        ClipToBounds = true;
        MinWidth = 360;
        MinHeight = 240;
    }

    public void SetFrame(int width, int height, int stride, byte[] pixels)
    {
        WriteableBitmap next = new(new PixelSize(width, height), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Premul);
        using (ILockedFramebuffer buffer = next.Lock())
        {
            for (int row = 0; row < height; row++)
                Marshal.Copy(pixels, row * stride, buffer.Address + row * buffer.RowBytes, width * 4);
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
