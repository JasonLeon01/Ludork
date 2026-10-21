using Ludork.Models;
using System;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ReferenceIndexService
{
    internal async Task<ReferenceIndexSnapshotBuilder.Result> GetCurrentAsync(
        IProgress<EditorOperationProgress>? progress)
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        foregroundRefresh = true;
        refreshTimer?.Stop();
        try
        {
            backgroundCancellation?.Cancel();
            if (backgroundTask is Task pending)
                await pending;
            ReferenceInputFiles? files = inputFiles;
            long requestedVersion = version;
            if (!dirty && pendingDocuments.Count == 0 && CurrentSnapshot?.Version == requestedVersion
                && files is not null && await Task.Run(files.IsCurrent)
                && requestedVersion == version && metadataRevision == metadataService.CacheRevision)
                return new ReferenceIndexSnapshotBuilder.Result(CurrentSnapshot!, files);
            ReferenceBuildInput input = await captureBuildInputAsync(progress);
            using ReferenceIndexSnapshotBuilder builder = new(gameData.ProjectPath);
            ReferenceIndexSnapshotBuilder.Result result = await Task.Run(() => builder.Build(input, default, progress));
            if (input.Version != version)
                throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            return result;
        }
        finally
        {
            foregroundRefresh = false;
            if (activated && (dirty || pendingDocuments.Count != 0))
                scheduleRefresh();
        }
    }

    internal void UseOperationSnapshot(ReferenceIndexSnapshotBuilder.Result result)
    {
        version = result.Snapshot.Version;
        importSnapshot(result.Snapshot);
        inputFiles = result.Files;
        observedFiles = result.Files;
        metadataRevision = metadataService.Revision;
        CurrentSnapshot = result.Snapshot;
    }
}
