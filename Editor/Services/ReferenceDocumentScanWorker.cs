using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json.Nodes;
using System.Threading;

namespace Ludork.Services;

internal sealed class ReferenceDocumentScanWorker : IDisposable
{
    private readonly BlueprintClassResolver resolver;
    private readonly DocumentReferenceScanner scanner;
    private readonly IDisposable metadataRead;
    private readonly CancellationToken cancellationToken;

    internal ReferenceDocumentScanWorker(LuaValidationReadCache reads,
        IReadOnlyDictionary<string, JsonObject> blueprints, ProjectEnumCatalog projectEnums,
        CancellationToken cancellationToken)
    {
        this.cancellationToken = cancellationToken;
        LuaMetadataService metadata = reads.CreateMetadata(strictReads: true);
        metadataRead = metadata.BeginReferenceRead();
        resolver = new BlueprintClassResolver(metadata, key => blueprints.TryGetValue(key, out JsonObject? value)
            ? (JsonObject)value.DeepClone() : null);
        scanner = new DocumentReferenceScanner(metadata, resolver, () => projectEnums, cancellationToken);
    }

    internal DocumentReferenceResult Scan(ReferenceBuildDocument document)
    {
        cancellationToken.ThrowIfCancellationRequested();
        document.ReadError?.Throw();
        JsonObject data = document.Data ?? JsonNode.Parse(File.ReadAllText(document.Path)) as JsonObject
            ?? throw new InvalidDataException($"The indexed document could not be read: {document.Path}");
        if (document.Data is null && (document.Catalog is not MapCatalogEntry catalog
            || data["type"]?.GetValue<string>() != "map" || !MapDataService.mapMatchesCatalogEntry(data, catalog)))
            throw new InvalidDataException($"The indexed map no longer matches its catalog: {document.Path}");
        return scanner.Scan(document.Section, document.Key, data);
    }

    public void Dispose()
    {
        resolver.Dispose();
        metadataRead.Dispose();
    }
}
