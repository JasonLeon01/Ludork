using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;

namespace Ludork.Services;

internal sealed class ReferenceIndexSnapshotBuilder
{
    private Dictionary<(string Section, string Key), CachedDocument> cache = [];
    private ReferenceBuildDocument[] catalogInputs = [];
    private ReferenceInputFiles? previousFiles;
    private long schemaVersion = -1;

    internal Result Build(ReferenceBuildInput input, CancellationToken token, IProgress<EditorOperationProgress>? progress = null)
    {
        ReferenceInputFiles files = ReferenceInputFiles.Capture(input.ProjectPath,
            input.Documents.Where(document => document.Data is null).Select(document => document.Path), includeStubs: true);
        ReferenceBuildDocument[] nextCatalogInputs = input.Documents
            .Where(document => document.Section is "General" or "Animations" or "Particles")
            .OrderBy(document => document.Section, StringComparer.Ordinal)
            .ThenBy(document => document.Key, StringComparer.Ordinal).ToArray();
        bool catalogChanged = catalogInputs.Length != nextCatalogInputs.Length
            || catalogInputs.Where((document, index) => document.Section != nextCatalogInputs[index].Section
                || document.Key != nextCatalogInputs[index].Key
                || document.Section == "General" && !ReferenceEquals(document.Data, nextCatalogInputs[index].Data)).Any();
        bool filesChanged = previousFiles is not null && !previousFiles.HasSameInputs(files);
        ProjectEnumCatalog projectEnums = new(
            nextCatalogInputs.Where(document => document.Section == "General")
                .ToDictionary(document => document.Key, document => document.Data!, StringComparer.Ordinal),
            nextCatalogInputs.Where(document => document.Section == "Animations").Select(document => document.Key),
            nextCatalogInputs.Where(document => document.Section == "Particles").Select(document => document.Key));
        bool rebuild = catalogChanged || filesChanged || schemaVersion != input.SchemaVersion;
        Dictionary<string, JsonObject> blueprints = input.Documents.Where(document => document.Section == "Blueprints" && document.Data is not null)
            .ToDictionary(document => document.Key, document => document.Data!, StringComparer.Ordinal);
        CachedDocument[] documents = new CachedDocument[input.Documents.Count];
        List<(int Index, ReferenceInputFiles.Stamp Stamp)> pending = [];
        for (int index = 0; index < input.Documents.Count; index++)
        {
            token.ThrowIfCancellationRequested();
            ReferenceBuildDocument document = input.Documents[index];
            (string Section, string Key) key = (document.Section, document.Key);
            ReferenceInputFiles.Stamp stamp = document.Data is null ? ReferenceInputFiles.Read(document.Path) : default;
            if (!rebuild && cache.TryGetValue(key, out CachedDocument? previous)
                && ReferenceEquals(previous.Input.Data, document.Data) && previous.Input.Path == document.Path
                && sameCatalog(previous.Input.Catalog, document.Catalog) && previous.Stamp == stamp)
                documents[index] = previous;
            else
                pending.Add((index, stamp));
        }
        int reused = documents.Length - pending.Count;
        progress?.Report(new EditorOperationProgress("EDIT_OPERATION_REFERENCES", Completed: reused, Total: documents.Length));
        DocumentReferenceResult[] scanned = ReferenceDocumentBatchScanner.Scan(input.ProjectPath,
            pending.Select(item => input.Documents[item.Index]).ToArray(), blueprints, projectEnums, token,
            completed => progress?.Report(new EditorOperationProgress("EDIT_OPERATION_REFERENCES",
                Completed: reused + completed, Total: documents.Length)));
        for (int index = 0; index < pending.Count; index++)
        {
            (int documentIndex, ReferenceInputFiles.Stamp stamp) = pending[index];
            documents[documentIndex] = new CachedDocument(input.Documents[documentIndex], stamp, scanned[index]);
        }
        Dictionary<(string Section, string Key), CachedDocument> next = [];
        Dictionary<string, ReferenceNode> nodes = new(StringComparer.Ordinal);
        HashSet<string> declared = new(StringComparer.Ordinal);
        Dictionary<string, string> members = new(StringComparer.Ordinal);
        List<ReferenceRecord> references = [];
        foreach (CachedDocument cached in documents)
        {
            token.ThrowIfCancellationRequested();
            ReferenceBuildDocument document = cached.Input;
            (string Section, string Key) key = (document.Section, document.Key);
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
        return new Result(snapshot, files);
    }

    private static bool sameCatalog(MapCatalogEntry? left, MapCatalogEntry? right)
        => ReferenceEquals(left, right) || left is not null && right is not null
            && left.Key == right.Key && left.Width == right.Width && left.Height == right.Height
            && left.LayerOrder.SequenceEqual(right.LayerOrder, StringComparer.Ordinal);

    private sealed record CachedDocument(ReferenceBuildDocument Input, ReferenceInputFiles.Stamp Stamp, DocumentReferenceResult Result);
    internal sealed record Result(ReferenceIndexSnapshot Snapshot, ReferenceInputFiles Files);
}
