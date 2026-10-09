using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    internal ProjectOperationSnapshot CaptureOperationSnapshot()
    {
        BreakHistoryGesture();
        CompleteDocumentChanges();
        ProjectOperationDocument[] documents = Documents.All
            .Where(document => sections.ContainsKey(document.Section))
            .Select(document => new ProjectOperationDocument(document.Id, document.Revision,
                document.CaptureState(), document.SavedState,
                document.UndoEntries.ToArray(), document.RedoEntries.ToArray())).ToArray();
        Dictionary<string, string> moves = new(StringComparer.Ordinal);
        foreach (string key in sections["WorldMaps"].Keys)
            if (Worlds.TryGetPendingDirectoryMove(key, out string? directory))
                moves.Add(key, directory);
        return new ProjectOperationSnapshot(ProjectPath, Documents.Revision, documents,
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
            if (data is not null)
                store.sections[current.Section][current.Key] = data;
            if (entry.Saved.InternalData is JsonObject saved)
                store.originData[entry.Saved.Section][entry.Saved.Key] = saved;
            EditorDocument document = store.Documents.Register(current.Section, current.Key,
                current.Path, data, entry.Saved.InternalData is null, entry.Id, current);
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
        return store;
    }
}
