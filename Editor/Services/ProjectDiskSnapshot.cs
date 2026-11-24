using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;

namespace Ludork.Services;

internal sealed class ProjectDiskSnapshot
{
    internal static StringComparer PathComparer { get; } = OperatingSystem.IsWindows()
        ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;

    internal Dictionary<string, string> Fingerprints { get; } = new(PathComparer);
    internal Dictionary<string, byte[]> Data { get; } = new(PathComparer);

    internal static ProjectDiskSnapshot Capture(string projectPath, FileFingerprintCache cache, CancellationToken token,
        IProgress<EditorOperationProgress>? progress = null, string stage = "EXTERNAL_SCANNING",
        IReadOnlyDictionary<string, string>? previous = null, IReadOnlySet<string>? contentPaths = null)
    {
        ProjectDiskSnapshot snapshot = new();
        progress?.Report(new(stage));
        snapshot.captureFiles(projectPath, EnumeratePaths(projectPath, token), cache, token, progress, stage, previous, contentPaths);
        return snapshot;
    }

    internal void CaptureRemaining(string projectPath, IReadOnlyList<string> paths, FileFingerprintCache cache, CancellationToken token)
        => captureFiles(projectPath, paths.Where(path => !Fingerprints.ContainsKey(path)).ToArray(), cache, token,
            null, "EXTERNAL_SCANNING", null, null);

    internal static List<string> EnumeratePaths(string projectPath, CancellationToken token)
    {
        token.ThrowIfCancellationRequested();
        List<string> paths = [];
        foreach (string root in new[] { "Data", "Assets" })
        {
            string directory = Path.Combine(projectPath, root);
            if (!DirectoryExists(directory))
                continue;
            EnumerationOptions options = new() { RecurseSubdirectories = true, IgnoreInaccessible = false,
                AttributesToSkip = FileAttributes.ReparsePoint };
            foreach (string path in Directory.EnumerateFiles(directory, "*", options))
            {
                token.ThrowIfCancellationRequested();
                if (!DataConfig.isAnimationCache(path))
                    paths.Add(path);
            }
        }
        return paths;
    }

    private void captureFiles(string projectPath, IReadOnlyList<string> paths, FileFingerprintCache cache,
        CancellationToken token, IProgress<EditorOperationProgress>? progress, string stage,
        IReadOnlyDictionary<string, string>? previous, IReadOnlySet<string>? contentPaths)
    {
        string dataDirectory = Path.Combine(projectPath, "Data");
        int completed = 0;
        object progressLock = new();
        (byte[] Digest, byte[]? Content)[] results = EditorValidationBatch.RunFiles<string, byte[], (byte[], byte[]?)>(
            paths, static () => new byte[128 * 1024], (buffer, path) =>
            {
                (byte[] Digest, byte[]? Content) result;
                bool dataJson = (previous is not null || contentPaths is not null) && IsWithin(path, dataDirectory)
                    && Path.GetExtension(path).Equals(".json", StringComparison.OrdinalIgnoreCase);
                if (dataJson && contentPaths?.Contains(path) == true)
                    result = cache.ReadContent(path, buffer, token);
                else if (dataJson && previous is not null)
                    result = cache.ReadChangedContent(path, buffer, token, previous.GetValueOrDefault(path));
                else
                    result = (cache.Read(path, buffer, token), null);
                if (progress is not null)
                    lock (progressLock)
                        progress.Report(new(stage, Path.GetRelativePath(projectPath, path), ++completed, paths.Count));
                return result;
            }, cancellationToken: token);
        for (int index = 0; index < paths.Count; index++)
        {
            string path = paths[index];
            (byte[] digest, byte[]? content) = results[index];
            Fingerprints[path] = Convert.ToHexString(digest);
            if (content is not null)
                Data[path] = content;
        }
        cache.RetainPaths(Fingerprints.Keys);
    }

    internal bool Matches(ProjectDiskSnapshot other) => Equal(Fingerprints, other.Fingerprints);

    internal static bool Equal(IReadOnlyDictionary<string, string> left, IReadOnlyDictionary<string, string> right)
        => left.Count == right.Count && left.All(pair => right.TryGetValue(pair.Key, out string? value) && value == pair.Value);

    internal static string? ReadFingerprint(string path)
    {
        try
        {
            using FileStream stream = new(path, FileMode.Open, FileAccess.Read, FileShare.Read);
            return Convert.ToHexString(SHA256.HashData(stream));
        }
        catch (FileNotFoundException) { return null; }
        catch (DirectoryNotFoundException) { return null; }
    }

    internal static Dictionary<string, string?> ReadFingerprints(IEnumerable<string> filePaths, CancellationToken token = default)
    {
        string[] paths = filePaths.Distinct(PathComparer).ToArray();
        string?[] hashes = EditorValidationBatch.RunFiles(paths, ReadFingerprint, token);
        Dictionary<string, string?> fingerprints = new(paths.Length, PathComparer);
        for (int index = 0; index < paths.Length; index++)
            fingerprints.Add(paths[index], hashes[index]);
        return fingerprints;
    }

    internal static bool IsWithin(string path, string root) => PathComparer.Equals(path, root)
        || path.StartsWith(Path.TrimEndingDirectorySeparator(root) + Path.DirectorySeparatorChar,
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);

    internal static bool DirectoryExists(string path)
    {
        try
        {
            if ((File.GetAttributes(path) & FileAttributes.Directory) == 0)
                throw new IOException($"The resource directory is not a directory: {path}");
            return true;
        }
        catch (FileNotFoundException) { return false; }
        catch (DirectoryNotFoundException) { return false; }
    }
}
