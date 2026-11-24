using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.ExceptionServices;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ReferenceIndexService
{
    private static ReferenceBuildDocument createScanDocument(string section, string key, JsonObject data)
        => new(section, key, $"Data/{section}/{key}.json", data);

    private ReferenceBuildDocument? createMapScanDocument(MapCatalogEntry entry)
    {
        cancellationToken.ThrowIfCancellationRequested();
        string path = $"Data/Maps/{entry.Key}.json";
        if (gameData.GetReferenceSection("Maps").TryGetValue(entry.Key, out JsonObject? loaded))
            return new ReferenceBuildDocument("Maps", entry.Key, path, loaded, entry);
        return entry.Kind == MapCatalogEntryKind.WorldChildMap
            ? new ReferenceBuildDocument("Maps", entry.Key, path, null, entry) : null;
    }

    private DocumentReferenceResult[] scanDocuments(IReadOnlyList<ReferenceBuildDocument> documents)
    {
        if (documents.Count == 0)
            return [];
        Dictionary<string, JsonObject> blueprints = gameData.GetReferenceSection("Blueprints")
            .ToDictionary(pair => pair.Key, pair => pair.Value, StringComparer.Ordinal);
        ProjectEnumCatalog projectEnums = gameData.GetProjectEnumCatalog();
        Dictionary<string, MapDataService.MapSnapshotRead> maps = gameData.Maps.ReadMapSnapshotsWithoutCaching(
            documents.Where(document => document.Data is null).Select(document => document.Key), cancellationToken);
        if (maps.Count != 0)
            documents = documents.Select(document =>
            {
                if (document.Data is not null)
                    return document;
                MapDataService.MapSnapshotRead read = maps[document.Key];
                return document with
                {
                    Data = read.Data,
                    ReadError = read.Error ?? (read.Data is null ? ExceptionDispatchInfo.Capture(
                        new InvalidDataException($"The indexed world child map could not be read: {document.Key}.")) : null),
                };
            }).ToArray();
        return ReferenceDocumentBatchScanner.Scan(gameData.ProjectPath, documents, blueprints, projectEnums,
            cancellationToken, completed => progress?.Invoke(documents[completed - 1].Path));
    }

    private void applyScannedDocument(ReferenceBuildDocument document, DocumentReferenceResult result)
    {
        cancellationToken.ThrowIfCancellationRequested();
        foreach (KeyValuePair<string, string> member in result.GeneralMemberTypes)
            generalMemberTypes[member.Key] = member.Value;
        foreach (ReferenceRecord reference in result.References)
            addReference(reference.Source, reference.Target, reference.Kind, reference.Path,
                reference.IsDictionaryKey, reference.StoragePath);
        if (document.Catalog?.Kind == MapCatalogEntryKind.WorldChildMap)
        {
            string sourceId = ReferenceIdentity.NodeId("map", document.Key);
            mapReferenceCache[document.Key] = result.References.Where(reference => reference.Source == sourceId).ToArray();
        }
    }
}
