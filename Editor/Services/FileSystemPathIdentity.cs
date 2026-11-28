using Microsoft.Win32.SafeHandles;
using System;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace Ludork.Services;

internal static class FileSystemPathIdentity
{
    internal static StringComparer GetProjectComparer(string projectPath)
    {
        if (OperatingSystem.IsWindows())
            return StringComparer.OrdinalIgnoreCase;
        string directory = Directory.Exists(Path.Combine(projectPath, "Assets")) ? "Assets" : "Data";
        return RefersToSameEntry(Path.Combine(projectPath, directory), Path.Combine(projectPath, directory.ToLowerInvariant()))
            ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
    }

    internal static bool RefersToSameEntry(string source, string destination)
    {
        source = Path.GetFullPath(source);
        destination = Path.GetFullPath(destination);
        if (source == destination)
            return true;
        if (!string.Equals(source, destination, StringComparison.OrdinalIgnoreCase)
            || !Path.Exists(source) || !Path.Exists(destination))
            return false;
        return actualPath(source) == actualPath(destination);
    }

    private static string actualPath(string path)
    {
        string current = Path.GetPathRoot(path)!;
        foreach (string part in path[current.Length..].Split(Path.DirectorySeparatorChar,
            StringSplitOptions.RemoveEmptyEntries))
        {
            string[] matches = Directory.EnumerateFileSystemEntries(current)
                .Where(entry => string.Equals(Path.GetFileName(entry), part, StringComparison.OrdinalIgnoreCase)).ToArray();
            current = matches.FirstOrDefault(entry => Path.GetFileName(entry) == part)
                ?? (matches.Length == 1 ? matches[0] : Path.Combine(current, part));
        }
        return current;
    }

    internal static string GetKey(string path)
    {
        string fullPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
        if (OperatingSystem.IsMacOS())
        {
            IntPtr resolved = realpath(fullPath, IntPtr.Zero);
            if (resolved == IntPtr.Zero)
                return fullPath;
            try
            {
                return Marshal.PtrToStringUTF8(resolved)!;
            }
            finally
            {
                free(resolved);
            }
        }
        if (!OperatingSystem.IsWindows())
            return fullPath;
        const uint openExisting = 3;
        const uint backupSemantics = 0x02000000;
        using SafeFileHandle handle = CreateFileW(fullPath, 0, FileShare.ReadWrite | FileShare.Delete,
            IntPtr.Zero, openExisting, backupSemantics, IntPtr.Zero);
        if (handle.IsInvalid)
            return fullPath.ToUpperInvariant();
        StringBuilder buffer = new(512);
        uint length = GetFinalPathNameByHandleW(handle, buffer, (uint)buffer.Capacity, 0);
        if (length >= buffer.Capacity)
        {
            buffer.EnsureCapacity(checked((int)length + 1));
            length = GetFinalPathNameByHandleW(handle, buffer, (uint)buffer.Capacity, 0);
        }
        return length > 0 && length < buffer.Capacity ? buffer.ToString() : fullPath.ToUpperInvariant();
    }

    [DllImport("/usr/lib/libSystem.B.dylib", EntryPoint = "realpath$DARWIN_EXTSN", ExactSpelling = true)]
    private static extern IntPtr realpath([MarshalAs(UnmanagedType.LPUTF8Str)] string path, IntPtr result);

    [DllImport("/usr/lib/libSystem.B.dylib", ExactSpelling = true)]
    private static extern void free(IntPtr pointer);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, ExactSpelling = true)]
    private static extern SafeFileHandle CreateFileW(string file, uint access, FileShare share,
        IntPtr security, uint creation, uint flags, IntPtr template);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, ExactSpelling = true)]
    private static extern uint GetFinalPathNameByHandleW(SafeFileHandle handle, StringBuilder path, uint size, uint flags);
}
