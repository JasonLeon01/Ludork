using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using System;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ludork.Services;

[SupportedOSPlatform("windows")]
internal static class WindowsVideoThumbnail
{
    private const int RpcEChangedMode = unchecked((int)0x80010106);
    private const uint ThumbnailOnly = 0x8;

    public static Bitmap? Create(string path, int pixelSize)
    {
        int initialization = CoInitializeEx(IntPtr.Zero, 0);
        if (initialization < 0 && initialization != RpcEChangedMode)
            return null;

        IShellItemImageFactory? factory = null;
        IntPtr bitmap = IntPtr.Zero;
        try
        {
            Guid interfaceId = typeof(IShellItemImageFactory).GUID;
            if (SHCreateItemFromParsingName(path, IntPtr.Zero, ref interfaceId, out factory) < 0 || factory == null)
                return null;

            int size = Math.Clamp(pixelSize, 1, 4096);
            SIZE requestedSize = new SIZE { Width = size, Height = size };
            if (factory.GetImage(requestedSize, ThumbnailOnly, out bitmap) < 0 || bitmap == IntPtr.Zero)
                return null;

            return ConvertBitmap(bitmap);
        }
        finally
        {
            if (bitmap != IntPtr.Zero)
                DeleteObject(bitmap);
            if (factory != null)
                Marshal.ReleaseComObject(factory);
            if (initialization >= 0)
                CoUninitialize();
        }
    }

    private static Bitmap? ConvertBitmap(IntPtr bitmap)
    {
        IntPtr deviceContext = CreateCompatibleDC(IntPtr.Zero);
        if (deviceContext == IntPtr.Zero)
            return null;

        try
        {
            BITMAPINFOHEADER info = new BITMAPINFOHEADER
            {
                Size = (uint)Marshal.SizeOf<BITMAPINFOHEADER>()
            };
            if (GetDIBits(deviceContext, bitmap, 0, 0, null, ref info, 0) == 0)
                return null;

            int width = info.Width;
            if (width <= 0 || width > 4096 || info.Height == 0 || info.Height < -4096 || info.Height > 4096)
                return null;

            int height = Math.Abs(info.Height);
            int stride = width * 4;
            byte[] pixels = new byte[stride * height];
            info.Height = -height;
            info.Planes = 1;
            info.BitCount = 32;
            info.Compression = 0;
            info.SizeImage = (uint)pixels.Length;
            info.ColorsUsed = 0;
            info.ColorsImportant = 0;
            if (GetDIBits(deviceContext, bitmap, 0, (uint)height, pixels, ref info, 0) != height)
                return null;

            bool hasAlpha = false;
            for (int index = 3; index < pixels.Length; index += 4)
            {
                if (pixels[index] == 0)
                    continue;
                hasAlpha = true;
                break;
            }

            GCHandle pinnedPixels = GCHandle.Alloc(pixels, GCHandleType.Pinned);
            try
            {
                return new Bitmap(
                    PixelFormat.Bgra8888,
                    hasAlpha ? AlphaFormat.Premul : AlphaFormat.Opaque,
                    pinnedPixels.AddrOfPinnedObject(),
                    new PixelSize(width, height),
                    new Vector(96, 96),
                    stride
                );
            }
            finally
            {
                pinnedPixels.Free();
            }
        }
        finally
        {
            DeleteDC(deviceContext);
        }
    }

    [ComImport]
    [Guid("BCC18B79-BA16-442F-80C4-8A59C30C463B")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IShellItemImageFactory
    {
        [PreserveSig]
        int GetImage(SIZE size, uint flags, out IntPtr bitmap);
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct SIZE
    {
        public int Width;
        public int Height;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct BITMAPINFOHEADER
    {
        public uint Size;
        public int Width;
        public int Height;
        public ushort Planes;
        public ushort BitCount;
        public uint Compression;
        public uint SizeImage;
        public int XPelsPerMeter;
        public int YPelsPerMeter;
        public uint ColorsUsed;
        public uint ColorsImportant;
    }

    [DllImport("ole32.dll", ExactSpelling = true)]
    private static extern int CoInitializeEx(IntPtr reserved, uint concurrencyModel);

    [DllImport("ole32.dll", ExactSpelling = true)]
    private static extern void CoUninitialize();

    [DllImport("shell32.dll", CharSet = CharSet.Unicode, ExactSpelling = true)]
    private static extern int SHCreateItemFromParsingName(
        string path,
        IntPtr bindContext,
        ref Guid interfaceId,
        [MarshalAs(UnmanagedType.Interface)] out IShellItemImageFactory? factory
    );

    [DllImport("gdi32.dll", ExactSpelling = true)]
    private static extern IntPtr CreateCompatibleDC(IntPtr deviceContext);

    [DllImport("gdi32.dll", ExactSpelling = true)]
    private static extern bool DeleteDC(IntPtr deviceContext);

    [DllImport("gdi32.dll", ExactSpelling = true)]
    private static extern bool DeleteObject(IntPtr bitmap);

    [DllImport("gdi32.dll", ExactSpelling = true)]
    private static extern int GetDIBits(
        IntPtr deviceContext,
        IntPtr bitmap,
        uint startScanLine,
        uint scanLineCount,
        [Out] byte[]? pixels,
        ref BITMAPINFOHEADER info,
        uint usage
    );
}
