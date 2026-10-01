using Avalonia.Media.Imaging;
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ludork.Services;

[SupportedOSPlatform("macos")]
internal static class MacOSVideoThumbnail
{
    private const string ObjectiveC = "/usr/lib/libobjc.A.dylib";
    private static readonly nint framework = NativeLibrary.Load("/System/Library/Frameworks/AVFoundation.framework/AVFoundation");

    public static Bitmap? Create(string path, int pixelSize)
    {
        _ = framework;
        nint pool = send(send(objc_getClass("NSAutoreleasePool"), selector("alloc")), selector("init"));
        nint generator = 0;
        nint image = 0;
        nint representation = 0;
        try
        {
            nint text = sendString(objc_getClass("NSString"), selector("stringWithUTF8String:"), Path.GetFullPath(path));
            nint url = sendObject(objc_getClass("NSURL"), selector("fileURLWithPath:"), text);
            nint asset = sendObject(objc_getClass("AVURLAsset"), selector("assetWithURL:"), url);
            generator = sendObject(send(objc_getClass("AVAssetImageGenerator"), selector("alloc")), selector("initWithAsset:"), asset);
            if (generator == 0)
                return null;
            sendBool(generator, selector("setAppliesPreferredTrackTransform:"), true);
            sendSize(generator, selector("setMaximumSize:"), new NativeSize { Width = pixelSize, Height = pixelSize });
            image = copyImage(generator, selector("copyCGImageAtTime:actualTime:error:"),
                new MediaTime { Timescale = 1, Flags = 1 }, 0, 0);
            if (image == 0)
                return null;
            representation = sendObject(send(objc_getClass("NSBitmapImageRep"), selector("alloc")), selector("initWithCGImage:"), image);
            nint properties = send(objc_getClass("NSDictionary"), selector("dictionary"));
            nint data = sendFormat(representation, selector("representationUsingType:properties:"), 4, properties);
            int length = checked((int)send(data, selector("length")));
            nint bytes = send(data, selector("bytes"));
            if (length <= 0 || bytes == 0)
                return null;
            byte[] buffer = new byte[length];
            Marshal.Copy(bytes, buffer, 0, length);
            using MemoryStream stream = new(buffer);
            return new Bitmap(stream);
        }
        finally
        {
            if (representation != 0)
                sendVoid(representation, selector("release"));
            if (image != 0)
                CGImageRelease(image);
            if (generator != 0)
                sendVoid(generator, selector("release"));
            sendVoid(pool, selector("drain"));
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct MediaTime
    {
        public long Value;
        public int Timescale;
        public uint Flags;
        public long Epoch;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeSize
    {
        public double Width;
        public double Height;
    }

    [DllImport(ObjectiveC)]
    private static extern nint objc_getClass(string name);
    [DllImport(ObjectiveC, EntryPoint = "sel_registerName")]
    private static extern nint selector(string name);
    [DllImport(ObjectiveC, EntryPoint = "objc_msgSend")]
    private static extern nint send(nint receiver, nint selector);
    [DllImport(ObjectiveC, EntryPoint = "objc_msgSend")]
    private static extern nint sendObject(nint receiver, nint selector, nint value);
    [DllImport(ObjectiveC, EntryPoint = "objc_msgSend")]
    private static extern nint sendString(nint receiver, nint selector, [MarshalAs(UnmanagedType.LPUTF8Str)] string value);
    [DllImport(ObjectiveC, EntryPoint = "objc_msgSend")]
    private static extern void sendBool(nint receiver, nint selector, [MarshalAs(UnmanagedType.I1)] bool value);
    [DllImport(ObjectiveC, EntryPoint = "objc_msgSend")]
    private static extern void sendSize(nint receiver, nint selector, NativeSize value);
    [DllImport(ObjectiveC, EntryPoint = "objc_msgSend")]
    private static extern nint copyImage(nint receiver, nint selector, MediaTime time, nint actualTime, nint error);
    [DllImport(ObjectiveC, EntryPoint = "objc_msgSend")]
    private static extern nint sendFormat(nint receiver, nint selector, nuint format, nint properties);
    [DllImport(ObjectiveC, EntryPoint = "objc_msgSend")]
    private static extern void sendVoid(nint receiver, nint selector);
    [DllImport("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")]
    private static extern void CGImageRelease(nint image);
}
