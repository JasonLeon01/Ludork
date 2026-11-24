using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;

namespace Ludork.Services;

internal sealed class ReferenceInputFiles
{
    private readonly string projectPath;
    private readonly string[] mapPaths;
    private readonly Dictionary<string, Stamp> stamps;
    private readonly bool includeData;
    private readonly bool includeStubs;

    private ReferenceInputFiles(string projectPath, string[] mapPaths, Dictionary<string, Stamp> stamps, bool includeData, bool includeStubs)
    {
        this.projectPath = projectPath;
        this.mapPaths = mapPaths;
        this.stamps = stamps;
        this.includeData = includeData;
        this.includeStubs = includeStubs;
    }

    internal static ReferenceInputFiles Capture(string projectPath, IEnumerable<string> mapPaths,
        bool includeData = false, bool includeStubs = false, CancellationToken cancellationToken = default)
    {
        string[] paths = mapPaths.Distinct(StringComparer.Ordinal).OrderBy(path => path, StringComparer.Ordinal).ToArray();
        string scripts = Path.Combine(projectPath, "Scripts");
        IEnumerable<string> metadata = Directory.Exists(scripts)
            ? Directory.EnumerateFiles(scripts, "*_meta.lua", SearchOption.AllDirectories) : [];
        string enumRoot = Path.Combine(scripts, "Enums");
        IEnumerable<string> enums = Directory.Exists(enumRoot)
            ? Directory.EnumerateFiles(enumRoot, "*.lua", SearchOption.AllDirectories) : [];
        string stubsRoot = Path.Combine(scripts, "stub");
        IEnumerable<string> stubs = includeStubs && Directory.Exists(stubsRoot)
            ? Directory.EnumerateFiles(stubsRoot, "*.d.lua", SearchOption.AllDirectories) : [];
        string dataRoot = Path.Combine(projectPath, "Data");
        IEnumerable<string> data = includeData && Directory.Exists(dataRoot)
            ? Directory.EnumerateFiles(dataRoot, "*.json", SearchOption.AllDirectories)
                .Where(path => !DataConfig.isAnimationCache(path)) : [];
        Dictionary<string, Stamp> stamps = ReadStamps(metadata.Concat(enums).Concat(stubs).Concat(paths).Concat(data), cancellationToken);
        return new ReferenceInputFiles(projectPath, paths, stamps, includeData, includeStubs);
    }

    internal Stamp GetStamp(string path) => stamps[path];

    internal bool IsCurrent() => IsCurrent(default);

    internal bool IsCurrent(CancellationToken cancellationToken)
    {
        ReferenceInputFiles current = Capture(projectPath, mapPaths, includeData, includeStubs, cancellationToken);
        return HasSameInputs(current);
    }

    internal bool HasSameInputs(ReferenceInputFiles other)
        => stamps.Count == other.stamps.Count && stamps.All(pair =>
            other.stamps.TryGetValue(pair.Key, out Stamp value) && pair.Value == value);

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

    internal static Dictionary<string, Stamp> ReadStamps(IEnumerable<string> paths, CancellationToken cancellationToken = default)
    {
        string[] files = paths.Distinct(StringComparer.Ordinal).ToArray();
        Stamp[] values = EditorValidationBatch.RunFiles(files, Read, cancellationToken);
        Dictionary<string, Stamp> result = new(files.Length, StringComparer.Ordinal);
        for (int index = 0; index < files.Length; index++)
            result.Add(files[index], values[index]);
        return result;
    }

    internal static Stamp Read(string path)
    {
        FileInfo file = new(path);
        return file.Exists ? new Stamp(true, file.Length, file.LastWriteTimeUtc.Ticks) : new Stamp(false, 0, 0);
    }

    internal readonly record struct Stamp(bool Exists, long Length, long Modified);
}
