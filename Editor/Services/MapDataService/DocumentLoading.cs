using Ludork.Models;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    internal void LoadStandaloneMap(string key, JsonObject map, long bytes)
    {
        mapDocuments[key] = map;
        mapLoadedBytes[key] = bytes;
        touchMap(key);
        setMapCatalogEntry(createMapCatalogEntry(key, MapCatalogEntryKind.StandaloneMap, null, map));
    }

    internal void CreateChildMapDocument(string key, string worldKey, JsonObject map)
    {
        mapDocuments.RecordChange(key);
        mapDocuments[key] = map;
        updateLoadedMapMetadata(key, map);
        setMapCatalogEntry(createMapCatalogEntry(key, MapCatalogEntryKind.WorldChildMap, worldKey, map));
        store.refreshModifiedState();
    }
}
