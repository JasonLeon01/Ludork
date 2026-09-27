using System;
using System.IO;
using System.Security;

namespace Ludork.Services;

internal static class EditorPathSandbox
{
    private static StringComparison PathComparison => OperatingSystem.IsWindows()
        ? StringComparison.OrdinalIgnoreCase
        : StringComparison.Ordinal;

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
            string prefix = Path.EndsInDirectorySeparator(fullRoot)
                ? fullRoot
                : fullRoot + Path.DirectorySeparatorChar;
            if (!candidate.Equals(fullRoot, PathComparison)
                && !candidate.StartsWith(prefix, PathComparison))
            {
                return false;
            }
            string current = candidate;
            while (true)
            {
                bool atRoot = current.Equals(fullRoot, PathComparison);
                FileAttributes? attributes = getAttributes(current, allowMissing && !atRoot);
                if (attributes is FileAttributes existing
                    && ((existing & FileAttributes.ReparsePoint) != 0
                        || (atRoot || !current.Equals(candidate, PathComparison))
                        && (existing & FileAttributes.Directory) == 0))
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
