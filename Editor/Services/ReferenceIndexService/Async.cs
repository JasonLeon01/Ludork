using Ludork.Models;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ReferenceIndexService
{
    private readonly SemaphoreSlim foregroundGate = new(1, 1);

    internal async Task<ReferenceIndexSnapshotBuilder.Result> GetCurrentAsync(
        IProgress<EditorOperationProgress>? progress)
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        await foregroundGate.WaitAsync();
        foregroundRefresh = true;
        refreshTimer?.Stop();
        try
        {
            ObjectDisposedException.ThrowIf(disposed, this);
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
            backgroundBuilder ??= new ReferenceIndexSnapshotBuilder();
            ReferenceIndexSnapshotBuilder builder = backgroundBuilder;
            ReferenceIndexSnapshotBuilder.Result result = await Task.Run(() => builder.Build(input, default, progress));
            if (disposed || input.Version != version)
                throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            return result;
        }
        finally
        {
            foregroundRefresh = false;
            foregroundGate.Release();
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
