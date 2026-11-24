using Microsoft.Win32.SafeHandles;
using System;

namespace Ludork.Services;

internal readonly partial record struct FileContentVersion(ulong Volume, ulong FileIdLow, ulong FileIdHigh, ulong Generation)
{
    internal static FileContentVersion? Read(SafeFileHandle handle)
    {
        if (OperatingSystem.IsWindows())
            return readWindows(handle);
        if (OperatingSystem.IsMacOS())
            return readMacOS(handle);
        return null;
    }
}
