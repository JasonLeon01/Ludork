using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.ExceptionServices;
using System.Text.Json.Nodes;
using System.Threading;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    internal sealed record MapSnapshotRead(JsonObject? Data, ExceptionDispatchInfo? Error = null)
    {
        internal JsonObject? Get()
        {
            Error?.Throw();
            return Data;
        }
    }

    internal IReadOnlyDictionary<string, JsonObject> LoadMapSnapshots(IReadOnlyList<string> keys)
    {
        Dictionary<string, JsonObject> result = new(StringComparer.Ordinal);
        LoadMaps(keys, (key, map) =>
        {
            if (map is not null)
                result[key] = (JsonObject)map.DeepClone();
        });
        return result;
    }

    internal void LoadMaps(IReadOnlyList<string> keys, Action<string, JsonObject?> receive)
    {
        List<string> paths = [];
        foreach (string key in keys)
        {
            if (mapDocuments.ContainsKey(key) || !tryGetMapCatalogEntry(key, out MapCatalogEntry entry)
                || entry.Kind != MapCatalogEntryKind.WorldChildMap)
                continue;
            string path = getReadableMapDataPath(key);
            if (!worldPreviewCache.CanTake(key, path))
                paths.Add(path);
        }
        Dictionary<string, MapSnapshotRead> files = readMapSnapshotFiles(paths, default);
        foreach (string key in keys)
            receive(key, getMap(key, files));
    }

    private Dictionary<string, MapSnapshotRead> readMapSnapshotFiles(IEnumerable<string> paths, CancellationToken token)
    {
        string[] files = paths.Distinct(ProjectDiskSnapshot.PathComparer).ToArray();
        MapSnapshotRead[] reads = EditorValidationBatch.RunFiles(files, path =>
        {
            try
            {
                return new MapSnapshotRead(readMapFile(path, true, validateBlueprints: false));
            }
            catch (Exception exception) when (exception is not OperationCanceledException)
            {
                return new MapSnapshotRead(null, ExceptionDispatchInfo.Capture(exception));
            }
        }, token);
        Dictionary<string, MapSnapshotRead> result = new(files.Length, ProjectDiskSnapshot.PathComparer);
        for (int index = 0; index < files.Length; index++)
            result.Add(files[index], reads[index]);
        return result;
    }

    internal Dictionary<string, MapSnapshotRead> ReadMapSnapshotsWithoutCaching(
        IEnumerable<string> keys, CancellationToken token)
    {
        Dictionary<string, MapSnapshotRead> result = new(StringComparer.Ordinal);
        List<(string Key, string Path, MapCatalogEntry Catalog)> files = [];
        foreach (string key in keys)
        {
            token.ThrowIfCancellationRequested();
            if (mapDocuments.TryGetValue(key, out JsonObject? loaded))
                result[key] = new((JsonObject)loaded.DeepClone());
            else if (tryGetMapCatalogEntry(key, out MapCatalogEntry entry) && entry.Kind == MapCatalogEntryKind.WorldChildMap)
                files.Add((key, getReadableMapDataPath(key), entry));
            else
                result[key] = new(null);
        }
        Dictionary<string, MapSnapshotRead> reads = readMapSnapshotFiles(files.Select(file => file.Path), token);
        foreach ((string key, string path, MapCatalogEntry entry) in files)
        {
            token.ThrowIfCancellationRequested();
            MapSnapshotRead read = reads[path];
            try
            {
                if (read.Data is JsonObject map && (!store.tryValidateMapBlueprintData(path, map) || !mapMatchesCatalogEntry(map, entry)))
                    read = new(null);
            }
            catch (Exception exception) when (exception is not OperationCanceledException)
            {
                read = new(null, ExceptionDispatchInfo.Capture(exception));
            }
            result[key] = read;
        }
        return result;
    }
}
