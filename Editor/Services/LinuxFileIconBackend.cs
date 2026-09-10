using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ludork.Services;

[SupportedOSPlatform("linux")]
internal sealed class LinuxFileIconBackend : IFileIconBackend
{
    private const int GtkIconLookupUseMime = 1 << 1;

    public IImage? GetIcon(string path, bool isDirectory, int size)
    {
        if (!tryLoadGtk(out GtkApi gtk) || !tryLoadGdkPixbuf(out GdkPixbufApi gdk))
            return loadFromThemeCache(path, isDirectory, size);

        gtk_init_check_delegate? gtkInitCheck = gtk.gtk_init_check;
        gtk_icon_theme_get_default_delegate? getDefaultTheme = gtk.gtk_icon_theme_get_default;
        gtk_icon_theme_lookup_icon_delegate? lookupIcon = gtk.gtk_icon_theme_lookup_icon;
        gtk_icon_info_load_icon_delegate? loadIcon = gtk.gtk_icon_info_load_icon;
        g_object_unref_delegate? objectUnref = gtk.g_object_unref;
        gdk_pixbuf_get_width_delegate? getWidth = gdk.gdk_pixbuf_get_width;
        gdk_pixbuf_get_height_delegate? getHeight = gdk.gdk_pixbuf_get_height;
        gdk_pixbuf_get_rowstride_delegate? getRowstride = gdk.gdk_pixbuf_get_rowstride;
        gdk_pixbuf_get_pixels_delegate? getPixels = gdk.gdk_pixbuf_get_pixels;
        if (gtkInitCheck is null || getDefaultTheme is null || lookupIcon is null || loadIcon is null || objectUnref is null
            || getWidth is null || getHeight is null || getRowstride is null || getPixels is null)
            return loadFromThemeCache(path, isDirectory, size);

        try
        {
            if (gtkInitCheck(IntPtr.Zero, IntPtr.Zero) == 0)
                return loadFromThemeCache(path, isDirectory, size);

            IntPtr theme = getDefaultTheme();
            if (theme == IntPtr.Zero)
                return loadFromThemeCache(path, isDirectory, size);

            string iconName = isDirectory ? "folder" : guessIconName(path);
            IntPtr info = lookupIcon(theme, iconName, size, GtkIconLookupUseMime);
            if (info == IntPtr.Zero)
                return loadFromThemeCache(path, isDirectory, size);

            try
            {
                IntPtr pixbuf = loadIcon(info, IntPtr.Zero);
                if (pixbuf == IntPtr.Zero)
                    return null;
                try
                {
                    int width = getWidth(pixbuf);
                    int height = getHeight(pixbuf);
                    int rowstride = getRowstride(pixbuf);
                    IntPtr pixels = getPixels(pixbuf);
                    if (width <= 0 || height <= 0 || pixels == IntPtr.Zero)
                        return null;

                    WriteableBitmap result = new WriteableBitmap(
                        new Avalonia.PixelSize(width, height),
                        new Avalonia.Vector(96, 96),
                        Avalonia.Platform.PixelFormat.Rgba8888,
                        Avalonia.Platform.AlphaFormat.Opaque
                    );
                    using ILockedFramebuffer frame = result.Lock();
                    byte[] source = new byte[rowstride * height];
                    Marshal.Copy(pixels, source, 0, source.Length);
                    byte[] target = new byte[frame.RowBytes * height];
                    for (int y = 0; y < height; y++)
                    {
                        for (int x = 0; x < width; x++)
                        {
                            int sourceIndex = y * rowstride + x * 4;
                            int targetIndex = y * frame.RowBytes + x * 4;
                            target[targetIndex] = source[sourceIndex + 2];
                            target[targetIndex + 1] = source[sourceIndex + 1];
                            target[targetIndex + 2] = source[sourceIndex];
                            target[targetIndex + 3] = source[sourceIndex + 3];
                        }
                    }
                    Marshal.Copy(target, 0, frame.Address, target.Length);
                    return result;
                }
                finally
                {
                    objectUnref(pixbuf);
                }
            }
            finally
            {
                objectUnref(info);
            }
        }
        finally
        {
            NativeLibrary.Free(gtk.Handle);
            if (gdk.Handle != IntPtr.Zero)
                NativeLibrary.Free(gdk.Handle);
        }
    }

    private static Bitmap? loadFromThemeCache(string path, bool isDirectory, int size)
    {
        string iconName = isDirectory ? "folder" : guessIconName(path);
        foreach (string root in new[] { "/usr/share/icons/hicolor", "/usr/share/pixmaps" })
        {
            foreach (string sizeDir in new[] { $"{size}x{size}", "48x48", "32x32", "22x22" })
            {
                foreach (string sub in new[] { "mimetypes", "places", "apps", string.Empty })
                {
                    string candidate = string.IsNullOrEmpty(sub)
                        ? Path.Combine(root, sizeDir, $"{iconName}.png")
                        : Path.Combine(root, sizeDir, sub, $"{iconName}.png");
                    if (!File.Exists(candidate))
                        continue;
                    using FileStream stream = File.OpenRead(candidate);
                    return Bitmap.DecodeToWidth(stream, size);
                }
            }
        }
        return null;
    }

