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
    internal MapHistoryEdits? PendingMapEdits;
    internal EditorDocumentState? CurrentState;
    internal Func<DocumentHistoryEntry, bool, HistoryResult>? HistoryRestorer;
    internal Action<EditorDocument>? StateRestored;
    internal Action<EditorDocument>? StateRestoring;
    internal Func<IReadOnlyDictionary<string, byte[]>>? SaveAdapter;
    internal Action<EditorDocument>? IdentityChanged;
    internal EditorHistoryGesture? Gesture;
    internal EditorDocumentLiveChange? LastChange { get; private set; }
    private bool modified;
    private int notificationDepth;

    internal EditorDocument(string section, string key, string path, JsonObject? data, bool isNew, Guid? id = null, EditorDocumentState? state = null)
    {
        Id = id ?? Guid.NewGuid();
        Section = section;
        Key = key;
        Path = path;
        InternalData = data;
        CurrentState = state ?? new EditorDocumentState(section, key, path, data);
        SavedState = isNew ? new EditorDocumentState(section, key, path, null) : CurrentState;
        UpdateModified();
    }

    public Guid Id { get; }
    public long Revision { get; internal set; }
    internal long LastContentChangeRevision { get; set; }
    public string Section { get; private set; }
    public string Key { get; private set; }
    public string Path { get; private set; }
    public string SavedPath => SavedState.Path;
    public JsonObject? Data => InternalData?.DeepClone() as JsonObject;
    internal JsonObject? InternalData { get; set; }
    public bool IsDeleted => InternalData is null;
    public bool Exists => !IsDeleted;
    public bool IsModified => modified;
    private bool HasGestureHistory => Gesture is { HasChanges: true, Before.InternalData: not null };
    public bool CanAttemptUndo => HasGestureHistory || UndoEntries.Count != 0;
    public bool CanUndo => HasGestureHistory || UndoEntries.Count != 0 && UndoEntries[^1].Marker?.IsBarrier != true;
    public bool CanRedo => RedoEntries.Count != 0;
    public string? UndoBlockedReason => CanAttemptUndo && !CanUndo ? UndoEntries[^1].Marker?.Reason : null;
    public IReadOnlyList<DocumentHistoryEntry> History
    {
        get
        {
            if (!HasGestureHistory || Gesture?.Before is not EditorDocumentState before)
                return UndoEntries.ToArray();
            EditorDocumentState after = CaptureState();
            return StatesEqual(before, after) ? UndoEntries.ToArray()
                : UndoEntries.Append(new DocumentHistoryEntry(before, after, Gesture.Description)
                {
                    MapEdits = Gesture.FreezeMapEdits(),
                }).TakeLast(MaximumHistoryEntries).ToArray();
        }
    }
    public event EventHandler? Changed;
    public event EventHandler? DiskReloaded;

    internal EditorDocumentState CaptureState()
    {
        return CurrentState ??= new EditorDocumentState(Section, Key, Path, InternalData);
    }

    internal IReadOnlyDictionary<string, byte[]> PrepareSave()
    {
        return SaveAdapter?.Invoke()
            ?? throw new InvalidOperationException($"The document has no save adapter: {Path}");
    }

    internal void RestoreState(EditorDocumentState state)
    {
        StateRestoring?.Invoke(this);
        SetIdentity(state.Section, state.Key, state.Path);
        InternalData = state.Data;
        CurrentState = state;
        UpdateModified();
        StateRestored?.Invoke(this);
    }

    internal void SetIdentity(string section, string key, string path)
    {
        if (Section == section && Key == key && Path == path)
            return;
        Section = section;
        Key = key;
        Path = path;
        IdentityChanged?.Invoke(this);
    }

    internal void NotifyChanged(EditorDocumentLiveChange? change = null)
    {
        EditorDocumentLiveChange? previous = LastChange;
        LastChange = change;
        notificationDepth++;
        try
        {
            Changed?.Invoke(this, EventArgs.Empty);
        }
        finally
        {
            notificationDepth--;
            if (notificationDepth != 0)
                LastChange = previous;
        }
    }
    internal void NotifyDiskReloaded() => DiskReloaded?.Invoke(this, EventArgs.Empty);

    internal void SetPreparedModified(bool value) => modified = value;

    internal void UpdateModified()
    {
        if (Gesture is { HasChanges: true })
        {
            modified = true;
            return;
        }
        if (ReferenceEquals(CurrentState, SavedState))
        {
            modified = false;
            return;
        }
        modified = Section != SavedState.Section || Key != SavedState.Key || Path != SavedState.Path
            || !JsonNode.DeepEquals(InternalData, SavedState.InternalData);
    }

    internal static bool StatesEqual(EditorDocumentState left, EditorDocumentState right)
    {
        return ReferenceEquals(left, right) || left.Section == right.Section && left.Key == right.Key
            && left.Path == right.Path && JsonNode.DeepEquals(left.InternalData, right.InternalData);
    }
}
