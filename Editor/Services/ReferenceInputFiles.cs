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

    private ReferenceInputFiles(string projectPath, string[] mapPaths, Dictionary<string, Stamp> stamps)
    {
        this.projectPath = projectPath;
        this.mapPaths = mapPaths;
        this.stamps = stamps;
    }

    internal static ReferenceInputFiles Capture(string projectPath, IEnumerable<string> mapPaths)
    {
        string[] paths = mapPaths.Distinct(StringComparer.Ordinal).OrderBy(path => path, StringComparer.Ordinal).ToArray();
        string scripts = Path.Combine(projectPath, "Scripts");
        IEnumerable<string> metadata = Directory.Exists(scripts)
            ? Directory.EnumerateFiles(scripts, "*_meta.lua", SearchOption.AllDirectories) : [];
        Dictionary<string, Stamp> stamps = metadata.Concat(paths).Distinct(StringComparer.Ordinal)
            .ToDictionary(path => path, Read, StringComparer.Ordinal);
        return new ReferenceInputFiles(projectPath, paths, stamps);
    }

    internal bool IsCurrent()
    {
        ReferenceInputFiles current = Capture(projectPath, mapPaths);
        return stamps.Count == current.stamps.Count && stamps.All(pair =>
            current.stamps.TryGetValue(pair.Key, out Stamp value) && pair.Value == value);
    }

    internal static Stamp Read(string path)
    {
        FileInfo file = new(path);
        return file.Exists ? new Stamp(true, file.Length, file.LastWriteTimeUtc.Ticks) : new Stamp(false, 0, 0);
    }

    internal readonly record struct Stamp(bool Exists, long Length, long Modified);
}
