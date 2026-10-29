using Ludork.Models;
using System;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    internal readonly SemaphoreSlim worldChildReads = new(2);

    internal bool CanLoadWorldChildPreview => worldPreviewCache.CanLoad;

    internal event EventHandler? WorldChildPreviewReady
    {
        add => worldPreviewCache.Ready += value;
        remove => worldPreviewCache.Ready -= value;
    }

    internal IDisposable RetainWorldChildPreview(string key) => worldPreviewCache.Acquire(normaliseMapKey(key));

    internal void SetWorldChildPreviewCacheBudget(int maximumLoadedChildren = 32,
        long maximumLoadedBytes = 256L * 1024L * 1024L)
        => worldPreviewCache.SetBudget(maximumLoadedChildren, maximumLoadedBytes);

    internal async Task<WorldChildMapReadResult?> ReadWorldChildMapSnapshotAsync(string key,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (store.IsDisposed)
            return null;
        key = normaliseMapKey(key);
        WorldMapPreviewCache.Version version = worldPreviewCache.GetVersion(key);
        if (!CanLoadWorldChildPreview)
            return new WorldChildMapReadResult(key, null, version, null, null, true);
        if (mapDocuments.TryGetValue(key, out JsonObject? current))
            return new WorldChildMapReadResult(key, (JsonObject)current.DeepClone(), version, null, null);
        if (!tryGetMapCatalogEntry(key, out MapCatalogEntry entry)
            || entry.Kind != MapCatalogEntryKind.WorldChildMap || entry.WorldKey is null)
            return null;
        if (worldPreviewCache.Read(key) is WorldChildMapReadResult cached)
            return cached;
        version = worldPreviewCache.GetVersion(key);
        string path = getReadableMapDataPath(key);
        await worldChildReads.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            return await Task.Run(() =>
            {
                cancellationToken.ThrowIfCancellationRequested();
                WorldChildMapReadResult.FileVersion? before = WorldChildMapReadResult.FileVersion.Read(path);
                JsonObject? snapshot = before is null ? null : readMapFile(path, true);
                WorldChildMapReadResult.FileVersion? after = WorldChildMapReadResult.FileVersion.Read(path);
                cancellationToken.ThrowIfCancellationRequested();
                return new WorldChildMapReadResult(key, snapshot, version, path, after, before != after);
            }, cancellationToken).ConfigureAwait(false);
        }
        finally
        {
            worldChildReads.Release();
        }
    }

    internal JsonObject? InstallWorldChildMapSnapshot(WorldChildMapReadResult read, out bool retry)
    {
        retry = false;
        if (store.IsDisposed)
            return null;
        if (!CanLoadWorldChildPreview)
        {
            retry = true;
            return null;
        }
        if (mapDocuments.TryGetValue(read.Key, out JsonObject? loaded))
            return (JsonObject)loaded.DeepClone();
        if (!worldPreviewCache.IsCurrent(read.Key, read.Version) || read.Retry || read.Path is null)
        {
            retry = true;
            return null;
        }
        if (!string.Equals(read.Path, getReadableMapDataPath(read.Key), OperatingSystem.IsWindows()
                ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal)
            || WorldChildMapReadResult.FileVersion.Read(read.Path) != read.File)
        {
            worldPreviewCache.Invalidate(read.Key);
            retry = true;
            return null;
        }
        if (read.Snapshot is not JsonObject snapshot || read.File is null
            || !tryGetMapCatalogEntry(read.Key, out MapCatalogEntry entry)
            || entry.Kind != MapCatalogEntryKind.WorldChildMap || entry.WorldKey is null
            || !mapMatchesCatalogEntry(snapshot, entry)
            || !store.tryValidateMapBlueprintData(read.Path, snapshot))
            return null;
        worldPreviewCache.Install(read);
        return snapshot;
    }

    internal void DisposePreviewCache() => worldPreviewCache.Dispose();
}
