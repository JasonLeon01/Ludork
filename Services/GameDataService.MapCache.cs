using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using Ludork.Models;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    public JsonObject? ReadMapSnapshotWithoutCaching(string key)
    {
        key = normaliseMapKey(key);
        if (sections["Maps"].Data.TryGetValue(key, out JsonObject? loaded))
            return (JsonObject)loaded.DeepClone();
        if (!tryGetMapCatalogEntry(key, out MapCatalogEntry entry)
            || entry.Kind != MapCatalogEntryKind.WorldChildMap)
        {
            return null;
        }
        JsonObject? snapshot = readMapFile(getReadableMapDataPath(key), true);
        return snapshot is not null && mapMatchesCatalogEntry(snapshot, entry) ? snapshot : null;
    }

    public void NotifyMapActorsChanged(string mapKey)
    {
        mapKey = normaliseMapKey(mapKey);
        mapActorTagIndexes.Remove(mapKey);
        if (sections["Maps"].Data.TryGetValue(mapKey, out JsonObject? map)
            && tryGetMapCatalogEntry(mapKey, out MapCatalogEntry entry))
        {
            setMapCatalogEntry(entry with { ActorTags = readActorTags(map) });
        }
    }

    public void NotifyMapContentChanged(string mapKey)
    {
        mapKey = normaliseMapKey(mapKey);
        if (mapKey.Length != 0)
            MapPreviewChanged?.Invoke(this, new MapPreviewChangedEventArgs(mapKey));
    }

    public void NotifyAllMapPreviewsChanged()
    {
        MapPreviewChanged?.Invoke(this, new MapPreviewChangedEventArgs(null));
    }

    public string GetMapRuntimePath(string mapKey)
    {
        mapKey = normaliseMapKey(mapKey);
        return mapKey.Length == 0 ? string.Empty : mapKey + ".json";
    }

    private void touchMap(string key)
    {
        nextMapAccessOrder += 1;
        mapAccessOrder[key] = nextMapAccessOrder;
    }

    private void updateLoadedMapMetadata(
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

    private void removeLoadedMapMetadata(string key)
    {
        key = normaliseMapKey(key);
        mapAccessOrder.Remove(key);
        mapLoadedBytes.Remove(key);
        mapActorTagIndexes.Remove(key);
    }

    private void rekeyLoadedMapMetadata(string oldKey, string newKey)
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

    private void rekeyLoadedMapMetadataPrefix(string oldPrefix, string newPrefix)
    {
        foreach (string oldKey in mapAccessOrder.Keys
                     .Concat(mapLoadedBytes.Keys)
                     .Concat(mapActorTagIndexes.Keys)
                     .Where(key => keyMatchesPrefix(key, oldPrefix))
                     .Distinct(StringComparer.Ordinal)
                     .ToArray())
        {
            string suffix = oldKey.Length == oldPrefix.Length
                ? string.Empty
                : oldKey[oldPrefix.Length..];
            rekeyLoadedMapMetadata(oldKey, newPrefix + suffix);
        }
    }

    private void rebuildLoadedMapMetadata()
    {
        HashSet<string> loadedKeys = sections["Maps"].Data.Keys.ToHashSet(StringComparer.Ordinal);
        foreach (string key in mapAccessOrder.Keys.Where(key => !loadedKeys.Contains(key)).ToArray())
            mapAccessOrder.Remove(key);
        foreach (string key in mapLoadedBytes.Keys.Where(key => !loadedKeys.Contains(key)).ToArray())
            mapLoadedBytes.Remove(key);
        foreach (string key in mapActorTagIndexes.Keys.Where(key => !loadedKeys.Contains(key)).ToArray())
            mapActorTagIndexes.Remove(key);
        foreach (KeyValuePair<string, JsonObject> map in sections["Maps"].Data)
        {
            mapLoadedBytes[map.Key] = Encoding.UTF8.GetByteCount(map.Value.ToJsonString());
            if (!mapAccessOrder.ContainsKey(map.Key))
                touchMap(map.Key);
        }
    }

    private bool isMapHeldByHistory(string key)
    {
        return undoStack.Any(snapshot => snapshot["Maps"].ContainsKey(key))
            || redoStack.Any(snapshot => snapshot["Maps"].ContainsKey(key));
    }

    private bool isCleanLoadedMap(string key)
    {
        return originData["Maps"].TryGetValue(key, out JsonObject? origin)
            && nodesEqual(sections["Maps"].Data[key], origin);
    }
}

