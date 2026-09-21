using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    private readonly ProjectDataStore store;
    private readonly EditorDocumentCollection mapDocuments;
    private readonly EditorDocumentCollection catalogDocuments;

    internal MapDataService(ProjectDataStore store, EditorDocumentCollection mapDocuments, EditorDocumentCollection catalogDocuments)
    {
        this.store = store;
        this.mapDocuments = mapDocuments;
        this.catalogDocuments = catalogDocuments;

        mapData = new LazyMapDataDictionary(store);
    }

    private readonly LazyMapDataDictionary mapData;

    private readonly Dictionary<string, long> mapAccessOrder = new(StringComparer.Ordinal);

    private readonly Dictionary<string, long> mapLoadedBytes = new(StringComparer.Ordinal);

    private readonly Dictionary<string, Dictionary<string, List<MapActorTagLocation>>> mapActorTagIndexes = new(StringComparer.Ordinal);

    private readonly Dictionary<string, JsonObject> loadedMapCatalogCache = new(StringComparer.Ordinal);

    private readonly Dictionary<string, JsonObject> nextMapCatalogCache = new(StringComparer.Ordinal);

    private long nextMapAccessOrder;

    public event EventHandler<MapPreviewChangedEventArgs>? MapPreviewChanged;

    public IReadOnlyDictionary<string, MapDocumentSnapshot> MapData => mapData;

    public IReadOnlyDictionary<string, MapDocumentSnapshot> LoadedMapData => new DocumentSnapshotDictionary<MapDocumentSnapshot>(mapDocuments, MapDocumentCodec.Decode);

    public IReadOnlyList<MapCatalogEntry> MapCatalog => getMapCatalogEntries();

}
