using System;
using System.Runtime.InteropServices;

namespace Ludork.Services;

static class ShellAssociationService
{
    private const uint AssociationChanged = 0x08000000;
    private const uint FlushNoWait = 0x2000;

    public static void NotifyChanged()
    {
        if (!OperatingSystem.IsWindows())
            return;
        SHChangeNotify(AssociationChanged, FlushNoWait, IntPtr.Zero, IntPtr.Zero);
    }

    [DllImport("shell32.dll")]
    private static extern void SHChangeNotify(
        uint eventId,
        uint flags,
        IntPtr item1,
        IntPtr item2);
}
