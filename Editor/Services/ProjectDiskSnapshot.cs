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

    internal static ProjectDiskSnapshot Capture(string projectPath, CancellationToken cancellationToken = default,
        bool includeContent = true, IProgress<EditorOperationProgress>? progress = null,
        string stage = "EXTERNAL_SCANNING")
    {
        ProjectDiskSnapshot snapshot = new();
        List<string> paths = [];
        progress?.Report(new(stage));
        foreach (string root in new[] { "Data", "Assets" })
        {
            string directory = Path.Combine(projectPath, root);
            if (!DirectoryExists(directory))
                continue;
            EnumerationOptions options = new() { RecurseSubdirectories = true, IgnoreInaccessible = false,
                AttributesToSkip = FileAttributes.ReparsePoint };
            foreach (string path in Directory.EnumerateFiles(directory, "*", options))
            {
                cancellationToken.ThrowIfCancellationRequested();
                if (!DataConfig.isAnimationCache(path))
                    paths.Add(path);
            }
        }
        byte[] buffer = new byte[128 * 1024];
        string dataDirectory = Path.Combine(projectPath, "Data");
        for (int index = 0; index < paths.Count; index++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            string path = paths[index];
            progress?.Report(new(stage, Path.GetRelativePath(projectPath, path), index, paths.Count));
            using FileStream stream = new(path, FileMode.Open, FileAccess.Read, FileShare.Read);
            using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
            using MemoryStream? content = includeContent && IsWithin(path, dataDirectory)
                && Path.GetExtension(path).Equals(".json", StringComparison.OrdinalIgnoreCase) ? new() : null;
            int count;
            while ((count = stream.Read(buffer)) != 0)
            {
                cancellationToken.ThrowIfCancellationRequested();
                hash.AppendData(buffer, 0, count);
                content?.Write(buffer, 0, count);
            }
            snapshot.Fingerprints[path] = Convert.ToHexString(hash.GetHashAndReset());
            if (content is not null)
                snapshot.Data[path] = content.ToArray();
            progress?.Report(new(stage, Path.GetRelativePath(projectPath, path), index + 1, paths.Count));
        }
        return snapshot;
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
