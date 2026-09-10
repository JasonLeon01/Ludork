using System;
using System.IO;

namespace Ludork.Services;

public sealed class WorldMapPathPolicy
{
    private readonly string mapsRoot;
    private readonly StringComparison pathComparison;

    public WorldMapPathPolicy(string projectPath)
    {
        mapsRoot = Path.GetFullPath(Path.Combine(projectPath, "Data", "Maps"));
        pathComparison = OperatingSystem.IsWindows()
            ? StringComparison.OrdinalIgnoreCase
            : StringComparison.Ordinal;
    }

    public string MapsRoot => mapsRoot;

    public bool IsMapsRoot(string path)
    {
        return string.Equals(
            Path.TrimEndingDirectorySeparator(Path.GetFullPath(path)),
            Path.TrimEndingDirectorySeparator(mapsRoot),
            pathComparison);
    }

    public bool IsWorldDirectory(string path)
    {
        return tryGetDepth(path, out int depth)
            && depth == 1
            && File.Exists(Path.Combine(Path.GetFullPath(path), "_world.json"));
    }

    public bool CanCreateDirectory(string parentPath)
    {
        return !isInMapsTree(parentPath);
    }

    public bool CanCreateWorldDirectory(string parentPath)
    {
        return IsMapsRoot(parentPath);
    }

    public bool CanDeletePath(string path)
    {
        return !isInMapsTree(path);
    }

    public bool CanTransferPath(string sourcePath, string destinationPath)
    {
        bool sourceInMaps = tryGetDepth(sourcePath, out int sourceDepth);
        bool destinationInMaps = tryGetDepth(destinationPath, out int destinationDepth);
        if (!sourceInMaps && !destinationInMaps)
            return true;
        if (!sourceInMaps || !destinationInMaps)
            return false;
        bool sourceDirectory = Directory.Exists(sourcePath);
        if (sourceDirectory)
        {
            return sourceDepth == 1
                && destinationDepth == 1
                && IsWorldDirectory(sourcePath);
        }
        bool sourceMap = isMapFile(sourcePath);
        bool destinationMap = isMapFile(destinationPath);
        if (!sourceMap || !destinationMap)
            return false;
        return sourceDepth == destinationDepth
            && (sourceDepth == 1 || sourceDepth == 2)
            && (sourceDepth != 2
                || string.Equals(
                    Path.GetDirectoryName(Path.GetFullPath(sourcePath)),
                    Path.GetDirectoryName(Path.GetFullPath(destinationPath)),
                    pathComparison));
    }

    private static bool isMapFile(string path)
    {
        return string.Equals(Path.GetExtension(path), ".json", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(Path.GetFileName(path), "_world.json", StringComparison.OrdinalIgnoreCase);
    }

    private bool tryGetDepth(string path, out int depth)
    {
        string relative = Path.GetRelativePath(mapsRoot, Path.GetFullPath(path));
        if (Path.IsPathRooted(relative)
            || relative == ".."
            || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal))
        {
            depth = 0;
            return false;
        }
        depth = relative == "."
            ? 0
            : relative.Split(Path.DirectorySeparatorChar, StringSplitOptions.RemoveEmptyEntries).Length;
        return true;
    }

    private bool isInMapsTree(string path)
    {
        return tryGetDepth(path, out _);
    }
}
