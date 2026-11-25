using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Ludork.Services;

internal sealed class BuiltInResourcePaths
{
    private static readonly string[] directories = loadDirectories();
    private readonly string[] absoluteDirectories;
    private readonly StringComparison comparison;

    internal BuiltInResourcePaths(string projectPath)
    {
        string root = Path.GetFullPath(projectPath);
        absoluteDirectories = Array.ConvertAll(directories, directory =>
            Path.GetFullPath(directory, root).Replace(Path.DirectorySeparatorChar, '/'));
        comparison = FileSystemPathIdentity.GetProjectComparer(root) == StringComparer.OrdinalIgnoreCase
            ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
    }

    internal bool IsFixedPath(string projectRelativePath)
    {
        string path = projectRelativePath.Replace(Path.DirectorySeparatorChar, '/');
        foreach (string directory in directories)
            if (contains(directory, path))
                return true;
        return false;
    }

    internal string? GetError(IEnumerable<string> paths, IEnumerable<string>? projectedPaths = null)
    {
        foreach (string path in paths)
        {
            string candidate = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path)).Replace(Path.DirectorySeparatorChar, '/');
            for (int index = 0; index < directories.Length; index++)
            {
                string protectedPath = absoluteDirectories[index];
                if (contains(protectedPath, candidate))
                    return formatError(directories[index] + candidate[protectedPath.Length..]);
                if (!contains(candidate, protectedPath))
                    continue;
                if (Directory.Exists(protectedPath) || projectedPaths?.Any(entry =>
                        contains(protectedPath, entry.Replace(Path.DirectorySeparatorChar, '/'))) == true)
                    return formatError(directories[index]);
            }
        }
        return null;
    }

    internal void AssertCanChangeOnDisk(string path)
    {
        if (GetError([path]) is string reason)
            throw new InvalidOperationException(reason);
    }

    private bool contains(string directory, string path) => path.StartsWith(directory, comparison)
        && (path.Length == directory.Length || directory.EndsWith('/') || path[directory.Length] == '/');

    private static string formatError(string path) =>
        LocaleService.Get("BUILT_IN_RESOURCE_FIXED_PATH").Replace("{path}", path, StringComparison.Ordinal);

    private static string[] loadDirectories()
    {
        using Stream stream = typeof(BuiltInResourcePaths).Assembly.GetManifestResourceStream("Ludork.BuiltInResourceDirectories")
            ?? throw new InvalidOperationException("The built-in resource directory list is missing.");
        using StreamReader reader = new(stream);
        return reader.ReadToEnd().Split(['\r', '\n'], StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
    }
}
