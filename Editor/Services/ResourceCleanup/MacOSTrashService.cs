using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ludork.Services.ResourceCleanup;

[SupportedOSPlatform("macos")]
internal sealed class MacOSTrashService : ISystemTrash
{
    private static readonly IntPtr Foundation = NativeLibrary.Load("/System/Library/Frameworks/Foundation.framework/Foundation");

    public string? MoveToTrash(string path)
    {
        GC.KeepAlive(Foundation);
        IntPtr pool = send(objc_getClass("NSAutoreleasePool"), sel_registerName("new"));
        try
        {
            IntPtr text = stringWithUtf8(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), path);
            IntPtr url = sendArgument(objc_getClass("NSURL"), sel_registerName("fileURLWithPath:"), text);
            IntPtr manager = send(objc_getClass("NSFileManager"), sel_registerName("defaultManager"));
            bool succeeded = trash(manager, sel_registerName("trashItemAtURL:resultingItemURL:error:"), url,
                out IntPtr destination, out IntPtr error);
            if (!succeeded)
            {
                string detail = error == IntPtr.Zero ? "The system could not move the file to Trash."
                    : readString(send(error, sel_registerName("localizedDescription")));
                throw new IOException(detail);
            }
            return destination == IntPtr.Zero ? null : readString(send(destination, sel_registerName("path")));
        }
        finally
        {
            send(pool, sel_registerName("drain"));
        }
    }

    private static string readString(IntPtr value)
    {
        return Marshal.PtrToStringUTF8(send(value, sel_registerName("UTF8String"))) ?? string.Empty;
    }

    [DllImport("/usr/lib/libobjc.A.dylib")]
    private static extern IntPtr objc_getClass(string name);

    [DllImport("/usr/lib/libobjc.A.dylib")]
    private static extern IntPtr sel_registerName(string name);

    [DllImport("/usr/lib/libobjc.A.dylib", EntryPoint = "objc_msgSend")]
    private static extern IntPtr send(IntPtr receiver, IntPtr selector);

    [DllImport("/usr/lib/libobjc.A.dylib", EntryPoint = "objc_msgSend")]
    private static extern IntPtr sendArgument(IntPtr receiver, IntPtr selector, IntPtr argument);

    [DllImport("/usr/lib/libobjc.A.dylib", EntryPoint = "objc_msgSend")]
    private static extern IntPtr stringWithUtf8(IntPtr receiver, IntPtr selector, [MarshalAs(UnmanagedType.LPUTF8Str)] string value);

    [DllImport("/usr/lib/libobjc.A.dylib", EntryPoint = "objc_msgSend")]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool trash(IntPtr receiver, IntPtr selector, IntPtr url, out IntPtr result, out IntPtr error);
}
