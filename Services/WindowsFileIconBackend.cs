using Avalonia.Media;
using Avalonia.Media.Imaging;
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ludork.Services;

[SupportedOSPlatform("windows")]
internal sealed class WindowsFileIconBackend : IFileIconBackend
{
    private const uint FileAttributeDirectory = 0x10;
    private const uint FileAttributeNormal = 0x80;
    private const uint ShgfiIcon = 0x100;
    private const uint ShgfiUseFileAttributes = 0x10;

    public IImage? GetIcon(string path, bool isDirectory, int size)
    {
        SHFILEINFO shfi = new SHFILEINFO();
        uint flags = ShgfiIcon | (size > 32 ? 0u : 1u);
        if (File.Exists(path) || Directory.Exists(path))
        {
            _ = SHGetFileInfo(path, 0, ref shfi, (uint)Marshal.SizeOf<SHFILEINFO>(), flags);
        }
        else
        {
            uint attributes = isDirectory ? FileAttributeDirectory : FileAttributeNormal;
            string queryPath = isDirectory ? path : "dummy" + Path.GetExtension(path);
            flags |= ShgfiUseFileAttributes;
            _ = SHGetFileInfo(queryPath, attributes, ref shfi, (uint)Marshal.SizeOf<SHFILEINFO>(), flags);
        }

        if (shfi.hIcon == IntPtr.Zero)
            return null;

        try
        {
            return IconBitmapConverter.fromHIcon(shfi.hIcon, size);
        }
        finally
        {
            DestroyIcon(shfi.hIcon);
        }
    }

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr SHGetFileInfo(
        string pszPath,
        uint dwFileAttributes,
        ref SHFILEINFO psfi,
        uint cbFileInfo,
        uint uFlags
    );

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool DestroyIcon(IntPtr hIcon);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct SHFILEINFO
    {
        public IntPtr hIcon;
        public int iIcon;
        public uint dwAttributes;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string szDisplayName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 80)]
        public string szTypeName;
    }
}
