using System;
using System.Linq;

namespace Ludork.Services;

public sealed partial class EditorDocumentRegistry
{
    private EditorHistoryGesture? activeGesture;
    internal Action? CompletePendingChanges { get; set; }
    internal Action? HistoryGestureCompleted { get; set; }
    internal long ActiveGestureId => activeGesture?.Id ?? 0;

    internal long BeginHistoryGesture(Action? flushPending = null)
    {
        if (notificationScopes.Any(scope => scope.Owner is EditorDocumentTransaction))
            throw new InvalidOperationException("History gestures cannot start inside a document transaction.");
        BreakHistoryGesture();
        if (activeGesture is { Completing: true })
            return activeGesture.Id;
        activeGesture = new EditorHistoryGesture(++nextGestureId, flushPending);
        return activeGesture.Id;
    }

    internal bool IsHistoryGestureActive(long id) => id != 0 && activeGesture?.Id == id;

    internal void PrepareGestureDocument(EditorDocument document)
    {
        if (activeGesture?.Document is EditorDocument previous && !ReferenceEquals(previous, document))
            BreakHistoryGesture();
    }

    internal void SetHistoryGestureFlush(long id, Action flushPending)
    {
        if (activeGesture?.Id == id)
            activeGesture.FlushPending = flushPending;
    }

    internal void MarkHistoryGestureChanged(long id, EditorDocument document)
    {
        if (activeGesture is not EditorHistoryGesture gesture || gesture.Id != id || gesture.HasChanges)
            return;
        PrepareGestureDocument(document);
        if (!ReferenceEquals(activeGesture, gesture))
            return;
        gesture.Document = document;
        gesture.Before ??= document.CaptureState();
        gesture.Description ??= "Edit " + document.Key;
        gesture.HasChanges = true;
        gesture.HasMapEdits = false;
        document.Gesture = gesture;
        document.RedoEntries.Clear();
        document.SetPreparedModified(true);
        Notify(document);
        HistoryGestureCompleted?.Invoke();
    }

    internal void EndHistoryGesture(long id)
    {
        if (activeGesture?.Id == id)
            BreakHistoryGesture();
    }

    internal void BreakHistoryGesture()
    {
        if (activeGesture is not EditorHistoryGesture gesture || gesture.Completing)
            return;
        gesture.Completing = true;
        try
        {
            gesture.FlushPending?.Invoke();
            CompletePendingChanges?.Invoke();
            EditorDocument? document = gesture.Document;
            if (document is not null)
            {
                document.Gesture = null;
                if (gesture.HasChanges && gesture.Before is EditorDocumentState before)
                {
                    EditorDocumentState after = document.CaptureState();
                    if (EditorDocument.StatesEqual(before, after))
                        document.CurrentState = before;
                    else if (!(before.InternalData is null && after.InternalData is not null
                        && document.SavedState.InternalData is null && document.UndoEntries.Count == 0))
                    {
                        document.UndoEntries.Add(new DocumentHistoryEntry(before, after, gesture.Description)
                        {
                            MapEdits = gesture.FreezeMapEdits(),
                        });
                        if (document.UndoEntries.Count > EditorDocument.MaximumHistoryEntries)
                            document.UndoEntries.RemoveAt(0);
                    }
                    document.UpdateModified();
                }
            }
            activeGesture = null;
            if (document is not null && gesture.HasChanges)
                Notify(document);
            HistoryGestureCompleted?.Invoke();
        }
        finally
        {
            gesture.Completing = false;
        }
    }

    private void NotifyLive(EditorDocument document, EditorDocumentLiveChange change)
    {
        NotificationScope scope = notificationScopes.TryPeek(out NotificationScope? current)
            ? current : new NotificationScope(document);
        scope.Changed.Add(document);
        scope.AddLive(document, change);
        if (notificationScopes.Count == 0)
            Publish(scope);
    }
}
