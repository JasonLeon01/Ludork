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
    private readonly Stack<(EditorDocumentTransaction Transaction, HashSet<EditorDocument> Changed)> transactions = [];
    private readonly StringComparison pathComparison = OperatingSystem.IsWindows()
        ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;

    public IReadOnlyList<EditorDocument> All => documents.ToArray();
    public IReadOnlyList<EditorDocument> ModifiedDocuments => documents.Where(document => document.IsModified).ToArray();
    public bool IsModified => documents.Any(document => document.IsModified);
    public event EventHandler? Changed;

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
        Notify(document);
    }

    internal string[] GetAdditionalPaths(EditorDocument document) => additionalPaths
        .Where(pair => ReferenceEquals(pair.Value, document)).Select(pair => pair.Key).ToArray();

    internal void RestoreRegistration(EditorDocument document, IReadOnlyList<string> aliases)
    {
        if (!documents.Contains(document))
            documents.Add(document);
        foreach (string alias in GetAdditionalPaths(document))
            additionalPaths.Remove(alias);
        foreach (string alias in aliases)
            additionalPaths[alias] = document;
    }

    internal long CreateGestureId() => ++nextGestureId;

    internal IEnumerable<EditorDocument> PendingDocuments => documents.Where(document => document.PendingState is not null);

    internal EditorDocument Register(string section, string key, string path, JsonObject? data, bool isNew = false)
    {
        EditorDocument? existing = Find(section, key) ?? FindByPath(path);
        if (existing is not null)
            return existing;
        EditorDocument document = new(section, key, Path.GetFullPath(path), data, isNew);
        documents.Add(document);
        Notify(document);
        return document;
    }

    internal void Capture(EditorDocument document, string? description = null, HistoryMarker? marker = null, long gestureId = 0)
    {
        if (document.PendingState is not null)
            return;
        document.PendingState = document.CaptureState();
        document.PendingDescription = description ?? "Edit " + document.Key;
        document.PendingMarker = marker;
        document.PendingGestureId = marker is null ? gestureId : 0;
    }

    internal bool Commit(EditorDocument document, JsonObject? data, string? key = null, string? path = null)
    {
        EditorDocumentState? before = document.PendingState;
        document.InternalData = data;
        document.Key = key ?? document.Key;
        document.Path = path is null ? document.Path : Path.GetFullPath(path);
        document.UpdateModified();
        if (before is null)
            return false;
        EditorDocumentState after = document.CaptureState();
        HistoryMarker? marker = document.PendingMarker;
        string? description = document.PendingDescription;
        long gestureId = document.PendingGestureId;
        ClearPending(document);
        if (EditorDocument.StatesEqual(before, after))
            return false;
        if (before.InternalData is null && after.InternalData is not null
            && document.SavedState.InternalData is null && document.UndoEntries.Count == 0)
        {
            document.RedoEntries.Clear();
            Notify(document);
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
        Notify(document);
        return true;
    }

    internal void MarkSaved(EditorDocument document)
    {
        if (document.UndoEntries.Count != 0)
            document.UndoEntries[^1] = document.UndoEntries[^1] with { GestureId = 0 };
        document.SavedState = document.CaptureState();
        document.UpdateModified();
        Notify(document);
    }

    internal void Restore(EditorDocument document, EditorDocumentState state)
    {
        document.RestoreState(state);
        ClearPending(document);
    }

    internal HistoryResult Undo(EditorDocument document) => Replay(document, true);
    internal HistoryResult Redo(EditorDocument document) => Replay(document, false);

    internal EditorDocumentTransaction BeginTransaction(IEnumerable<EditorDocument> affectedDocuments)
    {
        EditorDocumentTransaction transaction = new(this, affectedDocuments);
        transactions.Push((transaction, []));
        return transaction;
    }

    internal void Notify(EditorDocument document)
    {
        if (transactions.TryPeek(out (EditorDocumentTransaction Transaction, HashSet<EditorDocument> Changed) pending))
        {
            pending.Changed.Add(document);
            return;
        }
        document.Revision++;
        document.NotifyChanged();
        Changed?.Invoke(this, EventArgs.Empty);
    }

    internal void CompleteTransaction(EditorDocumentTransaction transaction, bool committed)
    {
        if (!transactions.TryPeek(out (EditorDocumentTransaction Transaction, HashSet<EditorDocument> Changed) current)
            || !ReferenceEquals(current.Transaction, transaction))
            throw new InvalidOperationException("Document transactions must complete in reverse opening order.");
        transactions.Pop();
        if (!committed)
            return;
        foreach (EditorDocument document in current.Changed)
            Notify(document);
    }

    internal void Clear()
    {
        documents.Clear();
        additionalPaths.Clear();
        Changed?.Invoke(this, EventArgs.Empty);
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
        return result;
    }

    private static void ClearPending(EditorDocument document)
    {
        document.PendingState = null;
        document.PendingDescription = null;
        document.PendingMarker = null;
        document.PendingGestureId = 0;
    }
}
