using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Ludork.Services;

public static class EditorRuntimePaths
{
    internal static string ContentRoot => OperatingSystem.IsMacOS()
        ? Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "Resources"))
        : AppContext.BaseDirectory;

    internal static string DevelopmentRoot => Path.GetFullPath(Path.Combine(
        AppContext.BaseDirectory,
        "..",
        "..",
        ".."
    ));

    public static string? FindDirectory(string relativePath)
    {
        foreach (string root in getRoots())
        {
            string path = Path.GetFullPath(Path.Combine(root, relativePath));
            if (Directory.Exists(path))
                return path;
        }
        return null;
    }

    public static string? FindFile(params string[] parts)
    {
        string relativePath = Path.Combine(parts);
        foreach (string root in getRoots())
        {
            string path = Path.GetFullPath(Path.Combine(root, relativePath));
            if (File.Exists(path))
                return path;
        }
        return null;
    }

    private static IEnumerable<string> getRoots()
    {
        string[] roots = OperatingSystem.IsMacOS()
            ?
            [
                ContentRoot,
                Environment.CurrentDirectory,
                AppContext.BaseDirectory,
                DevelopmentRoot,
            ]
            :
            [
                AppContext.BaseDirectory,
                DevelopmentRoot,
                Environment.CurrentDirectory,
            ];
        return roots
            .Where(Directory.Exists)
            .Select(Path.GetFullPath)
            .Distinct(StringComparer.OrdinalIgnoreCase);
    }
}
