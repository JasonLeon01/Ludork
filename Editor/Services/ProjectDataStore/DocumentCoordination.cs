using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    internal readonly HashSet<string> deletedDocumentPaths = new(OperatingSystem.IsWindows()
        ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);

    internal HistoryResult restoreDocumentHistory(EditorDocument document, DocumentHistoryEntry entry, bool undo)
    {
        EditorDocumentState state = undo ? entry.Before : entry.After;
        if (state.Data is not JsonObject candidate)
            return new HistoryResult(false, "File creation and deletion cannot be undone.") { IsBlocked = true };
        try
        {
            validateRestoredReferences(document, candidate);
            if (!commitResourceChange(document.Section, document.Key, state.Key, candidate, recordSource: false))
                return new HistoryResult(false, "The document could not be restored because its name or references changed.");
            NotifyDataRestored();
            return new HistoryResult(true) { Changes = [document.Path] };
        }
        catch (Exception exception) when (exception is InvalidDataException or InvalidOperationException or IOException)
        {
            return new HistoryResult(false, exception.Message);
        }
    }

    internal bool commitResourceChange(string section, string oldKey, string newKey, JsonObject candidate,
        JsonObject? worldCandidate = null, bool recordSource = true)
    {
        EditorDocument? source = GetDocument(section, oldKey);
        if (source is null || source.InternalData is null)
            return false;
        oldKey = source.Key;
        bool renamed = !string.Equals(oldKey, newKey, StringComparison.Ordinal);
        if (renamed && (GetDocument(section, newKey) is EditorDocument occupied && !ReferenceEquals(occupied, source)
            || File.Exists(getSectionDataPath(section, newKey))
                && !Documents.All.Any(document => pathsEqual(document.SavedPath, getSectionDataPath(section, newKey)))))
            return false;
        Dictionary<EditorDocument, (string Key, JsonObject Data)> changes = new()
        {
            [source] = (newKey, (JsonObject)candidate.DeepClone()),
        };
        LuaMetadataService metadata = new(ProjectPath);
        using BlueprintClassResolver resolver = new(this, metadata);
        using ReferenceIndexService references = new(this, metadata, resolver);
        if (renamed)
        {
            IReadOnlyList<ReferenceRewrite> rewrites;
            if (section == "WorldMaps")
            {
                Dictionary<string, string> replacements = new(StringComparer.Ordinal)
                {
                    [oldKey + "/_world.json"] = newKey + "/_world.json",
                };
                foreach (string childKey in Worlds.getWorldChildren(oldKey).ToArray())
                {
                    EditorDocument child = GetDocument("Maps", childKey)
                        ?? throw new InvalidDataException($"The world child could not be loaded: {childKey}");
                    string targetKey = newKey + childKey[oldKey.Length..];
                    replacements[childKey + ".json"] = targetKey + ".json";
                    changes[child] = (targetKey, child.Data!);
                }
                rewrites = references.PrepareMapReferenceRewrites(replacements);
            }
            else
                rewrites = references.PrepareDocumentRename(section, oldKey, newKey);
            foreach (ReferenceRewrite rewrite in rewrites)
            {
                EditorDocument target = GetDocument(rewrite.Section, rewrite.Key)
                    ?? throw new InvalidDataException($"The referenced document could not be loaded: {rewrite.Section}/{rewrite.Key}");
                (string Key, JsonObject Data) change = changes.TryGetValue(target, out (string Key, JsonObject Data) existing)
                    ? existing : (target.Key, target.Data!);
                applyDocumentDifference(change.Data, rewrite.Original, rewrite.Candidate);
                changes[target] = change;
            }
        }
        if (section == "Maps" && Worlds.TryGetWorldForMap(oldKey, out string worldKey))
        {
            EditorDocument world = GetDocument("WorldMaps", worldKey)!;
            JsonObject composition = worldCandidate ?? prepareWorldForMapChange(worldKey, oldKey, newKey, changes[source].Data);
            if (!JsonNode.DeepEquals(world.InternalData, composition))
                changes[world] = (worldKey, composition);
        }
        using EditorDocumentTransaction transaction = Documents.BeginTransaction(changes.Keys);
        foreach (KeyValuePair<EditorDocument, (string Key, JsonObject Data)> change in changes)
        {
            EditorDocument document = change.Key;
            string previousKey = document.Key;
            if (!ReferenceEquals(document, source) || recordSource)
            {
                HistoryMarker? marker = ReferenceEquals(document, source) ? null
                    : new HistoryMarker(LocaleService.Get("DOCUMENT_HISTORY_REFERENCE_BARRIER")
                        .Replace("{source}", source.Path, StringComparison.Ordinal), source.Id, source.Path, true);
                RecordDocumentSnapshot(document.Section, previousKey,
                    renamed ? $"Rename {oldKey} to {newKey}" : "Edit document", marker);
            }
            if (ReferenceEquals(document, source) && !recordSource)
                RestoreDocumentState(document, new EditorDocumentState(section, change.Value.Key,
                    getSectionDataPath(section, change.Value.Key), change.Value.Data));
            else
            {
                sections[document.Section].Remove(previousKey);
                sections[document.Section][change.Value.Key] = change.Value.Data;
                if (previousKey != change.Value.Key)
                    RenameDocument(document.Section, previousKey, change.Value.Key);
            }
            updateDocumentCatalog(document.Section, previousKey, change.Value.Key, change.Value.Data);
        }
        CompleteDocumentChanges();
        transaction.Commit();
        refreshModifiedState();
        if (changes.Keys.Any(document => document.Section == "UI"))
            UiAssets.NotifyUiAssetsChanged();
        foreach (EditorDocument document in changes.Keys.Where(document => document.Section == "Maps"))
            Maps.NotifyMapContentChanged(document.Key);
        return true;
    }

    internal JsonObject prepareWorldForMapChange(string worldKey, string oldKey, string newKey, JsonObject candidate)
    {
        JsonObject world = Worlds.getWorldMap(worldKey)!;
        WorldMapValidationResult current = Worlds.ValidateWorldMap(worldKey);
        string oldFile = Path.GetFileName(oldKey) + ".json";
        List<WorldMapPlacement> placements = current.Placements.Select(placement => placement.Map == oldFile
            ? new WorldMapPlacement(Path.GetFileName(newKey) + ".json", new WorldMapRect(
                placement.Rect.X, placement.Rect.Y, candidate["width"]!.GetValue<int>(), candidate["height"]!.GetValue<int>()))
            : placement).ToList();
        Dictionary<string, MapCatalogEntry> children = Worlds.getWorldChildCatalog(worldKey)
            .Where(pair => pair.Key != oldKey).ToDictionary(pair => pair.Key, pair => pair.Value, StringComparer.Ordinal);
        children[newKey] = MapDataService.createMapCatalogEntry(newKey, MapCatalogEntryKind.WorldChildMap, worldKey, candidate);
        IReadOnlyList<string> order = Worlds.worldMapValidation.TryMergeLayerOrder(worldKey, placements, children)
            ?? throw new InvalidDataException("The map layers conflict with the world composition.");
        JsonObject result = WorldDataService.replaceWorldComposition(world, order, placements);
        WorldMapValidationResult validation = Worlds.worldMapValidation.Validate(worldKey, result, children);
        if (!validation.IsValid)
            throw new InvalidDataException(WorldDataService.formatWorldMapValidation(validation));
        return result;
    }

    internal void updateDocumentCatalog(string section, string oldKey, string newKey, JsonObject? data)
    {
        if (data is null)
        {
            if (section == "Maps")
            {
                Maps.removeMapCatalogEntry(oldKey.Contains('/') ? MapCatalogEntryKind.WorldChildMap : MapCatalogEntryKind.StandaloneMap, oldKey);
                Maps.removeLoadedMapMetadata(oldKey);
            }
            else if (section == "WorldMaps")
            {
                Maps.removeMapCatalogEntry(MapCatalogEntryKind.WorldMap, oldKey);
                Worlds.UpdatePendingDirectoryMove(oldKey, oldKey, null);
            }
            return;
        }
        if (section == "Maps")
        {
            Maps.removeMapCatalogEntry(oldKey.Contains('/') ? MapCatalogEntryKind.WorldChildMap : MapCatalogEntryKind.StandaloneMap, oldKey);
            string? worldKey = newKey.Contains('/') ? newKey[..newKey.IndexOf('/')] : null;
            Maps.setMapCatalogEntry(MapDataService.createMapCatalogEntry(newKey, worldKey is null
                ? MapCatalogEntryKind.StandaloneMap : MapCatalogEntryKind.WorldChildMap, worldKey, data));
            if (oldKey != newKey)
                Maps.rekeyLoadedMapMetadata(oldKey, newKey);
            Maps.updateLoadedMapMetadata(newKey, data);
        }
        else if (section == "WorldMaps")
        {
            Maps.removeMapCatalogEntry(MapCatalogEntryKind.WorldMap, oldKey);
            Maps.setMapCatalogEntry(new MapCatalogEntry(newKey, getString(data["worldName"]) ?? newKey,
                MapCatalogEntryKind.WorldMap, null, data["width"]!.GetValue<int>(), data["height"]!.GetValue<int>(),
                MapDataService.readStringArray(data["layerOrder"]), []));
            Worlds.UpdatePendingDirectoryMove(oldKey, oldKey, null);
            EditorDocument? document = GetDocument(section, newKey);
            if (document is not null && document.SavedState.InternalData is not null && document.SavedPath != document.Path)
                Worlds.UpdatePendingDirectoryMove(oldKey, newKey, Path.GetDirectoryName(document.SavedPath)!);
        }
    }

    internal static void applyDocumentDifference(JsonObject target, JsonObject before, JsonObject after)
    {
        foreach (string key in before.Select(pair => pair.Key).Except(after.Select(pair => pair.Key)).ToArray())
            target.Remove(key);
        foreach (KeyValuePair<string, JsonNode?> pair in after)
        {
            if (JsonNode.DeepEquals(before[pair.Key], pair.Value))
                continue;
            if (before[pair.Key] is JsonObject oldObject && pair.Value is JsonObject newObject && target[pair.Key] is JsonObject currentObject)
                applyDocumentDifference(currentObject, oldObject, newObject);
            else if (before[pair.Key] is JsonArray oldArray && pair.Value is JsonArray newArray
                && target[pair.Key] is JsonArray currentArray && oldArray.Count == newArray.Count && currentArray.Count == oldArray.Count)
            {
                for (int index = 0; index < oldArray.Count; index++)
                {
                    if (JsonNode.DeepEquals(oldArray[index], newArray[index]))
                        continue;
                    if (oldArray[index] is JsonObject oldItem && newArray[index] is JsonObject newItem && currentArray[index] is JsonObject currentItem)
                        applyDocumentDifference(currentItem, oldItem, newItem);
                    else
                        currentArray[index] = newArray[index]?.DeepClone();
                }
            }
            else
                target[pair.Key] = pair.Value?.DeepClone();
        }
    }

    internal void validateRestoredReferences(EditorDocument document, JsonObject candidate)
    {
        JsonObject original = sections[document.Section][document.Key];
        LuaMetadataService metadata = new(ProjectPath);
        using BlueprintClassResolver resolver = new(this, metadata);
        using ReferenceIndexService references = new(this, metadata, resolver);
        try
        {
            sections[document.Section][document.Key] = candidate;
            references.MarkDirty();
            foreach (ReferenceRecord reference in references.GetOutgoingForDocumentPath(document.Path))
            {
                string path = references.GetNodePath(reference.Target);
                if (deletedDocumentPaths.Contains(path) && GetDocumentByPath(path)?.Exists != true)
                    throw new InvalidDataException($"Undo/Redo cannot restore a reference to a deleted file: {path}");
            }
        }
        finally
        {
            sections[document.Section][document.Key] = original;
        }
    }

}
