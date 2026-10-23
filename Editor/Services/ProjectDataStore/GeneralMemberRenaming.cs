using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    public Task<bool> RenameGeneralMemberAsync(string typeKey, string oldId, string newId,
        IProgress<EditorOperationProgress>? progress = null)
        => runResourceOperationAsync(store => store.RenameGeneralMember(typeKey, oldId, newId),
            result => result, progress);

    internal bool RenameGeneralMember(string typeKey, string oldId, string newId)
    {
        BreakHistoryGesture();
        CompleteDocumentChanges();
        return commitGeneralMemberRename(typeKey, oldId, newId);
    }

    private bool commitGeneralMemberRename(string typeKey, string oldId, string newId, JsonObject? restored = null)
    {
        EditorDocument? source = GetDocument("General", typeKey);
        if (string.IsNullOrWhiteSpace(newId) || source?.InternalData is not JsonObject original
            || original["members"] is not JsonObject members || !members.ContainsKey(oldId) || members.ContainsKey(newId))
            return false;
        IReadOnlyList<ReferenceRewrite> rewrites = ReferenceIndex.PrepareGeneralMemberRename(typeKey, oldId, newId);
        Dictionary<EditorDocument, (string Key, JsonObject Data)> changes = new()
        {
            [source] = (source.Key, (JsonObject)(restored ?? original).DeepClone()),
        };
        foreach (ReferenceRewrite rewrite in rewrites)
        {
            EditorDocument target = GetDocument(rewrite.Section, rewrite.Key)
                ?? throw new InvalidDataException($"The referenced document could not be loaded: {rewrite.Section}/{rewrite.Key}");
            if (!JsonNode.DeepEquals(target.InternalData, rewrite.Original))
                throw new InvalidOperationException($"The reference rewrite target changed: {rewrite.Section}/{rewrite.Key}");
            if (!ReferenceEquals(target, source) || restored is null)
                changes[target] = (target.Key, (JsonObject)rewrite.Candidate.DeepClone());
        }
        if (restored is null)
        {
            JsonObject nextMembers = (JsonObject)changes[source].Data["members"]!;
            GeneralDataService.replaceGeneralObjectKey(nextMembers, oldId, newId, nextMembers[oldId]);
        }
        validateBlueprintResourceChanges(changes);
        using EditorDocumentTransaction transaction = Documents.BeginTransaction(changes.Keys);
        foreach (KeyValuePair<EditorDocument, (string Key, JsonObject Data)> change in changes)
        {
            EditorDocument document = change.Key;
            if (ReferenceEquals(document, source) && restored is not null)
                RestoreDocumentState(document, new EditorDocumentState(document.Section, document.Key, document.Path, change.Value.Data));
            else
            {
                HistoryMarker? marker = ReferenceEquals(document, source) ? null
                    : new HistoryMarker(LocaleService.Get("DOCUMENT_HISTORY_REFERENCE_BARRIER")
                        .Replace("{source}", source.Path, StringComparison.Ordinal), source.Id, source.Path, true);
                RecordDocumentSnapshot(document.Section, document.Key, $"Rename {oldId} to {newId}", marker);
                sections[document.Section][document.Key] = change.Value.Data;
            }
            updateDocumentCatalog(document.Section, document.Key, document.Key, change.Value.Data);
        }
        CompleteDocumentChanges();
        if (restored is null)
            source.UndoEntries[^1] = source.UndoEntries[^1] with { GeneralMemberRename = new(oldId, newId) };
        transaction.Commit();
        refreshModifiedState();
        foreach (EditorDocument document in changes.Keys.Where(document => document.Section == "Maps"))
            Maps.NotifyMapContentChanged(document.Key);
        if (changes.Keys.Any(document => document.Section == "UI"))
            UiAssets.NotifyUiAssetsChanged();
        return true;
    }
}
