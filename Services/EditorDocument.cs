using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class EditorDocument
{
    internal const int MaximumHistoryEntries = 100;
    internal readonly List<DocumentHistoryEntry> UndoEntries = [];
    internal readonly List<DocumentHistoryEntry> RedoEntries = [];
    internal EditorDocumentState SavedState;
    internal EditorDocumentState? PendingState;
    internal HistoryMarker? PendingMarker;
    internal string? PendingDescription;
    internal long PendingGestureId;
    internal Func<DocumentHistoryEntry, bool, HistoryResult>? HistoryRestorer;
    internal Action<EditorDocument>? StateRestored;
    internal Action<EditorDocument>? StateRestoring;
    internal Func<IReadOnlyDictionary<string, byte[]>>? SaveAdapter;
    private bool modified;

    internal EditorDocument(string section, string key, string path, JsonObject? data, bool isNew)
    {
        Id = Guid.NewGuid();
        Section = section;
        Key = key;
        Path = path;
        InternalData = data;
        SavedState = new EditorDocumentState(section, key, path, isNew ? null : data);
        UpdateModified();
    }

    public Guid Id { get; }
    public long Revision { get; internal set; }
    public string Section { get; internal set; }
    public string Key { get; internal set; }
    public string Path { get; internal set; }
    public string SavedPath => SavedState.Path;
    public JsonObject? Data => InternalData?.DeepClone() as JsonObject;
    internal JsonObject? InternalData { get; set; }
    public bool IsDeleted => InternalData is null;
    public bool Exists => !IsDeleted;
    public bool IsModified => modified;
    public bool CanAttemptUndo => UndoEntries.Count != 0;
    public bool CanUndo => CanAttemptUndo && UndoEntries[^1].Marker?.IsBarrier != true;
    public bool CanRedo => RedoEntries.Count != 0;
    public string? UndoBlockedReason => CanAttemptUndo && !CanUndo ? UndoEntries[^1].Marker?.Reason : null;
    public IReadOnlyList<DocumentHistoryEntry> History => UndoEntries.ToArray();
    public event EventHandler? Changed;

    internal EditorDocumentState CaptureState() => new(Section, Key, Path, InternalData);

    internal IReadOnlyDictionary<string, byte[]> PrepareSave()
    {
        return SaveAdapter?.Invoke()
            ?? throw new InvalidOperationException($"The document has no save adapter: {Path}");
    }

    internal void RestoreState(EditorDocumentState state)
    {
        StateRestoring?.Invoke(this);
        Section = state.Section;
        Key = state.Key;
        Path = state.Path;
        InternalData = state.Data;
        UpdateModified();
        StateRestored?.Invoke(this);
    }

    internal void NotifyChanged() => Changed?.Invoke(this, EventArgs.Empty);

    internal void UpdateModified()
    {
        modified = Section != SavedState.Section || Key != SavedState.Key || Path != SavedState.Path
            || !JsonNode.DeepEquals(InternalData, SavedState.InternalData);
    }

    internal static bool StatesEqual(EditorDocumentState left, EditorDocumentState right)
    {
        return left.Section == right.Section && left.Key == right.Key
            && left.Path == right.Path && JsonNode.DeepEquals(left.InternalData, right.InternalData);
    }
}
