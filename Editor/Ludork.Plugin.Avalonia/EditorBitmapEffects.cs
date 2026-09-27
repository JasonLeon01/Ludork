using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace Ludork.Plugin.Avalonia;

public static class EditorBitmapEffects
{
    private const double HueEpsilon = 0.0001;

    public static double NormalizeHue(double hue)
    {
        if (!double.IsFinite(hue))
            return 0;
        double normalized = hue % 360;
        if (normalized < 0)
            normalized += 360;
        return normalized <= HueEpsilon || 360 - normalized <= HueEpsilon ? 0 : normalized;
    }

    public static bool IsNeutralHue(double hue) => NormalizeHue(hue) == 0;

    public static Bitmap CreateHueShiftedBitmap(
        Bitmap source,
        double hue,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        WriteableBitmap result = new(source.PixelSize, source.Dpi, PixelFormat.Bgra8888, AlphaFormat.Unpremul);
        try
        {
            using ILockedFramebuffer frame = result.Lock();
            source.CopyPixels(frame);
            byte[] pixels = new byte[checked(frame.RowBytes * frame.Size.Height)];
            Marshal.Copy(frame.Address, pixels, 0, pixels.Length);
            ApplyHueShiftBgra(pixels, frame.Size.Width, frame.Size.Height, frame.RowBytes, hue, cancellationToken);
            Marshal.Copy(pixels, 0, frame.Address, pixels.Length);
            return result;
        }
        catch
        {
            result.Dispose();
            throw;
        }
    }

    public static void ApplyHueShiftBgra(
        Span<byte> pixels,
        int width,
        int height,
        int stride,
        double hue,
        CancellationToken cancellationToken = default)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(width);
        ArgumentOutOfRangeException.ThrowIfNegative(height);
        ArgumentOutOfRangeException.ThrowIfLessThan(stride, checked(width * 4));
        int requiredLength = height == 0 ? 0 : checked((height - 1) * stride + width * 4);
        if (pixels.Length < requiredLength)
            throw new ArgumentException("The pixel buffer is smaller than the supplied dimensions.", nameof(pixels));
        cancellationToken.ThrowIfCancellationRequested();
        double normalizedHue = NormalizeHue(hue);
        if (normalizedHue == 0)
            return;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                if ((x & 4095) == 0)
                    cancellationToken.ThrowIfCancellationRequested();
                int index = y * stride + x * 4;
                if (pixels[index + 3] == 0)
                    continue;
                HsvColor hsv = Color.ToHsv(pixels[index + 2], pixels[index + 1], pixels[index]);
                Color shifted = HsvColor.ToRgb((hsv.H + normalizedHue) % 360, hsv.S, hsv.V);
                pixels[index] = shifted.B;
                pixels[index + 1] = shifted.G;
                pixels[index + 2] = shifted.R;
            }
        }
    }
}
