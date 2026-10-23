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
    internal async Task<ProjectOperationSnapshot> CaptureOperationSnapshotAsync(
        IProgress<EditorOperationProgress>? progress = null)
    {
        BreakHistoryGesture();
        if (EditOperations.IsBusy)
        {
            using EditorDocumentNotificationBatch notifications = Documents.BeginNotificationBatch();
            EditorUiBatch preparation = new();
            foreach (EditorDocument document in Documents.PendingDocuments.ToArray())
            {
                if (document.Section == ProjectAssetPaths.Section)
                {
                    Documents.Commit(document, document.InternalData);
                    continue;
                }
                if (!sections.TryGetValue(document.Section, out EditorDocumentCollection? section))
                    continue;
                section.TryGetValue(document.Key, out JsonObject? data);
                Documents.Commit(document, data);
                progress?.Report(new EditorOperationProgress("EDIT_OPERATION_PREPARING", document.Path));
                await preparation.YieldIfNeededAsync();
            }
            notifications.Commit();
        }
        else
            CompleteDocumentChanges();
        long revision = Documents.Revision;
        bool busy = EditOperations.IsBusy;
        EditorDocument[] sources = Documents.All.Where(document => sections.ContainsKey(document.Section)
            || document.Section == ProjectAssetPaths.Section).ToArray();
        List<ProjectOperationDocument> documents = new(sources.Length);
        EditorUiBatch batch = new();
        foreach (EditorDocument document in sources)
        {
            documents.Add(new ProjectOperationDocument(document.Id, document.Revision,
                document.CaptureState(), document.SavedState,
                document.UndoEntries.ToArray(), document.RedoEntries.ToArray()));
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_PREPARING", document.Path,
                documents.Count, sources.Length));
            await batch.YieldIfNeededAsync();
            if (IsDisposed || Documents.Revision != revision || EditOperations.IsBusy != busy
                || Documents.HasPendingNotifications)
                throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        }
        Dictionary<string, string> moves = new(StringComparer.Ordinal);
        foreach (string key in sections["WorldMaps"].Keys)
            if (Worlds.TryGetPendingDirectoryMove(key, out string? directory))
                moves.Add(key, directory);
        return new ProjectOperationSnapshot(ProjectPath, revision, documents,
            Maps.MapCatalog.ToArray(), moves, InvalidLoadPaths, InvalidLoadErrors, deletedDocumentPaths.ToArray(),
            projectEnumGenerationPending);
    }

    internal static ProjectDataStore CreateOperationStore(ProjectOperationSnapshot snapshot)
    {
        ProjectDataStore store = new(snapshot.ProjectPath, false, default, null, false);
        foreach (string section in store.sections.Keys)
            store.originData[section] = new Dictionary<string, JsonObject>(StringComparer.Ordinal);
        foreach (ProjectOperationDocument entry in snapshot.Documents)
        {
            EditorDocumentState current = entry.Current;
            JsonObject? data = current.Data;
            bool assetPath = current.Section == ProjectAssetPaths.Section;
            if (!assetPath && data is not null)
                store.sections[current.Section][current.Key] = data;
            if (!assetPath && entry.Saved.InternalData is JsonObject saved)
                store.originData[entry.Saved.Section][entry.Saved.Key] = saved;
            EditorDocument document = store.Documents.Register(current.Section, current.Key,
                current.Path, data, entry.Saved.InternalData is null, entry.Id, current);
            if (assetPath)
                store.AssetPaths.Attach(document);
            else
                store.RegisterLoadedDocument(current.Section, current.Key);
            document.SavedState = entry.Saved;
            document.CurrentState = current;
            document.Revision = entry.Revision;
            document.UndoEntries.AddRange(entry.Undo);
            document.RedoEntries.AddRange(entry.Redo);
            document.UpdateModified();
        }
        foreach (MapCatalogEntry entry in snapshot.MapCatalog)
            store.Maps.setMapCatalogEntry(entry);
        store.originData["MapCatalog"] = store.sections["MapCatalog"].ToDictionary(
            pair => pair.Key, pair => (JsonObject)pair.Value.DeepClone(), StringComparer.Ordinal);
        foreach (KeyValuePair<string, string> move in snapshot.WorldMoves)
            store.Worlds.UpdatePendingDirectoryMove(move.Key, move.Key, move.Value);
        store.invalidLoadPaths.AddRange(snapshot.InvalidPaths);
        foreach (KeyValuePair<string, string> error in snapshot.InvalidErrors)
            store.invalidLoadErrors[error.Key] = error.Value;
        store.deletedDocumentPaths.UnionWith(snapshot.DeletedPaths);
        store.projectEnumGenerationPending = snapshot.GenerateEnums;
        store.AssetPaths.Refresh();
        return store;
    }
}
