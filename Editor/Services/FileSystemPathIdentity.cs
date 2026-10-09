using System;
using System.IO;
using System.Linq;

namespace Ludork.Services;

internal static class FileSystemPathIdentity
{
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
}
