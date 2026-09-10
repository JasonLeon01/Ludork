using Avalonia.Media;
using Avalonia.Media.Imaging;
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ludork.Services;

[SupportedOSPlatform("macos")]
internal sealed class MacOSFileIconBackend : IFileIconBackend
{
    public IImage? GetIcon(string path, bool isDirectory, int size)
    {
        string fullPath = Path.GetFullPath(path);
        if (!isDirectory && !File.Exists(fullPath))
            fullPath = Path.Combine(Path.GetTempPath(), "dummy" + Path.GetExtension(path));

        IntPtr workspace = objc_msgSend(objc_getClass("NSWorkspace"), sel_registerName("sharedWorkspace"));
        if (workspace == IntPtr.Zero)
            return null;

        IntPtr nsPath = createNSString(fullPath);
        if (nsPath == IntPtr.Zero)
            return null;

        IntPtr image = objc_msgSend_intptr(workspace, sel_registerName("iconForFile:"), nsPath);
        if (image == IntPtr.Zero)
            return null;

        IntPtr tiffData = objc_msgSend(image, sel_registerName("TIFFRepresentation"));
        if (tiffData == IntPtr.Zero)
            return null;

        IntPtr bitmapRepresentation = objc_msgSend_intptr(
            objc_getClass("NSBitmapImageRep"),
            sel_registerName("imageRepWithData:"),
            tiffData
        );
        if (bitmapRepresentation == IntPtr.Zero)
            return null;

        IntPtr properties = objc_msgSend(objc_getClass("NSDictionary"), sel_registerName("dictionary"));
        IntPtr pngData = objc_msgSend_nuint_intptr(
            bitmapRepresentation,
            sel_registerName("representationUsingType:properties:"),
            4,
            properties
        );
        if (pngData == IntPtr.Zero)
            return null;

        int length = (int)objc_msgSend(pngData, sel_registerName("length"));
        IntPtr bytes = objc_msgSend(pngData, sel_registerName("bytes"));
        if (length <= 0 || bytes == IntPtr.Zero)
            return null;

        byte[] buffer = new byte[length];
        Marshal.Copy(bytes, buffer, 0, length);
        using MemoryStream stream = new MemoryStream(buffer);
        return Bitmap.DecodeToWidth(stream, size);
    }

    private static IntPtr createNSString(string value)
    {
        IntPtr utf8 = Marshal.StringToHGlobalAnsi(value);
        try
        {
            return objc_msgSend_intptr(
                objc_getClass("NSString"),
                sel_registerName("stringWithUTF8String:"),
                utf8
            );
        }
        finally
        {
            Marshal.FreeHGlobal(utf8);
        }
    }

    [DllImport("/usr/lib/libobjc.A.dylib")]
    private static extern IntPtr objc_getClass(string name);

    [DllImport("/usr/lib/libobjc.A.dylib")]
    private static extern IntPtr sel_registerName(string name);

    [DllImport("/usr/lib/libobjc.A.dylib", EntryPoint = "objc_msgSend")]
    private static extern IntPtr objc_msgSend(IntPtr receiver, IntPtr selector);

    [DllImport("/usr/lib/libobjc.A.dylib", EntryPoint = "objc_msgSend")]
    private static extern IntPtr objc_msgSend_intptr(IntPtr receiver, IntPtr selector, IntPtr arg);

    [DllImport("/usr/lib/libobjc.A.dylib", EntryPoint = "objc_msgSend")]
    private static extern IntPtr objc_msgSend_nuint_intptr(
        IntPtr receiver,
        IntPtr selector,
        nuint firstArg,
        IntPtr secondArg
    );
}
