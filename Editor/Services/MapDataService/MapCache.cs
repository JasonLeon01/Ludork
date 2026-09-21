using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
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

    internal void NotifyMapActorsChanged(string mapKey)
    {
        mapKey = normaliseMapKey(mapKey);
        mapActorTagIndexes.Remove(mapKey);
        if (mapDocuments.TryGetValue(mapKey, out JsonObject? map)
            && tryGetMapCatalogEntry(mapKey, out MapCatalogEntry entry))
        {
            setMapCatalogEntry(entry with { ActorTags = readActorTags(map) });
        }
    }

    internal void NotifyMapContentChanged(string mapKey, MapDataEditedEventArgs? edit = null)
    {
        mapKey = normaliseMapKey(mapKey);
        if (mapKey.Length != 0)
            store.Documents.AfterChangeNotifications(() =>
                MapPreviewChanged?.Invoke(store, new MapPreviewChangedEventArgs(mapKey, edit)));
    }

    internal void NotifyAllMapPreviewsChanged(bool reloadData = true)
    {
        store.Documents.AfterChangeNotifications(() =>
            MapPreviewChanged?.Invoke(store, new MapPreviewChangedEventArgs(null, reloadData: reloadData)));
    }

    public string GetMapRuntimePath(string mapKey)
    {
        mapKey = normaliseMapKey(mapKey);
        return mapKey.Length == 0 ? string.Empty : mapKey + ".json";
    }

    internal void touchMap(string key)
    {
        nextMapAccessOrder += 1;
        mapAccessOrder[key] = nextMapAccessOrder;
    }

    internal void updateLoadedMapMetadata(
        string key,
        JsonObject map,
        string? savedPath = null)
    {
        key = normaliseMapKey(key);
        long bytes = savedPath is not null && File.Exists(savedPath)
            ? new FileInfo(savedPath).Length
            : Encoding.UTF8.GetByteCount(map.ToJsonString());
        mapLoadedBytes[key] = bytes;
        touchMap(key);
    }

    internal void removeLoadedMapMetadata(string key)
    {
        key = normaliseMapKey(key);
        mapAccessOrder.Remove(key);
        mapLoadedBytes.Remove(key);
        mapActorTagIndexes.Remove(key);
    }

    internal void rekeyLoadedMapMetadata(string oldKey, string newKey)
    {
        oldKey = normaliseMapKey(oldKey);
        newKey = normaliseMapKey(newKey);
        if (mapAccessOrder.Remove(oldKey, out long accessOrder))
            mapAccessOrder[newKey] = accessOrder;
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
        foreach (string oldKey in mapAccessOrder.Keys
                     .Concat(mapLoadedBytes.Keys)
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
        foreach (string key in mapAccessOrder.Keys.Where(key => !loadedKeys.Contains(key)).ToArray())
            mapAccessOrder.Remove(key);
        foreach (string key in mapLoadedBytes.Keys.Where(key => !loadedKeys.Contains(key)).ToArray())
            mapLoadedBytes.Remove(key);
        foreach (string key in mapActorTagIndexes.Keys.Where(key => !loadedKeys.Contains(key)).ToArray())
            mapActorTagIndexes.Remove(key);
        foreach (KeyValuePair<string, JsonObject> map in mapDocuments)
        {
            mapLoadedBytes[map.Key] = Encoding.UTF8.GetByteCount(map.Value.ToJsonString());
            if (!mapAccessOrder.ContainsKey(map.Key))
                touchMap(map.Key);
        }
    }

    internal bool isMapHeldByHistory(string key)
    {
        return store.Documents.Find("Maps", key) is not null;
    }

    internal bool isCleanLoadedMap(string key)
    {
        return store.IsDocumentAtBaseline("Maps", key);
    }

    internal void ResetCacheMetadata()
    {
        mapActorTagIndexes.Clear();
        mapAccessOrder.Clear();
        mapLoadedBytes.Clear();
        nextMapAccessOrder = 0;
    }

    internal Action CaptureCacheState(string key)
    {
        Action restoreAccess = ProjectDataStore.captureExternalEntry(mapAccessOrder, key);
        Action restoreBytes = ProjectDataStore.captureExternalEntry(mapLoadedBytes, key);
        Action restoreTags = ProjectDataStore.captureExternalEntry(mapActorTagIndexes, key);
        return () =>
        {
            restoreTags();
            restoreBytes();
            restoreAccess();
        };
    }

}
