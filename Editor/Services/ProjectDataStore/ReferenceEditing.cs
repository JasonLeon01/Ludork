using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    public bool ApplyReferenceRewrites(IReadOnlyList<ReferenceRewrite> rewrites)
    {
        List<(EditorDocument Document, JsonObject Candidate)> prepared = [];
        HashSet<Guid> seen = [];
        foreach (ReferenceRewrite rewrite in rewrites)
        {
            EditorDocument document = GetDocument(rewrite.Section, rewrite.Key)
                ?? throw new InvalidDataException($"Invalid reference rewrite target: {rewrite.Section}/{rewrite.Key}");
            if (!seen.Add(document.Id) || !JsonNode.DeepEquals(document.InternalData, rewrite.Original))
                throw new InvalidOperationException($"The reference rewrite target changed: {rewrite.Section}/{rewrite.Key}");
            if (!JsonNode.DeepEquals(rewrite.Original, rewrite.Candidate))
                prepared.Add((document, (JsonObject)rewrite.Candidate.DeepClone()));
        }
        if (prepared.Count == 0)
            return false;
        using EditorDocumentTransaction transaction = Documents.BeginTransaction(prepared.Select(item => item.Document));
        foreach ((EditorDocument document, JsonObject candidate) in prepared)
        {
            RecordDocumentSnapshot(document.Section, document.Key, "Update references",
                new HistoryMarker(LocaleService.Get("DOCUMENT_HISTORY_REFERENCE_BARRIER")
                    .Replace("{source}", "references", StringComparison.Ordinal), IsBarrier: true));
            sections[document.Section][document.Key] = candidate;
            updateDocumentCatalog(document.Section, document.Key, document.Key, candidate);
        }
        CompleteDocumentChanges();
        transaction.Commit();
        refreshModifiedState();
        foreach ((EditorDocument document, _) in prepared.Where(item => item.Document.Section == "Maps"))
            Maps.NotifyMapContentChanged(document.Key);
        if (prepared.Any(item => item.Document.Section == "UI"))
            UiAssets.NotifyUiAssetsChanged();
        return true;
    }

}
