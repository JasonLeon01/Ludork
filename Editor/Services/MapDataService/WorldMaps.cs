using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    internal readonly SemaphoreSlim worldChildReads = new(2);

    public async Task<JsonObject?> ReadWorldChildMapSnapshotAsync(string key, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (store.IsDisposed)
            return null;
        key = normaliseMapKey(key);
        if (mapDocuments.TryGetValue(key, out JsonObject? current))
            return current.DeepClone() as JsonObject;
        if (!tryGetMapCatalogEntry(key, out MapCatalogEntry entry)
            || entry.Kind != MapCatalogEntryKind.WorldChildMap)
        {
            return null;
        }
        string path = getReadableMapDataPath(key);
        await worldChildReads.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            JsonObject? snapshot = await Task.Run(() => readMapFile(path, true), cancellationToken).ConfigureAwait(false);
            cancellationToken.ThrowIfCancellationRequested();
            return snapshot;
        }
        finally
        {
            worldChildReads.Release();
        }
    }

    public JsonObject? InstallWorldChildMapSnapshot(string key, JsonObject snapshot)
    {
        if (store.IsDisposed)
            return null;
        key = normaliseMapKey(key);
        if (mapDocuments.TryGetValue(key, out JsonObject? loaded))
        {
            touchMap(key);
            return (JsonObject)loaded.DeepClone();
        }
        if (!tryGetMapCatalogEntry(key, out MapCatalogEntry entry)
            || entry.Kind != MapCatalogEntryKind.WorldChildMap
            || !mapMatchesCatalogEntry(snapshot, entry))
        {
            return null;
        }
        string path = getReadableMapDataPath(key);
        if (!File.Exists(path))
            return null;
        store.AcceptLoadedDocument("Maps", key, (JsonObject)snapshot.DeepClone());
        mapLoadedBytes[key] = new FileInfo(path).Length;
        touchMap(key);
        return snapshot;
    }

    public int TrimWorldChildCache(
        IReadOnlyCollection<string>? pinnedMapKeys = null,
        int maximumLoadedChildren = 32,
        long maximumLoadedBytes = 256L * 1024L * 1024L)
    {
        HashSet<string> pinned = pinnedMapKeys is null
            ? new HashSet<string>(StringComparer.Ordinal)
            : new HashSet<string>(pinnedMapKeys.Select(normaliseMapKey), StringComparer.Ordinal);
        List<string> candidates = mapDocuments.Keys
            .Where(key => store.Worlds.TryGetWorldForMap(key, out _)
                && !pinned.Contains(key)
                && !isMapHeldByHistory(key)
                && isCleanLoadedMap(key))
            .OrderBy(key => mapAccessOrder.GetValueOrDefault(key))
            .ToList();
        int loadedCount = mapDocuments.Keys.Count(key => store.Worlds.TryGetWorldForMap(key, out _));
        long loadedBytes = mapDocuments.Keys
            .Where(key => store.Worlds.TryGetWorldForMap(key, out _))
            .Sum(key => mapLoadedBytes.GetValueOrDefault(key));
        int removed = 0;
        foreach (string key in candidates)
        {
            if (loadedCount <= Math.Max(0, maximumLoadedChildren)
                && loadedBytes <= Math.Max(0L, maximumLoadedBytes))
            {
                break;
            }
            loadedCount -= 1;
            loadedBytes -= mapLoadedBytes.GetValueOrDefault(key);
            store.ReleaseUntrackedDocument("Maps", key);
            mapAccessOrder.Remove(key);
            mapLoadedBytes.Remove(key);
            removed += 1;
        }
        return removed;
    }

}