    private static string guessIconName(string path)
    {
        string extension = Path.GetExtension(path).ToLowerInvariant();
        return extension switch
        {
            ".png" or ".jpg" or ".jpeg" or ".bmp" or ".gif" or ".webp" => "image-x-generic",
            ".json" => "text-x-script",
            ".cs" => "text-x-csharp",
            ".txt" => "text-plain",
            _ => "text-x-generic",
        };
    }

    private static bool tryLoadGtk(out GtkApi gtk)
    {
        gtk = default;
        if (!NativeLibrary.TryLoad("libgtk-3.so.0", out IntPtr handle)
            && !NativeLibrary.TryLoad("libgtk-3.so", out handle))
            return false;

        gtk = new GtkApi
        {
            Handle = handle,
            gtk_init_check = getDelegate<gtk_init_check_delegate>(handle, "gtk_init_check"),
            gtk_icon_theme_get_default = getDelegate<gtk_icon_theme_get_default_delegate>(handle, "gtk_icon_theme_get_default"),
            gtk_icon_theme_lookup_icon = getDelegate<gtk_icon_theme_lookup_icon_delegate>(handle, "gtk_icon_theme_lookup_icon"),
            gtk_icon_info_load_icon = getDelegate<gtk_icon_info_load_icon_delegate>(handle, "gtk_icon_info_load_icon"),
            g_object_unref = getDelegate<g_object_unref_delegate>(handle, "g_object_unref"),
        };
        return gtk.gtk_init_check is not null
            && gtk.gtk_icon_theme_get_default is not null
            && gtk.gtk_icon_theme_lookup_icon is not null
            && gtk.gtk_icon_info_load_icon is not null
            && gtk.g_object_unref is not null;
    }

    private static bool tryLoadGdkPixbuf(out GdkPixbufApi gdk)
    {
        gdk = default;
        if (!NativeLibrary.TryLoad("libgdk_pixbuf-2.0.so.0", out IntPtr handle)
            && !NativeLibrary.TryLoad("libgdk_pixbuf-2.0.so", out handle))
            return false;

        gdk = new GdkPixbufApi
        {
            Handle = handle,
            gdk_pixbuf_get_width = getDelegate<gdk_pixbuf_get_width_delegate>(handle, "gdk_pixbuf_get_width"),
            gdk_pixbuf_get_height = getDelegate<gdk_pixbuf_get_height_delegate>(handle, "gdk_pixbuf_get_height"),
            gdk_pixbuf_get_rowstride = getDelegate<gdk_pixbuf_get_rowstride_delegate>(handle, "gdk_pixbuf_get_rowstride"),
            gdk_pixbuf_get_pixels = getDelegate<gdk_pixbuf_get_pixels_delegate>(handle, "gdk_pixbuf_get_pixels"),
        };
        return gdk.gdk_pixbuf_get_width is not null
            && gdk.gdk_pixbuf_get_height is not null
            && gdk.gdk_pixbuf_get_rowstride is not null
            && gdk.gdk_pixbuf_get_pixels is not null;
    }

    private static T getDelegate<T>(IntPtr handle, string name) where T : Delegate
    {
        return NativeLibrary.TryGetExport(handle, name, out IntPtr ptr)
            ? Marshal.GetDelegateForFunctionPointer<T>(ptr)
            : default!;
    }

    private struct GdkPixbufApi
    {
        public IntPtr Handle;
        public gdk_pixbuf_get_width_delegate? gdk_pixbuf_get_width;
        public gdk_pixbuf_get_height_delegate? gdk_pixbuf_get_height;
        public gdk_pixbuf_get_rowstride_delegate? gdk_pixbuf_get_rowstride;
        public gdk_pixbuf_get_pixels_delegate? gdk_pixbuf_get_pixels;
    }

    private struct GtkApi
    {
        public IntPtr Handle;
        public gtk_init_check_delegate? gtk_init_check;
        public gtk_icon_theme_get_default_delegate? gtk_icon_theme_get_default;
        public gtk_icon_theme_lookup_icon_delegate? gtk_icon_theme_lookup_icon;
        public gtk_icon_info_load_icon_delegate? gtk_icon_info_load_icon;
        public g_object_unref_delegate? g_object_unref;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int gtk_init_check_delegate(IntPtr argc, IntPtr argv);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate IntPtr gtk_icon_theme_get_default_delegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate IntPtr gtk_icon_theme_lookup_icon_delegate(IntPtr theme, string iconName, int size, int flags);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate IntPtr gtk_icon_info_load_icon_delegate(IntPtr info, IntPtr error);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int gdk_pixbuf_get_width_delegate(IntPtr pixbuf);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int gdk_pixbuf_get_height_delegate(IntPtr pixbuf);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int gdk_pixbuf_get_rowstride_delegate(IntPtr pixbuf);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate IntPtr gdk_pixbuf_get_pixels_delegate(IntPtr pixbuf);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void g_object_unref_delegate(IntPtr obj);
}
