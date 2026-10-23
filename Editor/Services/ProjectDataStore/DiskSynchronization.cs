using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    internal bool IsSynchronizableDataPath(string path)
    {
        return !DataConfig.isAnimationCache(path)
            && tryGetDataLocation(path, out string section, out _)
            && hasDataFileExtension(section, path);
    }

    internal async Task ApplyDiskChangesAsync(IReadOnlyList<ExternalFileChange> changes,
        IReadOnlySet<string> keepMemoryPaths, IProgress<EditorOperationProgress>? progress = null,
        CancellationToken token = default, Func<Task<bool>>? canCommit = null)
    {
        token.ThrowIfCancellationRequested();
        BreakHistoryGesture();
        CompleteDocumentChanges();
        long revision = Documents.Revision;
        EditorUiBatch batch = new();
        Dictionary<string, EditorDocument> savedDocuments = new(getPathComparer());
        foreach (EditorDocument document in Documents.All)
        {
            savedDocuments.TryAdd(document.SavedPath, document);
            await batch.YieldIfNeededAsync(token);
        }
        HashSet<string> changedPaths = changes.Select(change => change.Path).ToHashSet(getPathComparer());
        HashSet<string> keepPaths = new(keepMemoryPaths, getPathComparer());
        List<DiskChangeInput> inputs = [];
        foreach (ExternalFileChange change in changes)
        {
            if (IsSynchronizableDataPath(change.Path)
                && tryGetDataLocation(change.Path, out string section, out string relative))
            {
                string key = section == "WorldMaps" ? normalizeDataKey(relative) : normalizeJsonKey(relative);
                EditorDocument? document = savedDocuments.GetValueOrDefault(change.Path) ?? Documents.FindByPath(change.Path);
                if (document is not null && !pathsEqual(document.SavedPath, change.Path)
                    && changedPaths.Contains(document.SavedPath))
                    document = null;
                inputs.Add(new DiskChangeInput(change.Path, section, key, change.Content, document,
                    document?.CaptureState(), keepPaths.Contains(change.Path)));
            }
            await batch.YieldIfNeededAsync(token);
        }
        bool synchronizeAssets = changes.Any(change => ProjectDiskSnapshot.IsWithin(change.Path, Path.Combine(ProjectPath, "Assets")))
            || AssetPaths.HasPendingChanges && inputs.Any(change => !change.Keep);
        bool validateBlueprints = inputs.Any(change => !change.Keep && change.Section is "Blueprints" or "Maps");
        bool validateWorlds = inputs.Any(change => !change.Keep && change.Section is "Maps" or "WorldMaps");
        ProjectOperationSnapshot? snapshot = validateBlueprints || validateWorlds || synchronizeAssets
            ? await captureDiskSynchronizationSnapshotAsync(batch, token) : null;
        (PreparedDiskChange[] prepared, PreparedAssetDiskChanges? assets) = await Task.Run(() => prepareDiskSynchronization(
            inputs, snapshot, changes, keepPaths, synchronizeAssets, validateBlueprints, validateWorlds, progress, token), token);
        if (Documents.Revision != revision)
            throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        List<Action> restoreEntries = [];
        HashSet<(string Section, string Key)> captured = [];
        foreach (PreparedDiskChange change in prepared)
        {
            DiskChangeInput input = change.Input;
            if (captured.Add((input.Section, input.Key)))
                captureExternalChange(input.Section, input.Key, restoreEntries);
            if (input.Document is EditorDocument document)
            {
                if (captured.Add((document.Section, document.Key)))
                    captureExternalChange(document.Section, document.Key, restoreEntries);
                if (captured.Add((document.SavedState.Section, document.SavedState.Key)))
                    captureExternalChange(document.SavedState.Section, document.SavedState.Key, restoreEntries);
            }
            await batch.YieldIfNeededAsync(token);
        }
        if (assets is not null)
            foreach (ReferenceRewrite rewrite in assets.Rewrites)
            {
                if (captured.Add((rewrite.Section, rewrite.Key)))
                    captureExternalChange(rewrite.Section, rewrite.Key, restoreEntries);
                await batch.YieldIfNeededAsync(token);
            }
        if (Documents.Revision != revision)
            throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        string[] previousDeletedPaths = deletedDocumentPaths.ToArray();
        bool committed = false;
        try
        {
            using EditorDocumentTransaction transaction = Documents.BeginTransaction([]);
            int completed = 0;
            foreach (PreparedDiskChange change in prepared)
            {
                token.ThrowIfCancellationRequested();
                if (change.Input.Document is EditorDocument document)
                    transaction.Capture(document);
                applyPreparedDiskChange(change);
                progress?.Report(new EditorOperationProgress("EDIT_OPERATION_APPLYING",
                    Path.GetRelativePath(ProjectPath, change.Input.Path), ++completed, prepared.Length));
                await batch.YieldIfNeededAsync(token);
            }
            if (assets is not null)
            {
                AssetPaths.ApplyExternalChanges(assets);
                ApplyReferenceRewrites(assets.Rewrites);
            }
            token.ThrowIfCancellationRequested();
            if (canCommit is not null && !await canCommit())
                throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            token.ThrowIfCancellationRequested();
            if (assets is not null)
                await Task.Run(assets.Inputs.Validate, token);
            token.ThrowIfCancellationRequested();
            foreach (PreparedDiskChange change in prepared)
                if (!change.Input.Keep)
                {
                    clearRejectedDataFile(change.Input.Path);
                    if (change.Data is null)
                        deletedDocumentPaths.Add(change.Input.Path);
                }
            refreshModifiedState();
            string[] changedMaps = prepared.Where(change => !change.Input.Keep && change.Input.Section == "Maps")
                .Select(change => change.Input.Key).Distinct(StringComparer.Ordinal).ToArray();
            if (changedMaps.Length > 1 || prepared.Any(change => !change.Input.Keep && change.Input.Section == "WorldMaps"))
                Maps.NotifyAllMapPreviewsChanged();
            else if (changedMaps.Length == 1)
                Maps.NotifyMapContentChanged(changedMaps[0]);
            if (prepared.Any(change => !change.Input.Keep && change.Input.Section == "UI"))
                UiAssets.NotifyUiAssetsChanged();
            committed = true;
            transaction.Commit();
        }
        finally
        {
            if (!committed)
            {
                foreach (Action restore in restoreEntries)
                    restore();
                deletedDocumentPaths.Clear();
                deletedDocumentPaths.UnionWith(previousDeletedPaths);
            }
        }
    }

    private async Task<ProjectOperationSnapshot> captureDiskSynchronizationSnapshotAsync(EditorUiBatch batch, CancellationToken token)
    {
        List<ProjectOperationDocument> documents = [];
        foreach (EditorDocument document in Documents.All)
        {
            if (sections.ContainsKey(document.Section) || document.Section == ProjectAssetPaths.Section)
                documents.Add(new ProjectOperationDocument(document.Id, document.Revision, document.CaptureState(),
                    document.SavedState, document.UndoEntries.ToArray(), document.RedoEntries.ToArray()));
            await batch.YieldIfNeededAsync(token);
        }
        Dictionary<string, string> moves = new(StringComparer.Ordinal);
        foreach (string key in sections["WorldMaps"].Keys)
        {
            if (Worlds.TryGetPendingDirectoryMove(key, out string? directory))
                moves.Add(key, directory);
            await batch.YieldIfNeededAsync(token);
        }
        return new ProjectOperationSnapshot(ProjectPath, Documents.Revision, documents, Maps.MapCatalog.ToArray(), moves,
            InvalidLoadPaths, InvalidLoadErrors, deletedDocumentPaths.ToArray(), projectEnumGenerationPending, RenamedReferences);
    }

    private (PreparedDiskChange[] Changes, PreparedAssetDiskChanges? Assets) prepareDiskSynchronization(
        IReadOnlyList<DiskChangeInput> inputs, ProjectOperationSnapshot? snapshot,
        IReadOnlyList<ExternalFileChange> externalChanges, IReadOnlySet<string> keepPaths,
        bool synchronizeAssets, bool validateBlueprints, bool validateWorlds,
        IProgress<EditorOperationProgress>? progress, CancellationToken token)
    {
        List<PreparedDiskChange> prepared = [];
        foreach (DiskChangeInput input in inputs.OrderBy(input => input.Document is null))
        {
            token.ThrowIfCancellationRequested();
            JsonObject? data = input.Content is null ? null
                : readDiskSynchronizationData(input.Path, input.Section, input.Key, input.Content);
            EditorDocumentState state = new(input.Section, input.Key, input.Path, data);
            bool modified = input.Current is not null && !EditorDocument.StatesEqual(input.Current, state);
            bool contentChanged = !EditorDocumentState.ContentEquals(input.Current, state);
            prepared.Add(new PreparedDiskChange(input, state, data, modified, contentChanged, null, 0));
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_PREPARING",
                Path.GetRelativePath(ProjectPath, input.Path), prepared.Count, inputs.Count));
        }
        if (snapshot is null)
            return (prepared.ToArray(), null);
        progress?.Report(new EditorOperationProgress("EDIT_OPERATION_VALIDATING"));
        using ProjectDataStore candidate = snapshot.CreateStore();
        Dictionary<Guid, EditorDocument> candidateDocuments = candidate.Documents.All.ToDictionary(document => document.Id);
        foreach (PreparedDiskChange change in prepared)
        {
            token.ThrowIfCancellationRequested();
            DiskChangeInput input = change.Input;
            if (input.Keep)
                continue;
            EditorDocument? existing = input.Document is null ? null : candidateDocuments.GetValueOrDefault(input.Document.Id);
            if (change.Data is null)
            {
                if (existing is not null)
                    candidate.removeDataKey(existing.Section, existing.Key);
                candidate.removeDataKey(input.Section, input.Key);
                continue;
            }
            EditorDocument document = existing ?? candidate.RegisterLoadedDocument(input.Section, input.Key);
            EditorDocument? occupied = candidate.Documents.Find(input.Section, input.Key);
            if (occupied is not null && !ReferenceEquals(occupied, document))
                throw new InvalidDataException($"The disk file conflicts with another editor document: {input.Path}");
            candidate.Documents.Restore(document, change.State);
        }
        token.ThrowIfCancellationRequested();
        PreparedAssetDiskChanges? assets = synchronizeAssets
            ? candidate.AssetPaths.PrepareExternalChanges(externalChanges, keepPaths) : null;
        if (assets is not null)
        {
            using EditorDocumentTransaction transaction = candidate.Documents.BeginTransaction([]);
            candidate.AssetPaths.ApplyExternalChanges(assets);
            candidate.ApplyReferenceRewrites(assets.Rewrites);
            transaction.Commit();
            Dictionary<string, string> replacements = candidate.AssetPaths.CapturePreviewMapping()
                .ToDictionary(pair => pair.Value, pair => pair.Key, StringComparer.Ordinal);
            if (replacements.Count != 0)
            {
                HashSet<(string Section, string Key)> diskDocuments = prepared
                    .Where(change => !change.Input.Keep && change.Data is not null)
                    .Select(change => (change.Input.Section, change.Input.Key)).ToHashSet();
                ReferenceRewrite[] forward = candidate.ReferenceIndex.PrepareAssetPathRewrites(replacements, diskDocuments).ToArray();
                candidate.ApplyReferenceRewrites(forward);
                Dictionary<(string Section, string Key), ReferenceRewrite> rewrites = assets.Rewrites
                    .ToDictionary(rewrite => (rewrite.Section, rewrite.Key));
                foreach (ReferenceRewrite rewrite in forward)
                {
                    (string Section, string Key) location = (rewrite.Section, rewrite.Key);
                    rewrites[location] = rewrites.TryGetValue(location, out ReferenceRewrite? previous)
                        ? previous with { Candidate = rewrite.Candidate } : rewrite;
                }
                assets = assets with { Rewrites = rewrites.Values.ToArray() };
            }
        }
        token.ThrowIfCancellationRequested();
        if (validateBlueprints)
        {
            IReadOnlyList<string> errors = candidate.ValidateBlueprintSchemas(includeRejectedFiles: false);
            if (errors.Count != 0)
                throw new InvalidDataException(string.Join(Environment.NewLine, errors));
        }
        token.ThrowIfCancellationRequested();
        if (validateWorlds)
        {
            foreach (MapCatalogEntry entry in candidate.Maps.MapCatalog.Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap))
                if (entry.WorldKey is null || !candidate.sections["WorldMaps"].ContainsKey(entry.WorldKey))
                    throw new InvalidDataException($"The child map has no world manifest: {entry.Key}");
            WorldMapMutationResult validation = candidate.Worlds.ValidateAllWorldMaps();
            if (!validation.Success)
                throw new InvalidDataException(validation.Details);
        }
        token.ThrowIfCancellationRequested();
        Dictionary<(string Section, string Key), MapCatalogEntry> catalog = candidate.Maps.MapCatalog.ToDictionary(
            entry => (entry.Kind == MapCatalogEntryKind.WorldMap ? "WorldMaps" : "Maps", entry.Key));
        for (int index = 0; index < prepared.Count; index++)
        {
            PreparedDiskChange change = prepared[index];
            token.ThrowIfCancellationRequested();
            if (!change.Input.Keep && change.Data is not null && change.Input.Section is "Maps" or "WorldMaps")
                prepared[index] = change with
                {
                    Catalog = catalog.GetValueOrDefault((change.Input.Section, change.Input.Key)),
                    MapBytes = change.Input.Section == "Maps" ? candidate.Maps.GetOperationMapBytes(change.Input.Key) : 0,
                };
        }
        return (prepared.ToArray(), assets);
    }

    private void applyPreparedDiskChange(PreparedDiskChange change)
    {
        DiskChangeInput input = change.Input;
        EditorDocument? existing = input.Document;
        if (input.Keep)
        {
            if (existing is null)
                return;
            originData[existing.SavedState.Section].Remove(existing.SavedState.Key);
            if (change.State.InternalData is JsonObject saved)
                originData[input.Section][input.Key] = saved;
            else
                originData[input.Section].Remove(input.Key);
            existing.SavedState = change.State;
            existing.SetPreparedModified(change.Modified);
            Documents.Notify(existing);
            return;
        }
        if (change.Data is null)
        {
            if (existing is not null)
            {
                originData[existing.SavedState.Section].Remove(existing.SavedState.Key);
                removeDataKey(existing.Section, existing.Key);
                Documents.RequestDiskReload(existing);
            }
            removeDataKey(input.Section, input.Key);
            return;
        }
        EditorDocument document = existing ?? Documents.Register(input.Section, input.Key, input.Path, null, true);
        if (existing is null)
            RegisterLoadedDocument(input.Section, input.Key);
        EditorDocument? occupied = Documents.Find(input.Section, input.Key);
        if (occupied is not null && !ReferenceEquals(occupied, document))
            throw new InvalidDataException($"The disk file conflicts with another editor document: {input.Path}");
        EditorDocumentState before = document.CaptureState();
        originData[document.SavedState.Section].Remove(document.SavedState.Key);
        removeDocumentIndex(document);
        document.SetIdentity(input.Section, input.Key, input.Path);
        document.InternalData = change.Data;
        document.CurrentState = change.State;
        document.SavedState = change.State;
        document.SetPreparedModified(false);
        document.UndoEntries.Clear();
        document.RedoEntries.Clear();
        sections[input.Section][input.Key] = change.Data;
        originData[input.Section][input.Key] = change.State.InternalData!;
        if (change.Catalog is MapCatalogEntry catalog)
        {
            if (input.Section == "Maps")
                Maps.ApplyOperationMapMetadata(before.Key, input.Key, change.MapBytes);
            else
                Worlds.UpdatePendingDirectoryMove(before.Key, input.Key, null);
            Maps.setMapCatalogEntry(catalog);
        }
        Documents.NotifyPrepared(document, before, change.State, change.ContentChanged);
        Documents.RequestDiskReload(document);
    }

    private sealed record DiskChangeInput(string Path, string Section, string Key, byte[]? Content,
        EditorDocument? Document, EditorDocumentState? Current, bool Keep);

    private sealed record PreparedDiskChange(DiskChangeInput Input, EditorDocumentState State, JsonObject? Data,
        bool Modified, bool ContentChanged, MapCatalogEntry? Catalog, long MapBytes);

    private JsonObject readDiskSynchronizationData(string path, string section, string key, byte[] content)
    {
        JsonObject data;
        try
        {
            data = JsonNode.Parse(Encoding.UTF8.GetString(content).TrimStart('\uFEFF')) as JsonObject
                ?? throw new InvalidDataException($"The data file must contain an object: {path}");
        }
        catch (JsonException exception)
        {
            throw new InvalidDataException($"The data file could not be parsed: {path}", exception);
        }
        if (!sections[section].AcceptsType(getString(data["type"])))
            throw new InvalidDataException($"The data file has an unexpected type: {path}");
        if (section == "Maps" && !MapDataService.tryReadMapShape(data, out _, out _, out _, true))
            throw new InvalidDataException($"The map data has an invalid shape: {path}");
        if (section == "WorldMaps" && (data["width"] is not JsonValue widthValue || !widthValue.TryGetValue(out int width) || width <= 0
            || data["height"] is not JsonValue heightValue || !heightValue.TryGetValue(out int height) || height <= 0))
            throw new InvalidDataException($"The world map must have positive integer dimensions: {path}");
        IReadOnlyList<string> errors = section switch
        {
            "Particles" => ParticleAssetSchema.Validate(data, key),
            "Subtitles" => SubtitleAssetSchema.Validate(data, key),
            "Materials" => MaterialAssetSchema.Validate(data, key),
            _ => [],
        };
        if (errors.Count != 0)
            throw new InvalidDataException(path + ": " + string.Join(Environment.NewLine, errors));
        if (!sections[section].PreserveType)
            data.Remove("type");
        return data;
    }
}
