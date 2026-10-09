using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;

namespace Ludork.Services;

internal sealed class ReferenceIndexSnapshotBuilder : IDisposable
{
    private readonly LuaMetadataService metadata;
    private readonly BlueprintClassResolver resolver;
    private IReadOnlyDictionary<string, JsonObject> blueprints = new Dictionary<string, JsonObject>();
    private Dictionary<(string Section, string Key), CachedDocument> cache = [];
    private ProjectEnumCatalog projectEnums = new(new Dictionary<string, JsonObject>(), [], []);
    private ReferenceBuildDocument[] catalogInputs = [];
    private ReferenceInputFiles? previousFiles;
    private long schemaVersion = -1;
    private long metadataRevision = -1;

    internal ReferenceIndexSnapshotBuilder(string projectPath)
    {
        metadata = new LuaMetadataService(projectPath, strictReads: true,
            enums: new LuaEnumService(projectPath, () => projectEnums));
        resolver = new BlueprintClassResolver(metadata, key => blueprints.TryGetValue(key, out JsonObject? value)
            ? (JsonObject)value.DeepClone() : null);
    }

    internal Result Build(ReferenceBuildInput input, CancellationToken token, IProgress<EditorOperationProgress>? progress = null)
    {
        ReferenceInputFiles files = ReferenceInputFiles.Capture(input.ProjectPath,
            input.Documents.Where(document => document.Data is null).Select(document => document.Path));
        ReferenceBuildDocument[] nextCatalogInputs = input.Documents
            .Where(document => document.Section is "General" or "Animations" or "Particles")
            .OrderBy(document => document.Section, StringComparer.Ordinal)
            .ThenBy(document => document.Key, StringComparer.Ordinal).ToArray();
        bool catalogChanged = catalogInputs.Length != nextCatalogInputs.Length
            || catalogInputs.Where((document, index) => document.Section != nextCatalogInputs[index].Section
                || document.Key != nextCatalogInputs[index].Key
                || document.Section == "General" && !ReferenceEquals(document.Data, nextCatalogInputs[index].Data)).Any();
        bool filesChanged = previousFiles is not null && !previousFiles.IsCurrent();
        projectEnums = new ProjectEnumCatalog(
            nextCatalogInputs.Where(document => document.Section == "General")
                .ToDictionary(document => document.Key, document => document.Data!, StringComparer.Ordinal),
            nextCatalogInputs.Where(document => document.Section == "Animations").Select(document => document.Key),
            nextCatalogInputs.Where(document => document.Section == "Particles").Select(document => document.Key));
        if (catalogChanged || filesChanged)
            metadata.ClearCache();
        bool rebuild = catalogChanged || filesChanged || schemaVersion != input.SchemaVersion || metadataRevision != metadata.Revision;
        if (rebuild)
            resolver.Invalidate();
        blueprints = input.Documents.Where(document => document.Section == "Blueprints" && document.Data is not null)
            .ToDictionary(document => document.Key, document => document.Data!, StringComparer.Ordinal);
        DocumentReferenceScanner scanner = new(metadata, resolver, () => projectEnums, token);
        Dictionary<(string Section, string Key), CachedDocument> next = [];
        Dictionary<string, ReferenceNode> nodes = new(StringComparer.Ordinal);
        HashSet<string> declared = new(StringComparer.Ordinal);
        Dictionary<string, string> members = new(StringComparer.Ordinal);
        List<ReferenceRecord> references = [];
        using IDisposable read = metadata.BeginReferenceRead();
        int completed = 0;
        foreach (ReferenceBuildDocument document in input.Documents)
        {
            token.ThrowIfCancellationRequested();
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_REFERENCES", document.Path, completed++, input.Documents.Count));
            (string Section, string Key) key = (document.Section, document.Key);
            ReferenceInputFiles.Stamp stamp = document.Data is null ? ReferenceInputFiles.Read(document.Path) : default;
            CachedDocument cached;
            if (!rebuild && cache.TryGetValue(key, out CachedDocument? previous)
                && ReferenceEquals(previous.Input.Data, document.Data) && previous.Input.Path == document.Path && sameCatalog(previous.Input.Catalog, document.Catalog) && previous.Stamp == stamp)
                cached = previous;
            else
            {
                JsonObject data = document.Data ?? JsonNode.Parse(File.ReadAllText(document.Path)) as JsonObject
                    ?? throw new InvalidDataException($"The indexed document could not be read: {document.Path}");
                if (document.Data is null && (document.Catalog is not MapCatalogEntry catalog
                    || data["type"]?.GetValue<string>() != "map" || !MapDataService.mapMatchesCatalogEntry(data, catalog)))
                    throw new InvalidDataException($"The indexed map no longer matches its catalog: {document.Path}");
                cached = new CachedDocument(document, stamp, scanner.Scan(document.Section, document.Key, data));
            }
            next[key] = cached;
            string type = ReferenceIndexSnapshot.DataRoots.Single(pair => pair.Value == document.Section).Key;
            string nodeKey = type == "blueprint" ? BlueprintReference.ToReference(document.Key) : document.Key;
            string id = ReferenceIdentity.NodeId(type, nodeKey);
            nodes[id] = ReferenceIndexSnapshot.ParseNode(id)!;
            declared.Add(id);
            foreach (KeyValuePair<string, string> member in cached.Result.GeneralMemberTypes)
                members[member.Key] = member.Value;
            foreach (ReferenceRecord reference in cached.Result.References)
            {
                references.Add(reference);
                nodes.TryAdd(reference.Source, ReferenceIndexSnapshot.ParseNode(reference.Source)!);
                nodes.TryAdd(reference.Target, ReferenceIndexSnapshot.ParseNode(reference.Target)!);
                if (reference.Kind == "member" && reference.Source == id && document.Section == "General")
                    declared.Add(reference.Target);
            }
        }
        token.ThrowIfCancellationRequested();
        if (!files.IsCurrent())
            throw new IOException("Reference inputs changed during indexing.");
        ReferenceIndexSnapshot snapshot = new(input.ProjectPath, input.Version, nodes.Values, references, members, declared);
        cache = next;
        catalogInputs = nextCatalogInputs;
        previousFiles = files;
        schemaVersion = input.SchemaVersion;
        metadataRevision = metadata.Revision;
        return new Result(snapshot, files);
    }

    private static bool sameCatalog(MapCatalogEntry? left, MapCatalogEntry? right)
        => ReferenceEquals(left, right) || left is not null && right is not null
            && left.Key == right.Key && left.Width == right.Width && left.Height == right.Height
            && left.LayerOrder.SequenceEqual(right.LayerOrder, StringComparer.Ordinal);

    public void Dispose() => resolver.Dispose();

    private sealed record CachedDocument(ReferenceBuildDocument Input, ReferenceInputFiles.Stamp Stamp, DocumentReferenceResult Result);
    internal sealed record Result(ReferenceIndexSnapshot Snapshot, ReferenceInputFiles Files);
}
