using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ReferenceIndexService
{
    public async Task<IReadOnlyList<ReferenceRecord>> GetExternalMapReferencesAsync(
        IEnumerable<string> runtimePaths, IReadOnlyCollection<string>? ignoredMapKeys = null,
        IProgress<EditorOperationProgress>? progress = null)
    {
        string[] paths = runtimePaths.ToArray();
        string[] ignored = ignoredMapKeys?.ToArray() ?? [];
        ReferenceIndexSnapshotBuilder.Result result = await GetCurrentAsync(progress);
        IReadOnlyList<ReferenceRecord> references = await Task.Run(() =>
        {
            ReferenceIndexSnapshot snapshot = result.Snapshot;
            HashSet<string> ignoredSources = ignored.Select(key =>
                ReferenceIdentity.NodeId("map", normalizeMapRuntimePath(key, false))).ToHashSet(StringComparer.Ordinal);
            IReadOnlyList<ReferenceRecord> incomingReferences = paths.Select(mapNodeIdFromRuntimePath)
                .Where(id => id is not null)
                .SelectMany(id => snapshot.GetIncoming(id!))
                .Where(record => record.Kind != "worldPlacement" && !ignoredSources.Contains(record.Source))
                .Distinct()
                .OrderBy(record => snapshot.GetNode(record.Source)?.Type, StringComparer.Ordinal)
                .ThenBy(record => snapshot.GetNode(record.Source)?.Key, StringComparer.Ordinal)
                .ThenBy(record => record.Path, StringComparer.Ordinal)
                .ToArray();
            if (!result.Files.IsCurrent())
                throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            return incomingReferences;
        });
        if (disposed || result.Snapshot.Version != version)
            throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        return references;
    }
}
