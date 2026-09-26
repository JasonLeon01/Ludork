using System;
using System.IO;
using System.Text;

namespace Ludork.Services;

internal static class HarmonySigningInput
{
    public static bool HasLineBreak(string value) =>
        value.IndexOfAny(['\r', '\n']) >= 0;

    public static bool IsNonEmptySingleLine(string value) =>
        value.Length != 0 && !HasLineBreak(value);

    public static bool IsReadableFile(string path)
    {
        if (!Path.IsPathFullyQualified(path)
            || HasLineBreak(path)
            || !File.Exists(path))
        {
            return false;
        }
        try
        {
            using FileStream stream = File.Open(
                path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            return stream.CanRead;
        }
        catch (UnauthorizedAccessException)
        {
            return false;
        }
        catch (IOException)
        {
            return false;
        }
    }

    public static bool IsOutsideProject(string path, string? projectPath)
    {
        if (string.IsNullOrEmpty(projectPath))
            return true;
        try
        {
            int remainingLinks = 64;
            string file = resolvePath(path, ref remainingLinks);
            string project = Path.TrimEndingDirectorySeparator(resolvePath(projectPath, ref remainingLinks));
            StringComparison comparison = OperatingSystem.IsWindows() || OperatingSystem.IsMacOS()
                ? StringComparison.OrdinalIgnoreCase
                : StringComparison.Ordinal;
            return !file.Equals(project, comparison)
                && !file.StartsWith(project + Path.DirectorySeparatorChar, comparison);
        }
        catch (Exception exception) when (
            exception is IOException or UnauthorizedAccessException or ArgumentException or NotSupportedException)
        {
            return false;
        }
    }

    private static string resolvePath(string path, ref int remainingLinks)
    {
        string fullPath = Path.GetFullPath(path);
        string root = Path.GetPathRoot(fullPath)!;
        string resolved = root;
        foreach (string component in fullPath[root.Length..].Split(
            [Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar],
            StringSplitOptions.RemoveEmptyEntries))
        {
            resolved = Path.Combine(resolved, component);
            FileSystemInfo entry = Directory.Exists(resolved)
                ? new DirectoryInfo(resolved)
                : new FileInfo(resolved);
            if (entry.LinkTarget is null)
                continue;
            if (--remainingLinks < 0)
                throw new IOException("Too many symbolic links in a signing path.");
            FileSystemInfo target = entry.ResolveLinkTarget(true)
                ?? throw new IOException("A signing path link could not be resolved.");
            resolved = resolvePath(target.FullName, ref remainingLinks);
        }
        return resolved.Normalize(NormalizationForm.FormC);
    }
}
