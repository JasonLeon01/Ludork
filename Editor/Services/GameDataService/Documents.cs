using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    private Guid? activeHistoryDocumentId;

    public EditorDocumentRegistry Documents { get; } = new();

    public EditorDocument? GetDocument(string section, string key)
    {
        key = normalizeJsonKey(key);
        EditorDocument? document = Documents.Find(section, key);
        if (document is null && sections.TryGetValue(section, out DataSection? requestedSection) && requestedSection.Persist)
            document = Documents.FindByPath(getSectionDataPath(section, key));
        if (document is not null)
            return document;
        if (section == "Maps")
        {
            key = getAllMapKeys().FirstOrDefault(candidate => getPathComparer().Equals(candidate, key)) ?? key;
            getMap(key);
        }
        return sections.TryGetValue(section, out DataSection? dataSection)
            && dataSection.Persist && dataSection.Data.ContainsKey(key)
                ? RegisterLoadedDocument(section, key) : null;
    }

    public EditorDocument? GetDocumentByPath(string path)
    {
        EditorDocument? existing = Documents.FindByPath(path);
        if (existing is not null)
            return existing;
        if (!tryGetDataLocation(path, out string section, out string relative))
            return null;
        string key = section == "WorldMaps" ? relative : normalizeJsonKey(relative);
        return GetDocument(section, key);
    }

    private void InitializeDocuments()
    {
        foreach (EditorDocument document in Documents.All.Where(document => document.Section == "Maps"))
        {
            if (containsMapKey(document.Key))
                getMap(document.Key);
        }
        Dictionary<(string Section, string Key), JsonObject> loaded = sections
            .Where(pair => pair.Value.Persist)
            .SelectMany(section => section.Value.Data.Select(pair =>
                new KeyValuePair<(string Section, string Key), JsonObject>((section.Key, pair.Key), pair.Value)))
            .ToDictionary(pair => pair.Key, pair => pair.Value);
        EditorDocument[] existing = Documents.All.Where(document => sections.ContainsKey(document.Section)).ToArray();
        using EditorDocumentTransaction transaction = Documents.BeginTransaction(existing);
        foreach (EditorDocument document in existing)
        {
            if (!loaded.TryGetValue((document.Section, document.Key), out JsonObject? data))
            {
                Documents.Remove(document);
                continue;
            }
            Documents.Restore(document, new EditorDocumentState(document.Section, document.Key,
                getSectionDataPath(document.Section, document.Key), data));
            document.UndoEntries.Clear();
            document.RedoEntries.Clear();
            Documents.MarkSaved(document);
        }
        foreach ((string section, string key) in loaded.Keys)
            RegisterLoadedDocument(section, key);
        transaction.Commit();
    }

    private EditorDocument RegisterLoadedDocument(string section, string key)
    {
        key = normalizeJsonKey(key);
        sections[section].Data.TryGetValue(key, out JsonObject? data);
        EditorDocument document = Documents.Register(section, key, getSectionDataPath(section, key), data,
            !originData.TryGetValue(section, out Dictionary<string, JsonObject>? origin) || !origin.ContainsKey(key));
        document.HistoryRestorer = (entry, undo) => restoreDocumentHistory(document, entry, undo);
        document.SaveAdapter = () => SerializeDocument(document);
        document.StateRestoring = removeDocumentIndex;
        document.StateRestored = synchronizeDocumentIndex;
        return document;
    }

    private void RecordDocumentSnapshot(string section, string key, string? description = null, HistoryMarker? marker = null)
    {
        key = normalizeJsonKey(key);
        EditorDocument document = GetDocument(section, key) ?? RegisterLoadedDocument(section, key);
        if (!string.Equals(document.Key, key, StringComparison.Ordinal))
            throw new InvalidOperationException($"Use the document's existing file name: {document.Key}");
        if (activeHistoryGestureId != 0 && activeHistoryDocumentId is Guid activeId && activeId != document.Id)
            clearHistoryGesture();
        if (activeHistoryGestureId != 0)
            activeHistoryDocumentId = document.Id;
        Documents.Capture(document, description, marker, activeHistoryGestureId);
    }

    private bool canCreateDocument(string section, string key)
    {
        if (string.IsNullOrWhiteSpace(key) || !string.Equals(key, normalizeJsonKey(key), StringComparison.Ordinal)
            || !sections.TryGetValue(section, out DataSection? dataSection) || !dataSection.Persist)
            return false;
        string path = getSectionDataPath(section, key);
        if (dataSection.Data.Keys.Any(existing => getPathComparer().Equals(existing, key))
            || Documents.FindByPath(path) is not null || Directory.Exists(path))
            return false;
        return !File.Exists(path) || Documents.All.Any(document =>
            pathsEqual(document.SavedPath, path) && !pathsEqual(document.Path, path));
    }

    private void RenameDocument(string section, string oldKey, string newKey)
    {
        EditorDocument document = Documents.Find(section, normalizeJsonKey(oldKey))
            ?? throw new InvalidOperationException("The renamed document was not captured.");
        document.Key = normalizeJsonKey(newKey);
        document.Path = getSectionDataPath(section, document.Key);
    }

    private void MarkDocumentSaved(string section, string key)
    {
        if (GetDocument(section, key) is EditorDocument document)
            Documents.MarkSaved(document);
    }

    private void CompleteDocumentChanges()
    {
        using EditorDocumentNotificationBatch notifications = Documents.BeginNotificationBatch();
        foreach (EditorDocument document in Documents.PendingDocuments.ToArray())
        {
            if (!sections.TryGetValue(document.Section, out DataSection? section))
                continue;
            section.Data.TryGetValue(document.Key, out JsonObject? data);
            Documents.Commit(document, data);
        }
        notifications.Commit();
    }

    private void RestoreDocumentState(EditorDocument document, EditorDocumentState state)
    {
        sections[document.Section].Data.Remove(document.Key);
        Documents.Restore(document, state);
    }

    private void synchronizeDocumentIndex(EditorDocument document)
    {
        Dictionary<string, JsonObject> data = sections[document.Section].Data;
        foreach (string oldKey in data.Where(pair => ReferenceEquals(pair.Value, document.InternalData))
                     .Select(pair => pair.Key).Where(key => key != document.Key).ToArray())
            data.Remove(oldKey);
        if (document.InternalData is JsonObject current)
            data[document.Key] = current;
        else
            data.Remove(document.Key);
        if (document.Section is "Maps" or "WorldMaps")
            updateDocumentCatalog(document.Section, document.Key, document.Key, document.InternalData);
    }

    private void removeDocumentIndex(EditorDocument document)
    {
        sections[document.Section].Data.Remove(document.Key);
        if (document.Section is "Maps" or "WorldMaps")
            updateDocumentCatalog(document.Section, document.Key, document.Key, null);
    }

    private void refreshModifiedState()
    {
        CompleteDocumentChanges();
        Documents.AfterChangeNotifications(refreshDocumentStatus);
    }

    private void refreshDocumentStatus()
    {
        UndoRedoStateChanged?.Invoke(this, EventArgs.Empty);
        bool modified = generalDataGenerationPending || Documents.IsModified;
        if (modified == isModified)
            return;
        isModified = modified;
        ModifiedChanged?.Invoke(this, EventArgs.Empty);
    }

    public long BeginHistoryGesture()
    {
        clearHistoryGesture();
        activeHistoryGestureId = Documents.CreateGestureId();
        return activeHistoryGestureId;
    }

    public void EndHistoryGesture(long gestureId)
    {
        if (gestureId == activeHistoryGestureId)
            clearHistoryGesture();
    }

    public void BreakHistoryGesture() => clearHistoryGesture();

    internal bool IsHistoryGestureActive(long gestureId) => gestureId != 0 && gestureId == activeHistoryGestureId;

    public HistoryResult Undo(string section, string key)
    {
        BreakHistoryGesture();
        CompleteDocumentChanges();
        EditorDocument? document = GetDocument(section, key);
        return document is null ? new HistoryResult(false) : Documents.Undo(document);
    }

    public HistoryResult Redo(string section, string key)
    {
        BreakHistoryGesture();
        CompleteDocumentChanges();
        EditorDocument? document = GetDocument(section, key);
        return document is null ? new HistoryResult(false) : Documents.Redo(document);
    }

    private void clearHistoryGesture()
    {
        activeHistoryGestureId = 0;
        activeHistoryDocumentId = null;
    }

    private void RecordMapSnapshot(string mapKey) => RecordDocumentSnapshot("Maps", normaliseMapKey(mapKey));

    private void RecordWorldSnapshot(string worldKey) => RecordDocumentSnapshot("WorldMaps", normalizeWorldKey(worldKey));

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        Thumbnails.Dispose();
        Documents.Clear();
        pendingWorldDirectoryMoves.Clear();
    }
}
