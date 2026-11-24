using Microsoft.Win32.SafeHandles;
using System;
using System.Runtime.InteropServices;

namespace Ludork.Services;

internal readonly partial record struct FileContentVersion
{
    private const uint ReadFileUsnData = 0x000900eb;
    private const int FileIdInfo = 18;

    private static FileContentVersion? readWindows(SafeFileHandle handle)
    {
        if (!DeviceIoControl(handle, ReadFileUsnData, IntPtr.Zero, 0, out UsnRecordV2 record,
                Marshal.SizeOf<UsnRecordV2>(), out int size, IntPtr.Zero)
            || size < 60 || record.MajorVersion != 2 || record.Usn == 0
            || !GetFileInformationByHandleEx(handle, FileIdInfo, out WindowsFileId id, Marshal.SizeOf<WindowsFileId>()))
            return null;
        return new FileContentVersion(id.VolumeSerialNumber, id.Low, id.High, record.Usn);
    }

    [StructLayout(LayoutKind.Explicit, Size = 1024)]
    private struct UsnRecordV2
    {
        [FieldOffset(4)] public ushort MajorVersion;
        [FieldOffset(24)] public ulong Usn;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct WindowsFileId
    {
        public ulong VolumeSerialNumber;
        public ulong Low;
        public ulong High;
    }

    [DllImport("kernel32.dll", ExactSpelling = true)]
    private static extern bool DeviceIoControl(SafeFileHandle device, uint controlCode, IntPtr input, int inputSize,
        out UsnRecordV2 output, int outputSize, out int bytesReturned, IntPtr overlapped);

    [DllImport("kernel32.dll", ExactSpelling = true)]
    private static extern bool GetFileInformationByHandleEx(SafeFileHandle handle, int infoClass, out WindowsFileId id,
        int bufferSize);
}
