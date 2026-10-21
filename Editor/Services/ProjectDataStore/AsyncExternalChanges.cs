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
    public async Task<Exception?> ApplyExternalFileChangesAsync(IReadOnlyList<string> addedPaths,
        IReadOnlyList<(string OldPath, string NewPath)> movedPaths, IReadOnlyList<string> deletedPaths,
        IProgress<EditorOperationProgress>? progress = null)
    {
        if (addedPaths.Count == 0 && movedPaths.Count == 0 && deletedPaths.Count == 0)
            return null;
        ProjectOperationSnapshot snapshot = await CaptureOperationSnapshotAsync(progress);
        PreparedExternalFileChanges prepared = await Task.Run(() =>
            prepareExternalFileChanges(snapshot, addedPaths, movedPaths, deletedPaths, progress));
        bool inputsCurrent = await Task.Run(prepared.Inputs.IsCurrent);
        if (IsDisposed || Documents.Revision != snapshot.Revision || !inputsCurrent)
            throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        Dictionary<Guid, EditorDocument> documents = Documents.All.ToDictionary(document => document.Id);
        List<Action> restoreEntries = [];
        EditorUiBatch batch = new();
        foreach ((string section, string key) in prepared.Origins.Keys)
        {
            captureExternalChange(section, key, restoreEntries);
            await batch.YieldIfNeededAsync();
        }
        if (IsDisposed || Documents.Revision != snapshot.Revision)
            throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        string[] previousInvalidPaths = InvalidLoadPaths.ToArray();
        IReadOnlyDictionary<string, string> previousInvalidErrors = InvalidLoadErrors;
        bool committed = false;
        Exception? notificationError = null;
        try
        {
            using EditorDocumentTransaction transaction = Documents.BeginTransaction(
                prepared.Removed.Concat(prepared.Changes.Select(change => change.Id)).Distinct()
                    .Where(documents.ContainsKey).Select(id => documents[id]));
            foreach (Guid id in prepared.Removed)
            {
                EditorDocument document = documents[id];
                removeDataKey(document.Section, document.Key);
                progress?.Report(new EditorOperationProgress("EDIT_OPERATION_APPLYING", document.Path));
                await batch.YieldIfNeededAsync();
            }
            await applyResourceChangesAsync(prepared.Changes, progress);
            foreach ((MapCatalogEntry? before, MapCatalogEntry? after) in prepared.Catalog)
            {
                if (after is not null)
                    Maps.setMapCatalogEntry(after);
                else if (before is not null)
                {
                    Maps.removeMapCatalogEntry(before.Kind, before.Key);
                    if (before.Kind == MapCatalogEntryKind.WorldMap)
                        Worlds.UpdatePendingDirectoryMove(before.Key, before.Key, null);
                    else
                        Maps.removeLoadedMapMetadata(before.Key);
                }
                await batch.YieldIfNeededAsync();
            }
            foreach (KeyValuePair<(string Section, string Key), JsonObject?> origin in prepared.Origins)
            {
                if (origin.Value is JsonObject data)
                    originData[origin.Key.Section][origin.Key.Key] = data;
                else
                    originData[origin.Key.Section].Remove(origin.Key.Key);
                await batch.YieldIfNeededAsync();
            }
            inputsCurrent = await Task.Run(prepared.Inputs.IsCurrent);
            if (IsDisposed || Documents.Revision != snapshot.Revision || !inputsCurrent)
                throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            replaceExternalInvalidPaths(prepared.InvalidPaths, prepared.InvalidErrors);
            refreshModifiedState();
            if (prepared.Catalog.Length != 0)
                Maps.NotifyAllMapPreviewsChanged();
            if (prepared.Changes.All(change => change.After.Section != "UI")
                && prepared.Origins.Keys.Any(location => location.Section == "UI"))
                UiAssets.NotifyUiAssetsChanged();
            Documents.PublishReset();
            committed = true;
            transaction.Commit();
        }
        catch (Exception exception) when (committed)
        {
            notificationError = exception;
        }
        finally
        {
            if (!committed)
            {
                foreach (Action restore in restoreEntries)
                    restore();
                replaceExternalInvalidPaths(previousInvalidPaths, previousInvalidErrors);
            }
        }
        try
        {
            DataReloaded?.Invoke(this, EventArgs.Empty);
        }
        catch (Exception exception)
        {
            notificationError = notificationError is null ? exception : new AggregateException(notificationError, exception);
        }
        return notificationError;
    }

    private static PreparedExternalFileChanges prepareExternalFileChanges(ProjectOperationSnapshot snapshot,
        IReadOnlyList<string> addedPaths, IReadOnlyList<(string OldPath, string NewPath)> movedPaths,
        IReadOnlyList<string> deletedPaths, IProgress<EditorOperationProgress>? progress)
    {
        string[] paths = snapshot.Documents.SelectMany(document => new[] { document.Current.Path, document.Saved.Path })
            .Distinct(StringComparer.Ordinal).ToArray();
        ReferenceInputFiles inputs = ReferenceInputFiles.Capture(snapshot.ProjectPath, paths, includeData: true);
        progress?.Report(new EditorOperationProgress("EDIT_OPERATION_VALIDATING"));
        using ProjectDataStore working = snapshot.CreateStore();
        IReadOnlyDictionary<string, string> replacements = working.ReferenceIndex.CreateMapMoveReplacements(movedPaths);
        working.ApplyExternalFileChanges(addedPaths, movedPaths, deletedPaths,
            replacements.Count == 0 ? null : () => working.ReferenceIndex.PrepareMapReferenceRewrites(replacements));
        Dictionary<Guid, ProjectOperationDocument> originals = snapshot.Documents.ToDictionary(document => document.Id);
        Dictionary<string, MapCatalogEntry> beforeCatalog = snapshot.MapCatalog.ToDictionary(
            entry => MapDataService.getMapCatalogDataKey(entry.Kind, entry.Key), StringComparer.Ordinal);
        Dictionary<string, MapCatalogEntry> catalog = working.Maps.MapCatalog.ToDictionary(
            entry => MapDataService.getMapCatalogDataKey(entry.Kind, entry.Key), StringComparer.Ordinal);
        List<PreparedResourceChange> changes = [];
        HashSet<Guid> remaining = [];
        HashSet<(string Section, string Key)> locations = [];
        foreach (EditorDocument document in working.Documents.All)
        {
            remaining.Add(document.Id);
            originals.TryGetValue(document.Id, out ProjectOperationDocument? original);
            if (original is not null && original.Revision == document.Revision)
                continue;
            EditorDocumentState after = document.CaptureState();
            EditorDocumentState before = original?.Current
                ?? new EditorDocumentState(after.Section, after.Key, after.Path, null);
            MapCatalogEntryKind kind = document.Section == "WorldMaps" ? MapCatalogEntryKind.WorldMap
                : document.Key.Contains('/') ? MapCatalogEntryKind.WorldChildMap : MapCatalogEntryKind.StandaloneMap;
            changes.Add(new PreparedResourceChange(document.Id, before, after, document.SavedState,
                document.InternalData, document.UndoEntries.ToArray(), document.RedoEntries.ToArray(),
                document.IsModified, !EditorDocumentState.ContentEquals(before, after),
                document.Section is "Maps" or "WorldMaps"
                    ? catalog.GetValueOrDefault(MapDataService.getMapCatalogDataKey(kind, document.Key)) : null,
                document.Section == "Maps" ? working.Maps.GetOperationMapBytes(document.Key) : 0));
            locations.Add((before.Section, before.Key));
            locations.Add((after.Section, after.Key));
            locations.Add((document.SavedState.Section, document.SavedState.Key));
        }
        Guid[] removed = originals.Keys.Where(id => !remaining.Contains(id)).ToArray();
        foreach (Guid id in removed)
        {
            ProjectOperationDocument original = originals[id];
            locations.Add((original.Current.Section, original.Current.Key));
            locations.Add((original.Saved.Section, original.Saved.Key));
        }
        List<(MapCatalogEntry? Before, MapCatalogEntry? After)> catalogChanges = [];
        foreach (string key in beforeCatalog.Keys.Union(catalog.Keys, StringComparer.Ordinal))
        {
            beforeCatalog.TryGetValue(key, out MapCatalogEntry? before);
            catalog.TryGetValue(key, out MapCatalogEntry? after);
            if (before is not null && after is not null
                && JsonNode.DeepEquals(MapDataService.createMapCatalogData(before), MapDataService.createMapCatalogData(after)))
                continue;
            catalogChanges.Add((before, after));
            MapCatalogEntry entry = after ?? before!;
            locations.Add((entry.Kind == MapCatalogEntryKind.WorldMap ? "WorldMaps" : "Maps", entry.Key));
        }
        Dictionary<(string Section, string Key), JsonObject?> origins = locations.ToDictionary(
            location => location, location => working.originData[location.Section].GetValueOrDefault(location.Key));
        if (!inputs.IsCurrent())
            throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        return new PreparedExternalFileChanges(changes.ToArray(), removed, origins, catalogChanges.ToArray(),
            working.InvalidLoadPaths.ToArray(), working.InvalidLoadErrors, inputs);
    }

    private void replaceExternalInvalidPaths(IReadOnlyList<string> paths, IReadOnlyDictionary<string, string> errors)
    {
        lock (invalidLoadLock)
        {
            invalidLoadPaths.Clear();
            invalidLoadPaths.AddRange(paths);
            invalidLoadErrors.Clear();
            foreach ((string path, string error) in errors)
                invalidLoadErrors[path] = error;
        }
    }

    private sealed record PreparedExternalFileChanges(PreparedResourceChange[] Changes, Guid[] Removed,
        IReadOnlyDictionary<(string Section, string Key), JsonObject?> Origins,
        (MapCatalogEntry? Before, MapCatalogEntry? After)[] Catalog,
        string[] InvalidPaths, IReadOnlyDictionary<string, string> InvalidErrors, ReferenceInputFiles Inputs);
}
