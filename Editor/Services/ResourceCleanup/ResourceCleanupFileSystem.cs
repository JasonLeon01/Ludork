using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;

namespace Ludork.Services;

internal static class ResourceCleanupFileSystem
{
    public const string KeepListPath = "EditorCache/ResourceCleanupKeep.list";

    public static string NormalizeKeepPath(string path)
    {
        string normalized = path.Trim().Replace('\\', '/').TrimEnd('/');
        if (normalized.StartsWith("/Game/Assets/", StringComparison.Ordinal))
            normalized = normalized["/Game/".Length..];
        if (normalized == "/Game/Assets")
            normalized = "Assets";
        if (normalized.Length == 0 || normalized.Any(char.IsControl) || Path.IsPathRooted(normalized)
            || normalized.Split('/').Any(part => part.Length == 0 || part is "." or ".."
                || part.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
            || normalized is not "Assets" and not "Data"
                && !normalized.StartsWith("Assets/", StringComparison.Ordinal)
                && !normalized.StartsWith("Data/", StringComparison.Ordinal))
        {
            throw new InvalidDataException($"Keep paths must be project-relative Data or Assets paths: {path}");
        }
        return normalized;
    }

    public static string ResolveSafePath(string projectPath, string relativePath)
    {
        string root = Path.TrimEndingDirectorySeparator(Path.GetFullPath(projectPath));
        string normalized = relativePath.Replace('\\', '/');
        if (Path.IsPathRooted(normalized) || normalized.Length == 0
            || normalized.Split('/').Any(part => part.Length == 0 || part is "." or ".."))
            throw new InvalidDataException($"The path is outside the project: {relativePath}");
        string absolute = Path.GetFullPath(Path.Combine(root, normalized));
        StringComparison comparison = OperatingSystem.IsWindows()
            ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
        if (!absolute.StartsWith(root + Path.DirectorySeparatorChar, comparison))
            throw new InvalidDataException($"The path is outside the project: {relativePath}");
        rejectLink(root);
        string current = root;
        foreach (string part in normalized.Split('/'))
        {
            current = Path.Combine(current, part);
            if (File.Exists(current) || Directory.Exists(current)
                || new FileInfo(current).LinkTarget is not null)
                rejectLink(current);
        }
        return absolute;
    }

    public static IReadOnlyList<string> EnumerateInputs(
        string projectPath,
        ICollection<ResourceCleanupIssue> issues,
        IProgress<ResourceCleanupProgress>? progress,
        CancellationToken token)
    {
        List<string> files = [];
        foreach (string directory in new[] { "Data", "Assets", "Scripts" })
        {
            token.ThrowIfCancellationRequested();
            try
            {
                string path = ResolveSafePath(projectPath, directory);
                if (Directory.Exists(path))
                    enumerateDirectory(projectPath, path, files, issues, progress, token);
            }
            catch (Exception exception) when (IsReadFailure(exception))
            {
                issues.Add(new ResourceCleanupIssue(directory, exception.Message));
            }
        }
        foreach (string relative in new[] { "Main.ini", "Main.proj", KeepListPath,
                     "Data.ldpak", "Scripts.ldpak", "Assets.ldpak" })
        {
            try
            {
                string path = ResolveSafePath(projectPath, relative);
                if (!File.Exists(path))
                    continue;
                files.Add(relative);
                if (relative.EndsWith(".ldpak", StringComparison.Ordinal))
                    issues.Add(new ResourceCleanupIssue(relative, "Resource cleanup requires loose authoring files."));
            }
            catch (Exception exception) when (IsReadFailure(exception))
            {
                issues.Add(new ResourceCleanupIssue(relative, exception.Message));
            }
        }
        return files.OrderBy(path => path, StringComparer.Ordinal).ToArray();
    }

    public static ResourceCleanupFileFingerprint Fingerprint(
        string projectPath, string relativePath, CancellationToken token)
    {
        string path = ResolveSafePath(projectPath, relativePath);
        FileInfo before = new(path);
        long size = before.Length;
        long modified = before.LastWriteTimeUtc.Ticks;
        using FileStream stream = new(path, FileMode.Open, FileAccess.Read, FileShare.Read,
            131072, FileOptions.SequentialScan);
        using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        byte[] buffer = new byte[131072];
        int read;
        while ((read = stream.Read(buffer)) != 0)
        {
            token.ThrowIfCancellationRequested();
            hash.AppendData(buffer, 0, read);
        }
        FileInfo after = new(path);
        if (after.Length != size || after.LastWriteTimeUtc.Ticks != modified)
            throw new InvalidDataException($"The file changed while reading; scan again: {relativePath}");
        ResolveSafePath(projectPath, relativePath);
        return new ResourceCleanupFileFingerprint(size, modified, Convert.ToHexString(hash.GetHashAndReset()));
    }

    public static bool IsReadFailure(Exception exception)
    {
        return exception is IOException or InvalidDataException or UnauthorizedAccessException or ArgumentException
            or NotSupportedException or System.Text.Json.JsonException;
    }

    private static void enumerateDirectory(
        string projectPath,
        string directory,
        ICollection<string> files,
        ICollection<ResourceCleanupIssue> issues,
        IProgress<ResourceCleanupProgress>? progress,
        CancellationToken token)
    {
        foreach (string path in Directory.EnumerateFileSystemEntries(directory).OrderBy(path => path, StringComparer.Ordinal))
        {
            token.ThrowIfCancellationRequested();
            string relative = Path.GetRelativePath(projectPath, path).Replace('\\', '/');
            try
            {
                rejectLink(path);
                if (Directory.Exists(path))
                    enumerateDirectory(projectPath, path, files, issues, progress, token);
                else
                    files.Add(relative);
                if (files.Count % 32 == 0)
                    progress?.Report(new ResourceCleanupProgress("Inventory", files.Count, 0, relative));
            }
            catch (Exception exception) when (IsReadFailure(exception))
            {
                issues.Add(new ResourceCleanupIssue(relative, exception.Message));
            }
        }
    }

    private static void rejectLink(string path)
    {
        if ((File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
            throw new InvalidDataException($"Symbolic links and reparse points cannot be scanned or recycled: {path}");
    }
}
