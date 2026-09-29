using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Ludork.Services;

internal sealed class ProjectResourceDeletionValidation(string projectPath) : IDisposable
{
    private ProjectDataStore? diskData;
    private Dictionary<string, ReferenceInputFiles.Stamp> inputs = new(StringComparer.Ordinal);

    internal void AssertUnreferenced(IReadOnlyList<string> paths)
    {
        Dictionary<string, ReferenceInputFiles.Stamp> current = captureInputs();
        if (diskData is null || inputs.Count != current.Count
            || inputs.Any(pair => !current.TryGetValue(pair.Key, out ReferenceInputFiles.Stamp value) || value != pair.Value))
        {
            diskData?.Dispose();
            diskData = null;
            inputs = current;
            diskData = new ProjectDataStore(projectPath, cacheMapCatalog: false);
        }
        ProjectDataStore.assertDocumentsUnreferenced(diskData, paths);
    }

    internal void AcceptDeleted(IReadOnlyList<string> paths)
    {
        if (diskData is null || paths.Count == 0)
            return;
        diskData.AcceptTrashedResources(paths, false);
        foreach (string path in paths)
            inputs.Remove(Path.GetFullPath(path));
    }

    private Dictionary<string, ReferenceInputFiles.Stamp> captureInputs()
    {
        string data = Path.Combine(projectPath, "Data");
        IEnumerable<string> files = Directory.Exists(data)
            ? Directory.EnumerateFiles(data, "*.json", SearchOption.AllDirectories)
                .Where(path => !DataConfig.isAnimationCache(path))
            : [];
        string scripts = Path.Combine(projectPath, "Scripts");
        if (Directory.Exists(scripts))
            files = files.Concat(Directory.EnumerateFiles(scripts, "*_meta.lua", SearchOption.AllDirectories));
        return files.Distinct(StringComparer.Ordinal).ToDictionary(path => path, ReferenceInputFiles.Read, StringComparer.Ordinal);
    }

    public void Dispose() => diskData?.Dispose();
}
