using Ludork.Models;
using System;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    public Task<bool> UpdateMapAsync(string currentKey, MapInfo info, IProgress<EditorOperationProgress>? progress = null)
        => store.UpdateMapAsync(currentKey, info, progress);

    internal long GetOperationMapBytes(string key) => mapLoadedBytes.TryGetValue(key, out long bytes)
        ? bytes : mapDocuments.TryGetValue(key, out System.Text.Json.Nodes.JsonObject? data) ? getMapSerializedByteCount(data) : 0;

    internal void ApplyOperationMapMetadata(string oldKey, string key, long bytes)
    {
        rekeyLoadedMapMetadata(oldKey, key);
        mapLoadedBytes[key] = bytes;
        touchMap(key);
    }
}
