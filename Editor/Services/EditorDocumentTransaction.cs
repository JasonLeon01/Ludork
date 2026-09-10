using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Services;

public sealed class EditorDocumentTransaction : IDisposable
{
    private readonly EditorDocumentRegistry registry;
    private readonly Dictionary<EditorDocument, Snapshot> snapshots = [];
    private bool completed;

    internal EditorDocumentTransaction(EditorDocumentRegistry registry, IEnumerable<EditorDocument> documents)
    {
        this.registry = registry;
        foreach (EditorDocument document in documents)
            Capture(document);
    }

    internal void Capture(EditorDocument document)
    {
        if (snapshots.ContainsKey(document))
            return;
        snapshots.Add(document, new Snapshot(
            document, registry.IsRegistered(document), document.CaptureState(), document.SavedState, document.PendingState,
            document.PendingMarker, document.PendingDescription, document.PendingGestureId,
            document.UndoEntries.ToArray(), document.RedoEntries.ToArray(), document.Revision,
            registry.GetAdditionalPaths(document)));
    }

    public void Commit()
    {
        if (completed)
            return;
        registry.ValidateNotificationScope(this);
        completed = true;
        registry.CompleteNotificationScope(this, true);
    }

    public void Dispose()
    {
        if (completed)
            return;
        registry.ValidateNotificationScope(this);
        completed = true;
        foreach (Snapshot snapshot in snapshots.Values)
        {
            EditorDocument document = snapshot.Document;
            registry.RestoreRegistration(document, snapshot.Aliases, snapshot.Registered);
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
        registry.CompleteNotificationScope(this, false);
    }

    private sealed record Snapshot(EditorDocument Document, bool Registered, EditorDocumentState Current,
        EditorDocumentState Saved, EditorDocumentState? Pending, HistoryMarker? Marker,
        string? Description, long GestureId, DocumentHistoryEntry[] Undo, DocumentHistoryEntry[] Redo, long Revision,
        string[] Aliases);
}
