using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Services;

public sealed class EditorDocumentTransaction : IDisposable
{
    private readonly EditorDocumentRegistry registry;
    private readonly List<Snapshot> snapshots;
    private bool completed;

    internal EditorDocumentTransaction(EditorDocumentRegistry registry, IEnumerable<EditorDocument> documents)
    {
        this.registry = registry;
        snapshots = documents.Distinct().Select(document => new Snapshot(
            document, document.CaptureState(), document.SavedState, document.PendingState,
            document.PendingMarker, document.PendingDescription, document.PendingGestureId,
            document.UndoEntries.ToArray(), document.RedoEntries.ToArray(), document.Revision,
            registry.GetAdditionalPaths(document))).ToList();
    }

    public void Commit()
    {
        if (completed)
            return;
        completed = true;
        registry.CompleteTransaction(this, true);
    }

    public void Dispose()
    {
        if (completed)
            return;
        completed = true;
        foreach (Snapshot snapshot in snapshots)
        {
            EditorDocument document = snapshot.Document;
            registry.RestoreRegistration(document, snapshot.Aliases);
            document.SavedState = snapshot.Saved;
            document.RestoreState(snapshot.Current);
            document.PendingState = snapshot.Pending;
            document.PendingMarker = snapshot.Marker;
            document.PendingDescription = snapshot.Description;
            document.PendingGestureId = snapshot.GestureId;
            document.UndoEntries.Clear();
            document.UndoEntries.AddRange(snapshot.Undo);
            document.RedoEntries.Clear();
            document.RedoEntries.AddRange(snapshot.Redo);
            document.UpdateModified();
            document.Revision = snapshot.Revision;
        }
        registry.CompleteTransaction(this, false);
    }

    private sealed record Snapshot(EditorDocument Document, EditorDocumentState Current,
        EditorDocumentState Saved, EditorDocumentState? Pending, HistoryMarker? Marker,
        string? Description, long GestureId, DocumentHistoryEntry[] Undo, DocumentHistoryEntry[] Redo, long Revision,
        string[] Aliases);
}
