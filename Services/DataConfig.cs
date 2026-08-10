using System;
using System.IO;
using System.Linq;

namespace Ludork.Services;

public static class DataConfig
{
    public const string DataFileExtension = ".json";
    public const string AnimationCacheFileSuffix = ".anim.json";

    private static readonly string[] HiddenExtensions =
    [
        ".ini", ".proj", ".csproj", ".vcxproj", ".log", ".tmp",
    ];

    public static bool shouldDisplay(string path)
    {
        string name = Path.GetFileName(path);
        if (string.IsNullOrWhiteSpace(name) || name.StartsWith(".", StringComparison.Ordinal)
            || string.Equals(name, "__pycache__", StringComparison.OrdinalIgnoreCase)
            || isAnimationCache(path))
            return false;
        return Directory.Exists(path)
            || !HiddenExtensions.Contains(Path.GetExtension(name), StringComparer.OrdinalIgnoreCase);
    }

    public static bool isAnimationCache(string path)
    {
        return Path.GetFileName(path).EndsWith(AnimationCacheFileSuffix, StringComparison.OrdinalIgnoreCase);
    }
}
