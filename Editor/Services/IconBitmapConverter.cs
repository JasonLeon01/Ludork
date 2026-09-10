using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using System;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ludork.Services;

[SupportedOSPlatform("windows")]
internal static class IconBitmapConverter
{
    private const int DiNormal = 0x0003;
    private const int BiRgb = 0;

    public static Bitmap? fromHIcon(IntPtr hIcon, int size)
    {
        int width = Math.Max(1, size);
        int height = Math.Max(1, size);
        IntPtr screenDc = GetDC(IntPtr.Zero);
        if (screenDc == IntPtr.Zero)
            return null;

        IntPtr memDc = CreateCompatibleDC(screenDc);
        if (memDc == IntPtr.Zero)
        {
            ReleaseDC(IntPtr.Zero, screenDc);
            return null;
        }

        BITMAPINFOHEADER info = new BITMAPINFOHEADER
        {
            biSize = (uint)Marshal.SizeOf<BITMAPINFOHEADER>(),
            biWidth = width,
            biHeight = -height,
            biPlanes = 1,
            biBitCount = 32,
            biCompression = BiRgb,
        };
        IntPtr dib = CreateDIBSection(screenDc, ref info, BiRgb, out IntPtr bitsPtr, IntPtr.Zero, 0);
        if (dib == IntPtr.Zero || bitsPtr == IntPtr.Zero)
        {
            DeleteDC(memDc);
            ReleaseDC(IntPtr.Zero, screenDc);
            return null;
        }

        IntPtr old = SelectObject(memDc, dib);
        try
        {
            DrawIconEx(memDc, 0, 0, hIcon, width, height, 0, IntPtr.Zero, DiNormal);

            WriteableBitmap result = new WriteableBitmap(
                new PixelSize(width, height),
                new Vector(96, 96),
                PixelFormat.Bgra8888,
                AlphaFormat.Unpremul
            );
            using (ILockedFramebuffer frame = result.Lock())
            {
                int bufferSize = frame.RowBytes * height;
                byte[] pixels = new byte[bufferSize];
                Marshal.Copy(bitsPtr, pixels, 0, Math.Min(bufferSize, width * height * 4));
                Marshal.Copy(pixels, 0, frame.Address, bufferSize);
            }
            return result;
        }
        finally
        {
            SelectObject(memDc, old);
            DeleteObject(dib);
            DeleteDC(memDc);
            ReleaseDC(IntPtr.Zero, screenDc);
        }
    }

    [DllImport("user32.dll")]
    private static extern IntPtr GetDC(IntPtr hwnd);

    [DllImport("user32.dll")]
    private static extern int ReleaseDC(IntPtr hwnd, IntPtr hdc);

    [DllImport("gdi32.dll")]
    private static extern IntPtr CreateCompatibleDC(IntPtr hdc);

    [DllImport("gdi32.dll")]
    private static extern IntPtr SelectObject(IntPtr hdc, IntPtr hgdiobj);

    [DllImport("gdi32.dll")]
    private static extern bool DeleteObject(IntPtr hObject);

    [DllImport("gdi32.dll")]
    private static extern bool DeleteDC(IntPtr hdc);

    [DllImport("gdi32.dll")]
    private static extern IntPtr CreateDIBSection(
        IntPtr hdc,
        ref BITMAPINFOHEADER pbmi,
        uint usage,
        out IntPtr ppvBits,
        IntPtr hSection,
        uint offset
    );

    [DllImport("user32.dll")]
    private static extern bool DrawIconEx(
        IntPtr hdc,
        int xLeft,
        int yTop,
        IntPtr hIcon,
        int cxWidth,
        int cyHeight,
        uint istepIfAniCur,
        IntPtr hbrFlickerFreeDraw,
        uint diFlags
    );

    [StructLayout(LayoutKind.Sequential)]
    private struct BITMAPINFOHEADER
    {
        public uint biSize;
        public int biWidth;
        public int biHeight;
        public ushort biPlanes;
        public ushort biBitCount;
        public uint biCompression;
        public uint biSizeImage;
        public int biXPelsPerMeter;
        public int biYPelsPerMeter;
        public uint biClrUsed;
        public uint biClrImportant;
    }
}
