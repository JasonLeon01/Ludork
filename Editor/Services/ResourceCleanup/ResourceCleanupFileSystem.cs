using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
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

    public static string ResolveSafePath(string projectPath, string relativePath, bool allowMissing = false)
    {
        string normalized = relativePath.Replace('\\', '/');
        if (Path.IsPathRooted(normalized) || normalized.Length == 0
            || normalized.Split('/').Any(part => part.Length == 0 || part is "." or "..")
            || !EditorPathSandbox.TryResolve(projectPath, normalized, out string absolute, allowMissing))
        {
            throw new InvalidDataException($"The path is outside the project, linked or inaccessible: {relativePath}");
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
        string[] roots = ["Data", "Assets", "Scripts"];
        List<string> directories = [.. roots];
        List<ResourceCleanupIssue> directoryIssues = [];
        bool optionalRoots = true;
        while (directories.Count != 0)
        {
            (string[] Entries, ResourceCleanupIssue? Issue)[] listings = EditorValidationBatch.RunFiles(
                directories, directory => inspectInput(projectPath, directory, optionalRoots,
                    path => enumerateDirectory(projectPath, path, optionalRoots)), token);
            optionalRoots = false;
            List<string> entries = [];
            foreach ((string[] children, ResourceCleanupIssue? issue) in listings)
            {
                if (issue is not null)
                    directoryIssues.Add(issue);
                else
                    entries.AddRange(children);
            }
            (bool Directory, ResourceCleanupIssue? Issue)[] inspected = EditorValidationBatch.RunFiles(
                entries, relative => inspectInput(projectPath, relative, false, Directory.Exists), token);
            directories.Clear();
            for (int index = 0; index < entries.Count; index++)
            {
                token.ThrowIfCancellationRequested();
                string relative = entries[index];
                (bool isDirectory, ResourceCleanupIssue? issue) = inspected[index];
                if (issue is not null)
                    directoryIssues.Add(issue);
                else if (isDirectory)
                    directories.Add(relative);
                else
                {
                    files.Add(relative);
                    if (files.Count % 32 == 0)
                        progress?.Report(new ResourceCleanupProgress("Inventory", files.Count, 0, relative));
                }
            }
        }
        foreach (ResourceCleanupIssue issue in directoryIssues
            .OrderBy(issue => Array.IndexOf(roots, issue.Path.Split('/', 2)[0]))
            .ThenBy(issue => issue.Path.Replace('/', '\0'), StringComparer.Ordinal))
        {
            issues.Add(issue);
        }
        string[] optionalFiles = ["Main.proj", KeepListPath, "Data.ldpak", "Scripts.ldpak", "Assets.ldpak"];
        (bool Present, ResourceCleanupIssue? Issue)[] checkedFiles = EditorValidationBatch.RunFiles(
            optionalFiles, relative => inspectInput(projectPath, relative, true, File.Exists), token);
        for (int index = 0; index < optionalFiles.Length; index++)
        {
            token.ThrowIfCancellationRequested();
            string relative = optionalFiles[index];
            (bool present, ResourceCleanupIssue? issue) = checkedFiles[index];
            if (issue is not null)
                issues.Add(issue);
            else if (present)
            {
                files.Add(relative);
                if (relative.EndsWith(".ldpak", StringComparison.Ordinal))
                    issues.Add(new ResourceCleanupIssue(relative, "Resource cleanup requires loose authoring files."));
            }
        }
        return files.OrderBy(path => path, StringComparer.Ordinal).ToArray();
    }

    public static (ResourceCleanupFileFingerprint Fingerprint, byte[]? Content) ReadFile(
        string projectPath, string relativePath, FileFingerprintCache cache, byte[] buffer,
        CancellationToken token, bool includeContent = false, bool forceRead = false)
    {
        string path = ResolveSafePath(projectPath, relativePath);
        FileInfo before = new(path);
        long size = before.Length;
        long modified = before.LastWriteTimeUtc.Ticks;
        using FileStream stream = new(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1, FileOptions.SequentialScan);
        (byte[] digest, byte[]? content) = cache.Read(path, stream, buffer, token, includeContent, forceRead);
        FileInfo after = new(path);
        if (after.Length != size || after.LastWriteTimeUtc.Ticks != modified)
            throw new InvalidDataException($"The file changed while reading; scan again: {relativePath}");
        ResolveSafePath(projectPath, relativePath);
        return (new ResourceCleanupFileFingerprint(size, modified, Convert.ToHexString(digest)), content);
    }

    public static bool IsReadFailure(Exception exception)
    {
        return EditorPathSandbox.IsPathFailure(exception) || exception is InvalidDataException or System.Text.Json.JsonException;
    }

    private static string[] enumerateDirectory(string projectPath, string directory, bool optional)
    {
        if (optional && !Directory.Exists(directory))
            return [];
        return Directory.GetFileSystemEntries(directory)
            .Select(path => Path.GetRelativePath(projectPath, path).Replace('\\', '/'))
            .Order(StringComparer.Ordinal).ToArray();
    }

    private static (T Value, ResourceCleanupIssue? Issue) inspectInput<T>(
        string projectPath, string relative, bool allowMissing, Func<string, T> inspect)
    {
        try
        {
            return (inspect(ResolveSafePath(projectPath, relative, allowMissing)), null);
        }
        catch (Exception exception) when (IsReadFailure(exception))
        {
            return (default!, new ResourceCleanupIssue(relative, exception.Message));
        }
    }
}
