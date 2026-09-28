using System;
using System.IO;
using System.Security;

namespace Ludork.Services;

internal static class EditorPathSandbox
{
    private static StringComparison PathComparison => OperatingSystem.IsWindows()
        ? StringComparison.OrdinalIgnoreCase
        : StringComparison.Ordinal;

    public static bool IsSameOrChildPath(string root, string path)
    {
        string relative = Path.GetRelativePath(Path.GetFullPath(root), Path.GetFullPath(path));
        return !Path.IsPathRooted(relative)
            && relative != ".."
            && !relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal);
    }

    public static bool IsLink(string path)
    {
        FileSystemInfo entry = Directory.Exists(path) ? new DirectoryInfo(path) : new FileInfo(path);
        return IsLink(entry);
    }

    public static bool IsLink(FileSystemInfo entry)
    {
        return entry.LinkTarget is not null
            || entry.Exists && (entry.Attributes & FileAttributes.ReparsePoint) != 0;
    }

    public static bool TryResolve(
        string root,
        string path,
        out string fullPath,
        bool allowMissing = false)
    {
        fullPath = string.Empty;
        if (string.IsNullOrWhiteSpace(root) || string.IsNullOrWhiteSpace(path)
            || root.Contains('\0') || path.Contains('\0')
            || Path.IsPathRooted(path) && !Path.IsPathFullyQualified(path))
        {
            return false;
        }
        try
        {
            string fullRoot = Path.TrimEndingDirectorySeparator(Path.GetFullPath(root));
            string candidate = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path, fullRoot));
            if (!IsSameOrChildPath(fullRoot, candidate))
            {
                return false;
            }
            string current = candidate;
            while (true)
            {
                bool atRoot = current.Equals(fullRoot, PathComparison);
                FileAttributes? attributes = getAttributes(current, allowMissing && !atRoot);
                if (IsLink(current)
                    || attributes is FileAttributes existing
                        && (atRoot || !current.Equals(candidate, PathComparison))
                        && (existing & FileAttributes.Directory) == 0)
                {
                    return false;
                }
                if (atRoot)
                    break;
                string? parent = Path.GetDirectoryName(current);
                if (parent is null || parent.Equals(current, PathComparison))
                    return false;
                current = parent;
            }
            fullPath = candidate;
            return true;
        }
        catch (Exception exception) when (IsPathFailure(exception))
        {
            return false;
        }
    }

    public static bool IsPathFailure(Exception exception)
    {
        return exception is IOException or UnauthorizedAccessException or ArgumentException
            or NotSupportedException or SecurityException;
    }

    private static FileAttributes? getAttributes(string path, bool allowMissing)
    {
        try
        {
            return File.GetAttributes(path);
        }
        catch (FileNotFoundException) when (allowMissing)
        {
            return null;
        }
        catch (DirectoryNotFoundException) when (allowMissing)
        {
            return null;
        }
    }
}
