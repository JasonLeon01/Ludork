using Avalonia.Media;
using Ludork.Services;
using SkiaSharp;
using System;
using System.Collections.Generic;
using System.IO;

namespace Ludork.Views;

internal static class TextConfigFontLoader
{
    private static readonly Dictionary<string, FontCacheEntry> Cache =
        new(StringComparer.OrdinalIgnoreCase);

    public static bool TryResolve(
        ProjectDataStore gameData,
        string reference,
        out FontFamily family)
    {
        family = FontFamily.Default;
        if (!tryResolvePath(gameData, reference, out string path))
            return false;
        DateTime stamp = File.GetLastWriteTimeUtc(path);
        if (Cache.TryGetValue(path, out FontCacheEntry? cached)
            && cached.Stamp == stamp)
        {
            family = cached.Family;
            return true;
        }
        using SKTypeface? typeface = SKTypeface.FromFile(path);
        if (typeface is null || string.IsNullOrWhiteSpace(typeface.FamilyName))
            return false;
        Uri baseUri = new(Path.GetDirectoryName(path)! + Path.DirectorySeparatorChar);
        family = new FontFamily(
            baseUri,
            $"{Path.GetFileName(path)}#{typeface.FamilyName}");
        Cache[path] = new FontCacheEntry(stamp, family);
        return true;
    }

    private static bool tryResolvePath(
        ProjectDataStore gameData,
        string reference,
        out string path)
    {
        path = string.Empty;
        string extension = Path.GetExtension(reference);
        if (!string.Equals(extension, ".ttf", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(extension, ".otf", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }
        if (!gameData.AssetPaths.TryResolveAsset(reference, out path))
            return false;
        string fontsRoot = Path.GetFullPath(Path.Combine(gameData.ProjectPath, "Assets", "Fonts"));
        GameAssetPath.TryToProjectFile(gameData.ProjectPath, reference, out string logicalPath);
        string relative = Path.GetRelativePath(fontsRoot, logicalPath);
        return !Path.IsPathRooted(relative)
            && relative != ".."
            && !relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal);
    }

    private sealed record FontCacheEntry(DateTime Stamp, FontFamily Family);
}
