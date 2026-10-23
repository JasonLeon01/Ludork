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
    public Task<bool> RenameDocumentResourceAsync(string section, string oldKey, string newKey,
        IProgress<EditorOperationProgress>? progress = null)
    {
        if (normalizeJsonKey(oldKey) == normalizeJsonKey(newKey))
            return Task.FromResult(sections.TryGetValue(section, out EditorDocumentCollection? data)
                && (data.ContainsKey(normalizeJsonKey(oldKey))
                    || section == "Maps" && Maps.containsMapKey(normalizeJsonKey(oldKey))));
        return runResourceOperationAsync(store => store.RenameDocumentResource(section, oldKey, newKey),
            result => result, progress);
    }

    public Task<(bool Handled, string? Error)> TryRenameManagedPathAsync(string oldPath, string newPath,
        IProgress<EditorOperationProgress>? progress = null)
    {
        return runResourceOperationAsync(store =>
        {
            bool handled = store.TryRenameManagedPath(oldPath, newPath, out string? error);
            return (Handled: handled, Error: error);
        }, result => result.Handled && result.Error is null, progress);
    }

    public Task<(bool Handled, string? Error)> TryCopyManagedPathAsync(string sourcePath, string destinationPath,
        IProgress<EditorOperationProgress>? progress = null)
    {
        return runResourceOperationAsync(store =>
        {
            bool handled = store.TryCopyManagedPath(sourcePath, destinationPath, out string? error);
            return (Handled: handled, Error: error);
        }, result => result.Handled && result.Error is null, progress, "COPY");
    }

    internal Task<bool> UpdateMapAsync(string key, MapInfo info, IProgress<EditorOperationProgress>? progress)
        => runResourceOperationAsync(store => store.Maps.UpdateMap(key, info), result => result, progress);

    internal Task<WorldMapMutationResult> RemoveWorldPlacementAsync(string worldKey, string childKey,
        IProgress<EditorOperationProgress>? progress)
        => runResourceOperationAsync(store => store.Worlds.RemoveWorldPlacement(worldKey, childKey),
            result => result.Success, progress, "EDIT_OPERATION_VALIDATING");

    public Task<HistoryResult> UndoAsync(string section, string key, IProgress<EditorOperationProgress>? progress = null)
        => replayResourceAsync(section, key, true, progress);

    public Task<HistoryResult> RedoAsync(string section, string key, IProgress<EditorOperationProgress>? progress = null)
        => replayResourceAsync(section, key, false, progress);

    private Task<HistoryResult> replayResourceAsync(string section, string key, bool undo,
        IProgress<EditorOperationProgress>? progress)
    {
        EditorDocument? document = Documents.Find(section, section == ProjectAssetPaths.Section ? key : normalizeJsonKey(key));
        DocumentHistoryEntry? entry = (undo ? document?.UndoEntries : document?.RedoEntries)?.LastOrDefault();
        if (document is null || entry is null || entry.Marker?.IsBarrier == true
            || !sections.ContainsKey(document.Section) && document.Section != ProjectAssetPaths.Section)
            return Task.FromResult(undo ? Undo(section, key) : Redo(section, key));
        return runResourceOperationAsync(store => undo ? store.Undo(section, key) : store.Redo(section, key),
            result => result.Success, progress, undo ? "UNDO" : "REDO");
    }

    private async Task<T> runResourceOperationAsync<T>(Func<ProjectDataStore, T> operation,
        Func<T, bool> succeeded, IProgress<EditorOperationProgress>? progress,
        string stage = "EDIT_OPERATION_RENAMING")
    {
        progress?.Report(new EditorOperationProgress("EDIT_OPERATION_PREPARING"));
        ProjectOperationSnapshot snapshot = await CaptureOperationSnapshotAsync(progress);
        string[] paths = snapshot.Documents.SelectMany(document => new[] { document.Current.Path, document.Saved.Path }).ToArray();
        ReferenceInputFiles inputs = await Task.Run(() => ReferenceInputFiles.Capture(ProjectPath, paths));
        ReferenceIndexSnapshotBuilder.Result references = await ReferenceIndex.GetCurrentAsync(progress);
        (T result, PreparedResourceChange[] changes, ProjectAssetPathInputs? assetInputs,
            RenamedReferenceState renamedReferences) = await Task.Run(() =>
        {
            if (!references.Files.IsCurrent() || !inputs.IsCurrent())
                throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            using ProjectDataStore working = snapshot.CreateStore();
            working.ReferenceIndex.UseOperationSnapshot(references);
            progress?.Report(new EditorOperationProgress(stage));
            T result = operation(working);
            if (!succeeded(result))
                return (result, Array.Empty<PreparedResourceChange>(), (ProjectAssetPathInputs?)null, snapshot.RenamedReferences);
            Dictionary<Guid, ProjectOperationDocument> originals = snapshot.Documents.ToDictionary(document => document.Id);
            Dictionary<string, MapCatalogEntry> catalog = working.Maps.MapCatalog.ToDictionary(
                entry => MapDataService.getMapCatalogDataKey(entry.Kind, entry.Key), StringComparer.Ordinal);
            List<PreparedResourceChange> changes = [];
            foreach (EditorDocument document in working.Documents.All)
            {
                originals.TryGetValue(document.Id, out ProjectOperationDocument? original);
                EditorDocumentState before = original?.Current ?? document.SavedState;
                EditorDocumentState after = document.CaptureState();
                if (original is not null && original.Revision == document.Revision)
                    continue;
                if (original is null && EditorDocument.StatesEqual(before, after))
                    continue;
                MapCatalogEntryKind kind = document.Section == "WorldMaps" ? MapCatalogEntryKind.WorldMap
                    : document.Key.Contains('/') ? MapCatalogEntryKind.WorldChildMap : MapCatalogEntryKind.StandaloneMap;
                changes.Add(new PreparedResourceChange(document.Id, before, after, document.SavedState,
                    document.InternalData, document.UndoEntries.ToArray(), document.RedoEntries.ToArray(),
                    document.IsModified, !EditorDocumentState.ContentEquals(before, after),
                    document.Section is "Maps" or "WorldMaps"
                        ? catalog.GetValueOrDefault(MapDataService.getMapCatalogDataKey(kind, document.Key)) : null,
                    document.Section == "Maps" ? working.Maps.GetOperationMapBytes(document.Key) : 0));
            }
            if (!references.Files.IsCurrent() || !inputs.IsCurrent())
                throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            ProjectAssetPathInputs assetInputs = working.AssetPaths.CaptureInputs();
            assetInputs.Validate();
            return (result, changes.ToArray(), (ProjectAssetPathInputs?)assetInputs, working.RenamedReferences);
        });
        if (!succeeded(result) || changes.Length == 0)
            return result;
        bool filesCurrent = await Task.Run(() =>
        {
            assetInputs?.Validate();
            return references.Files.IsCurrent() && inputs.IsCurrent();
        });
        if (Documents.Revision != snapshot.Revision || !filesCurrent)
            throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        await applyResourceChangesAsync(changes, progress, renamedReferences);
        return result;
    }

    private async Task applyResourceChangesAsync(IReadOnlyList<PreparedResourceChange> changes,
        IProgress<EditorOperationProgress>? progress, RenamedReferenceState renamedReferences)
    {
        Dictionary<Guid, EditorDocument> existing = Documents.All.ToDictionary(document => document.Id);
        HashSet<Guid> existingIds = existing.Keys.ToHashSet();
        List<Action> restoreEntries = [];
        EditorUiBatch batch = new();
        foreach ((string section, string key) in changes.SelectMany(change => new[]
        {
            (change.Before.Section, change.Before.Key), (change.After.Section, change.After.Key),
        }).Distinct())
        {
            if (section != ProjectAssetPaths.Section)
                captureExternalChange(section, key, restoreEntries);
            await batch.YieldIfNeededAsync();
        }
        bool committed = false;
        try
        {
            using EditorDocumentTransaction transaction = Documents.BeginTransaction([]);
            foreach (PreparedResourceChange change in changes)
            {
                if (!existing.TryGetValue(change.Id, out EditorDocument? document))
                {
                    document = Documents.Register(change.Before.Section, change.Before.Key, change.Before.Path,
                        null, true, change.Id);
                    if (document.Id != change.Id)
                        throw new InvalidOperationException($"The resource document identity changed: {change.Before.Path}");
                    existing.Add(document.Id, document);
                    if (change.Before.Section == ProjectAssetPaths.Section)
                        AssetPaths.Attach(document);
                    else
                        RegisterLoadedDocument(change.Before.Section, change.Before.Key);
                }
                else
                    transaction.Capture(document);
                await batch.YieldIfNeededAsync();
            }
            int completed = 0;
            foreach (PreparedResourceChange change in changes)
            {
                EditorDocument document = existing[change.Id];
                string oldKey = document.Key;
                bool assetPath = document.Section == ProjectAssetPaths.Section;
                if (!assetPath)
                    sections[document.Section].Remove(oldKey);
                document.SetIdentity(change.After.Section, change.After.Key, change.After.Path);
                document.InternalData = change.Data;
                document.CurrentState = change.After;
                document.SavedState = change.Saved;
                document.UndoEntries.Clear();
                document.UndoEntries.AddRange(change.Undo);
                document.RedoEntries.Clear();
                document.RedoEntries.AddRange(change.Redo);
                document.SetPreparedModified(change.Modified);
                if (!assetPath && change.Data is not null)
                    sections[document.Section][document.Key] = change.Data;
                if (change.Catalog is MapCatalogEntry catalog)
                {
                    if (document.Section == "Maps")
                    {
                        Maps.removeMapCatalogEntry(oldKey.Contains('/') ? MapCatalogEntryKind.WorldChildMap
                            : MapCatalogEntryKind.StandaloneMap, oldKey);
                        Maps.ApplyOperationMapMetadata(oldKey, document.Key, change.MapBytes);
                    }
                    else
                    {
                        Maps.removeMapCatalogEntry(MapCatalogEntryKind.WorldMap, oldKey);
                        Worlds.UpdatePendingDirectoryMove(oldKey, document.Key,
                            document.SavedState.InternalData is not null && document.SavedPath != document.Path
                                ? Path.GetDirectoryName(document.SavedPath) : null);
                    }
                    Maps.setMapCatalogEntry(catalog);
                }
                Documents.NotifyPrepared(document, change.Before, change.After, change.ContentChanged);
                progress?.Report(new EditorOperationProgress("EDIT_OPERATION_APPLYING", document.Path, ++completed, changes.Count));
                await batch.YieldIfNeededAsync();
            }
            foreach (PreparedResourceChange change in changes.Where(change => !existingIds.Contains(change.Id)))
                if (change.Saved.Section != ProjectAssetPaths.Section && change.Saved.InternalData is JsonObject saved)
                    originData[change.Saved.Section][change.Saved.Key] = saved;
            SetRenamedReferences(renamedReferences);
            refreshModifiedState();
            foreach (PreparedResourceChange change in changes.Where(change => change.After.Section == "Maps"))
                Maps.NotifyMapContentChanged(change.After.Key);
            if (changes.Any(change => change.After.Section == "UI"))
                UiAssets.NotifyUiAssetsChanged();
            committed = true;
            transaction.Commit();
        }
        finally
        {
            if (!committed)
                foreach (Action restore in restoreEntries)
                    restore();
        }
    }
}
