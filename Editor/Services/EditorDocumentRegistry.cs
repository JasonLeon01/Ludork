using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class EditorDocumentRegistry
{
    private readonly List<EditorDocument> documents = [];
    private readonly Dictionary<string, EditorDocument> additionalPaths = new(OperatingSystem.IsWindows()
        ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
    private long nextGestureId;
    private readonly Stack<NotificationScope> notificationScopes = [];
    private readonly StringComparison pathComparison = OperatingSystem.IsWindows()
        ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;

    public IReadOnlyList<EditorDocument> All => documents.ToArray();
    public IReadOnlyList<EditorDocument> ModifiedDocuments => documents.Where(document => document.IsModified).ToArray();
    public bool IsModified => documents.Any(document => document.IsModified);
    public event EventHandler? Changed;
    internal event EventHandler<EditorDocumentsChangedEventArgs>? ContentInvalidated;
    public event EventHandler<EditorDocumentsChangedEventArgs>? ContentChanged;

    public EditorDocument? Find(string section, string key)
    {
        return documents.FirstOrDefault(document => document.Section == section && document.Key == key);
    }

    public EditorDocument? FindByPath(string path)
    {
        string fullPath = Path.GetFullPath(path);
        return documents.FirstOrDefault(document => string.Equals(document.Path, fullPath, pathComparison))
            ?? additionalPaths.GetValueOrDefault(fullPath);
    }

    public IReadOnlyList<string> GetPaths(EditorDocument document)
    {
        return new[] { document.Path }.Concat(GetAdditionalPaths(document))
            .Distinct(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal).ToArray();
    }

    internal void BindPath(EditorDocument document, string path) => additionalPaths[Path.GetFullPath(path)] = document;

    internal void Remove(EditorDocument document)
    {
        TrackDocument(document);
        EditorDocumentState before = document.CaptureState();
        if (!documents.Remove(document))
            return;
        foreach (string path in additionalPaths.Where(pair => ReferenceEquals(pair.Value, document))
                     .Select(pair => pair.Key).ToArray())
            additionalPaths.Remove(path);
        document.RestoreState(new EditorDocumentState(document.Section, document.Key, document.Path, null));
        document.SavedState = document.CaptureState();
        document.UndoEntries.Clear();
        document.RedoEntries.Clear();
        ClearPending(document);
        document.UpdateModified();
        Notify(document, before, document.CaptureState());
    }

    internal string[] GetAdditionalPaths(EditorDocument document) => additionalPaths
        .Where(pair => ReferenceEquals(pair.Value, document)).Select(pair => pair.Key).ToArray();

    internal void RestoreRegistration(EditorDocument document, IReadOnlyList<string> aliases, bool registered)
    {
        if (registered && !documents.Contains(document))
            documents.Add(document);
        else if (!registered)
            documents.Remove(document);
        foreach (string alias in GetAdditionalPaths(document))
            additionalPaths.Remove(alias);
        foreach (string alias in aliases)
            additionalPaths[alias] = document;
    }

    internal long CreateGestureId() => ++nextGestureId;

    internal bool IsRegistered(EditorDocument document) => documents.Contains(document);

    internal IEnumerable<EditorDocument> PendingDocuments => documents.Where(document => document.PendingState is not null);

    internal EditorDocument Register(string section, string key, string path, JsonObject? data, bool isNew = false)
    {
        EditorDocument? existing = Find(section, key) ?? FindByPath(path);
        if (existing is not null)
            return existing;
        EditorDocument document = new(section, key, Path.GetFullPath(path), data, isNew);
        TrackDocument(document);
        documents.Add(document);
        Notify(document);
        return document;
    }

    internal void Capture(EditorDocument document, string? description = null, HistoryMarker? marker = null, long gestureId = 0)
    {
        if (document.PendingState is not null)
            return;
        TrackDocument(document);
        document.PendingState = document.CaptureState();
        document.CurrentState = null;
        document.PendingDescription = description ?? "Edit " + document.Key;
        document.PendingMarker = marker;
        document.PendingGestureId = marker is null ? gestureId : 0;
    }

    internal bool Commit(EditorDocument document, JsonObject? data, string? key = null, string? path = null)
    {
        EditorDocumentState? before = document.PendingState;
        TrackDocument(document);
        document.CurrentState = null;
        document.InternalData = data;
        document.Key = key ?? document.Key;
        document.Path = path is null ? document.Path : Path.GetFullPath(path);
        if (before is null)
        {
            document.UpdateModified();
            return false;
        }
        EditorDocumentState after = document.CaptureState();
        HistoryMarker? marker = document.PendingMarker;
        string? description = document.PendingDescription;
        long gestureId = document.PendingGestureId;
        ClearPending(document);
        document.CurrentState = after;
        document.UpdateModified();
        if (EditorDocument.StatesEqual(before, after))
        {
            document.CurrentState = before;
            return false;
        }
        if (before.InternalData is null && after.InternalData is not null
            && document.SavedState.InternalData is null && document.UndoEntries.Count == 0)
        {
            document.RedoEntries.Clear();
            Notify(document, before, after);
            return true;
        }
        DocumentHistoryEntry entry = new(before, after, description, marker) { GestureId = gestureId };
        if (gestureId != 0 && document.UndoEntries.LastOrDefault() is DocumentHistoryEntry previous
            && previous.GestureId == gestureId && previous.Marker is null)
        {
            if (EditorDocument.StatesEqual(previous.Before, after))
                document.UndoEntries.RemoveAt(document.UndoEntries.Count - 1);
            else
                document.UndoEntries[^1] = entry with { Before = previous.Before };
        }
        else
        {
            document.UndoEntries.Add(entry);
            if (document.UndoEntries.Count > EditorDocument.MaximumHistoryEntries)
                document.UndoEntries.RemoveAt(0);
        }
        document.RedoEntries.Clear();
        Notify(document, before, after);
        return true;
    }

    internal void MarkSaved(EditorDocument document)
    {
        TrackDocument(document);
        if (document.UndoEntries.Count != 0)
            document.UndoEntries[^1] = document.UndoEntries[^1] with { GestureId = 0 };
        document.SavedState = document.CaptureState();
        document.UpdateModified();
        Notify(document);
    }

    internal void Restore(EditorDocument document, EditorDocumentState state)
    {
        TrackDocument(document);
        EditorDocumentState before = document.CaptureState();
        document.RestoreState(state);
        ClearPending(document);
        if (!EditorDocument.StatesEqual(before, state))
            Notify(document, before, state);
    }

    internal HistoryResult Undo(EditorDocument document) => Replay(document, true);
    internal HistoryResult Redo(EditorDocument document) => Replay(document, false);

    internal EditorDocumentTransaction BeginTransaction(IEnumerable<EditorDocument> affectedDocuments)
    {
        EditorDocument[] affected = affectedDocuments.Distinct().ToArray();
        foreach (EditorDocument document in affected)
            TrackDocument(document);
        EditorDocumentTransaction transaction = new(this, affected);
        notificationScopes.Push(new NotificationScope(transaction));
        return transaction;
    }

    internal EditorDocumentNotificationBatch BeginNotificationBatch()
    {
        EditorDocumentNotificationBatch batch = new(this);
        notificationScopes.Push(new NotificationScope(batch));
        return batch;
    }

    internal void AfterChangeNotifications(Action action)
    {
        if (notificationScopes.TryPeek(out NotificationScope? scope))
        {
            scope.AfterNotifications.Add(action);
            return;
        }
        action();
    }

    internal void Notify(EditorDocument document, EditorDocumentState? before = null, EditorDocumentState? after = null)
    {
        NotificationScope scope = notificationScopes.TryPeek(out NotificationScope? current)
            ? current : new NotificationScope(document);
        scope.Changed.Add(document);
        if (before is not null && after is not null)
            scope.AddContent(document, before, after);
        if (notificationScopes.Count == 0)
            Publish(scope);
    }

    internal void ValidateNotificationScope(object owner)
    {
        if (!notificationScopes.TryPeek(out NotificationScope? scope) || !ReferenceEquals(scope.Owner, owner))
            throw new InvalidOperationException("Document scopes must complete in reverse opening order.");
    }

    internal void CompleteNotificationScope(object owner, bool committed)
    {
        ValidateNotificationScope(owner);
        NotificationScope scope = notificationScopes.Pop();
        if (!committed)
            return;
        if (!notificationScopes.TryPeek(out NotificationScope? parent))
        {
            Publish(scope);
            return;
        }
        parent.Changed.UnionWith(scope.Changed);
        foreach (KeyValuePair<EditorDocument, (EditorDocumentState Before, EditorDocumentState After)> pair in scope.Content)
            parent.AddContent(pair.Key, pair.Value.Before, pair.Value.After);
        parent.Reset |= scope.Reset;
        parent.AfterNotifications.AddRange(scope.AfterNotifications);
    }

    internal void PublishReset()
    {
        if (notificationScopes.TryPeek(out NotificationScope? scope))
            scope.Reset = true;
        else
            Publish(new NotificationScope(this) { Reset = true });
    }

    private void TrackDocument(EditorDocument document)
    {
        foreach (NotificationScope scope in notificationScopes)
            if (scope.Owner is EditorDocumentTransaction transaction)
                transaction.Capture(document);
    }

    private void Publish(NotificationScope scope)
    {
        EditorDocumentChange[] changes = scope.Content.Select(pair => new EditorDocumentChange(
                pair.Key.Id, pair.Value.After.Section,
                pair.Value.Before.InternalData is null ? null : pair.Value.Before.Key,
                pair.Value.After.InternalData is null ? null : pair.Value.After.Key,
                pair.Value.Before.InternalData is null ? null : pair.Value.Before.Path,
                pair.Value.After.InternalData is null ? null : pair.Value.After.Path,
                !JsonNode.DeepEquals(pair.Value.Before.InternalData, pair.Value.After.InternalData)))
            .Where(change => change.ContentChanged || change.IdentityChanged).ToArray();
        EditorDocumentsChangedEventArgs? content = scope.Reset || changes.Length != 0
            ? new EditorDocumentsChangedEventArgs(changes, scope.Reset) : null;
        if (content is not null)
            ContentInvalidated?.Invoke(this, content);
        foreach (EditorDocument document in scope.Changed)
            document.Revision++;
        foreach (EditorDocument document in scope.Changed)
            document.NotifyChanged();
        if (scope.Changed.Count != 0 || scope.Reset)
            Changed?.Invoke(this, EventArgs.Empty);
        if (content is not null)
            ContentChanged?.Invoke(this, content);
        foreach (Action action in scope.AfterNotifications)
            action();
    }

    internal void Clear()
    {
        documents.Clear();
        additionalPaths.Clear();
        PublishReset();
    }

    private HistoryResult Replay(EditorDocument document, bool undo)
    {
        List<DocumentHistoryEntry> source = undo ? document.UndoEntries : document.RedoEntries;
        List<DocumentHistoryEntry> target = undo ? document.RedoEntries : document.UndoEntries;
        if (source.Count == 0)
            return new HistoryResult(false);
        DocumentHistoryEntry entry = source[^1];
        if (entry.Marker?.IsBarrier == true)
            return new HistoryResult(false, entry.Marker.Reason) { IsBlocked = true };
        using EditorDocumentTransaction transaction = BeginTransaction([document]);
        HistoryResult result;
        if (document.HistoryRestorer is not null)
        {
            result = document.HistoryRestorer(entry, undo);
            if (!result.Success)
                return result;
        }
        else
        {
            Restore(document, undo ? entry.Before : entry.After);
            result = new HistoryResult(true) { Changes = [document.Path] };
        }
        source.RemoveAt(source.Count - 1);
        target.Add(entry with { GestureId = 0 });
        Notify(document);
        transaction.Commit();
        return result;
    }

    private static void ClearPending(EditorDocument document)
    {
        document.PendingState = null;
        document.PendingDescription = null;
        document.PendingMarker = null;
        document.PendingGestureId = 0;
    }

    private sealed class NotificationScope(object owner)
    {
        public object Owner { get; } = owner;
        public HashSet<EditorDocument> Changed { get; } = [];
        public Dictionary<EditorDocument, (EditorDocumentState Before, EditorDocumentState After)> Content { get; } = [];
        public List<Action> AfterNotifications { get; } = [];
        public bool Reset { get; set; }

        public void AddContent(EditorDocument document, EditorDocumentState before, EditorDocumentState after)
        {
            Content[document] = (Content.TryGetValue(document, out (EditorDocumentState Before, EditorDocumentState After) current)
                ? current.Before : before, after);
        }
    }
}
