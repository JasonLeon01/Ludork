using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    public JsonObject? ReadMapSnapshotWithoutCaching(string key)
    {
        key = normaliseMapKey(key);
        if (mapDocuments.TryGetValue(key, out JsonObject? loaded))
            return (JsonObject)loaded.DeepClone();
        if (!tryGetMapCatalogEntry(key, out MapCatalogEntry entry)
            || entry.Kind != MapCatalogEntryKind.WorldChildMap)
        {
            return null;
        }
        JsonObject? snapshot = readMapFile(getReadableMapDataPath(key), true);
        return snapshot is not null && mapMatchesCatalogEntry(snapshot, entry) ? snapshot : null;
    }

    internal void NotifyMapActorsChanged(string mapKey, MapDataEditedEventArgs? edit = null)
    {
        mapKey = normaliseMapKey(mapKey);
        bool preserveTags = edit?.ChangesOnlyActorOverrides == true;
        if (!preserveTags)
            mapActorTagIndexes.Remove(mapKey);
        if (mapDocuments.TryGetValue(mapKey, out JsonObject? map)
            && tryGetMapCatalogEntry(mapKey, out MapCatalogEntry entry))
        {
            setMapCatalogEntry(entry with
            {
                ActorTags = preserveTags ? entry.ActorTags : readActorTags(map),
                PreviewLightSources = MapPreviewCatalogCapture.Extract(map),
            });
        }
    }

    internal void NotifyMapContentChanged(string mapKey, MapDataEditedEventArgs? edit = null)
    {
        mapKey = normaliseMapKey(mapKey);
        if (mapKey.Length != 0)
        {
            worldPreviewCache.Invalidate(mapKey);
            store.Documents.AfterChangeNotifications(() =>
                MapPreviewChanged?.Invoke(store, new MapPreviewChangedEventArgs(mapKey, edit)));
        }
    }

    internal void NotifyAllMapPreviewsChanged(bool reloadData = true)
    {
        if (reloadData)
            worldPreviewCache.Clear();
        store.Documents.AfterChangeNotifications(() =>
            MapPreviewChanged?.Invoke(store, new MapPreviewChangedEventArgs(null, reloadData: reloadData)));
    }

    public string GetMapRuntimePath(string mapKey)
    {
        mapKey = normaliseMapKey(mapKey);
        return mapKey.Length == 0 ? string.Empty : mapKey + ".json";
    }

    internal void updateLoadedMapMetadata(
        string key,
        JsonObject map,
        string? savedPath = null)
    {
        key = normaliseMapKey(key);
        long bytes = savedPath is not null && File.Exists(savedPath)
            ? new FileInfo(savedPath).Length
            : getMapSerializedByteCount(map);
        mapLoadedBytes[key] = bytes;
    }

    internal void removeLoadedMapMetadata(string key)
    {
        key = normaliseMapKey(key);
        worldPreviewCache.Invalidate(key);
        mapLoadedBytes.Remove(key);
        mapActorTagIndexes.Remove(key);
    }

    internal void rekeyLoadedMapMetadata(string oldKey, string newKey)
    {
        oldKey = normaliseMapKey(oldKey);
        newKey = normaliseMapKey(newKey);
        worldPreviewCache.Invalidate(oldKey);
        if (newKey != oldKey)
            worldPreviewCache.Invalidate(newKey);
        if (mapLoadedBytes.Remove(oldKey, out long bytes))
            mapLoadedBytes[newKey] = bytes;
        if (mapActorTagIndexes.Remove(
                oldKey,
                out Dictionary<string, List<MapActorTagLocation>>? actorTags))
        {
            mapActorTagIndexes[newKey] = actorTags;
        }
    }

    internal void rekeyLoadedMapMetadataPrefix(string oldPrefix, string newPrefix)
    {
        worldPreviewCache.InvalidatePrefix(oldPrefix);
        worldPreviewCache.InvalidatePrefix(newPrefix);
        foreach (string oldKey in mapLoadedBytes.Keys
                     .Concat(mapActorTagIndexes.Keys)
                     .Where(key => ProjectDataStore.keyMatchesPrefix(key, oldPrefix))
                     .Distinct(StringComparer.Ordinal)
                     .ToArray())
        {
            string suffix = oldKey.Length == oldPrefix.Length
                ? string.Empty
                : oldKey[oldPrefix.Length..];
            rekeyLoadedMapMetadata(oldKey, newPrefix + suffix);
        }
    }

    internal void rebuildLoadedMapMetadata()
    {
        HashSet<string> loadedKeys = mapDocuments.Keys.ToHashSet(StringComparer.Ordinal);
        foreach (string key in mapLoadedBytes.Keys.Where(key => !loadedKeys.Contains(key)).ToArray())
            mapLoadedBytes.Remove(key);
        foreach (string key in mapActorTagIndexes.Keys.Where(key => !loadedKeys.Contains(key)).ToArray())
            mapActorTagIndexes.Remove(key);
        foreach (KeyValuePair<string, JsonObject> map in mapDocuments)
            mapLoadedBytes[map.Key] = getMapSerializedByteCount(map.Value);
    }

    private static long getMapSerializedByteCount(JsonObject map)
    {
        using JsonByteCountingBuffer buffer = new();
        using Utf8JsonWriter writer = new(buffer);
        map.WriteTo(writer);
        writer.Flush();
        return buffer.Count;
    }

    internal void ResetCacheMetadata()
    {
        worldPreviewCache.Clear();
        mapActorTagIndexes.Clear();
        mapLoadedBytes.Clear();
    }

    internal Action CaptureCacheState(string key)
    {
        Action restoreBytes = ProjectDataStore.captureExternalEntry(mapLoadedBytes, key);
        Action restoreTags = ProjectDataStore.captureExternalEntry(mapActorTagIndexes, key);
        return () =>
        {
            restoreTags();
            restoreBytes();
            worldPreviewCache.InvalidatePrefix(key);
        };
    }

    internal void InvalidateWorldChildPreview(string key) => worldPreviewCache.Invalidate(normaliseMapKey(key));

}
