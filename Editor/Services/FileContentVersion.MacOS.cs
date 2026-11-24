using Microsoft.Win32.SafeHandles;
using System;
using System.Runtime.InteropServices;

namespace Ludork.Services;

internal readonly partial record struct FileContentVersion
{
    private const uint DeviceIdAttribute = 0x00000002;
    private const uint GenerationCountAttribute = 0x00080000;
    private const uint FileIdAttribute = 0x02000000;
    private const uint ReturnedAttributes = 0x80000000;
    private const uint VersionAttributes = ReturnedAttributes | DeviceIdAttribute | GenerationCountAttribute | FileIdAttribute;
    private const nuint ExtendedCommonAttributes = 0x20;

    private static FileContentVersion? readMacOS(SafeFileHandle handle)
    {
        MacAttributeList request = new() { BitmapCount = 5, Common = VersionAttributes };
        int size = Marshal.SizeOf<MacFileAttributes>();
        int result = fgetattrlist(handle.DangerousGetHandle().ToInt32(), in request, out MacFileAttributes attributes,
            (nuint)size, ExtendedCommonAttributes);
        GC.KeepAlive(handle);
        if (result != 0 || attributes.Length != size || attributes.Common != VersionAttributes || attributes.Generation == 0)
            return null;
        return new FileContentVersion(attributes.Device, attributes.FileId, 0, attributes.Generation);
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct MacAttributeList
    {
        public ushort BitmapCount;
        public ushort Reserved;
        public uint Common;
        public uint Volume;
        public uint Directory;
        public uint File;
        public uint Fork;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    private struct MacFileAttributes
    {
        public uint Length;
        public uint Common;
        public uint VolumeAttributes;
        public uint DirectoryAttributes;
        public uint FileAttributes;
        public uint ForkAttributes;
        public uint Device;
        public uint Generation;
        public ulong FileId;
    }

    [DllImport("/usr/lib/libSystem.B.dylib", ExactSpelling = true)]
    private static extern int fgetattrlist(int fd, in MacAttributeList request, out MacFileAttributes attributes,
        nuint size, nuint options);
}
