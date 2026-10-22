using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Ludork.Services;

internal sealed class ReferenceInputFiles
{
    private readonly string projectPath;
    private readonly string[] mapPaths;
    private readonly Dictionary<string, Stamp> stamps;
    private readonly bool includeData;

    private ReferenceInputFiles(string projectPath, string[] mapPaths, Dictionary<string, Stamp> stamps, bool includeData)
    {
        this.projectPath = projectPath;
        this.mapPaths = mapPaths;
        this.stamps = stamps;
        this.includeData = includeData;
    }

    internal static ReferenceInputFiles Capture(string projectPath, IEnumerable<string> mapPaths, bool includeData = false)
    {
        string[] paths = mapPaths.Distinct(StringComparer.Ordinal).OrderBy(path => path, StringComparer.Ordinal).ToArray();
        string scripts = Path.Combine(projectPath, "Scripts");
        IEnumerable<string> metadata = Directory.Exists(scripts)
            ? Directory.EnumerateFiles(scripts, "*_meta.lua", SearchOption.AllDirectories) : [];
        string enumRoot = Path.Combine(scripts, "Enums");
        IEnumerable<string> enums = Directory.Exists(enumRoot)
            ? Directory.EnumerateFiles(enumRoot, "*.lua", SearchOption.AllDirectories) : [];
        string dataRoot = Path.Combine(projectPath, "Data");
        IEnumerable<string> data = includeData && Directory.Exists(dataRoot)
            ? Directory.EnumerateFiles(dataRoot, "*.json", SearchOption.AllDirectories)
                .Where(path => !DataConfig.isAnimationCache(path)) : [];
        Dictionary<string, Stamp> stamps = metadata.Concat(enums).Concat(paths).Concat(data).Distinct(StringComparer.Ordinal)
            .ToDictionary(path => path, Read, StringComparer.Ordinal);
        return new ReferenceInputFiles(projectPath, paths, stamps, includeData);
    }

    internal bool IsCurrent()
    {
        ReferenceInputFiles current = Capture(projectPath, mapPaths, includeData);
        return stamps.Count == current.stamps.Count && stamps.All(pair =>
            current.stamps.TryGetValue(pair.Key, out Stamp value) && pair.Value == value);
    }

    internal void AcceptDeleted(IReadOnlyList<string> paths)
    {
        StringComparer comparer = OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
        foreach (string path in paths)
        {
            foreach (string capturedPath in stamps.Keys.Where(value => comparer.Equals(value, path)).ToArray())
            {
                if (Array.IndexOf(mapPaths, capturedPath) >= 0)
                    stamps[capturedPath] = default;
                else
                    stamps.Remove(capturedPath);
            }
        }
    }

    internal static Stamp Read(string path)
    {
        FileInfo file = new(path);
        return file.Exists ? new Stamp(true, file.Length, file.LastWriteTimeUtc.Ticks) : new Stamp(false, 0, 0);
    }

    internal readonly record struct Stamp(bool Exists, long Length, long Modified);
}
