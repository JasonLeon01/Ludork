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
    private static bool canReplayMapHistory(EditorDocument document, DocumentHistoryEntry entry)
        => document.Section == "Maps" && entry.MapEdits is not null && entry.Marker is null
            && entry.GeneralMemberRename is null && entry.Before.InternalData is not null && entry.After.InternalData is not null
            && entry.Before.Section == document.Section && entry.After.Section == document.Section
            && entry.Before.Key == document.Key && entry.After.Key == document.Key
            && entry.Before.Path == document.Path && entry.After.Path == document.Path;

    private async Task<HistoryResult> replayMapHistoryAsync(EditorDocument document, DocumentHistoryEntry entry,
        bool undo, IProgress<EditorOperationProgress>? progress)
    {
        EditorDocumentState source = undo ? entry.After : entry.Before;
        EditorDocumentState target = undo ? entry.Before : entry.After;
        long revision = Documents.Revision;
        long documentRevision = document.Revision;
        IReadOnlyList<JsonDataEdit> edits = undo ? entry.MapEdits!.Backward : entry.MapEdits!.Forward;
        bool current() => !IsDisposed && Documents.Revision == revision && document.Revision == documentRevision
            && ReferenceEquals(Documents.Find(document.Section, document.Key), document)
            && ReferenceEquals((undo ? document.UndoEntries : document.RedoEntries).LastOrDefault(), entry);
        if (!current() || !EditorDocument.StatesEqual(document.CaptureState(), source)
            || !JsonNode.DeepEquals(document.InternalData, source.InternalData))
            return new HistoryResult(false, LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        try
        {
            MapHistoryReplayImpact? impact = MapHistoryReplayImpact.Create(document.Key,
                source.InternalData!, target.InternalData!, edits);
            if (impact is null)
            {
                validateMapHistoryTags(document.Key, target.InternalData!, getNewMapHistoryTags(source.InternalData!, target.InternalData!));
                return await runResourceOperationAsync(store => undo ? store.Undo(document.Section, document.Key)
                    : store.Redo(document.Section, document.Key), result => result.Success, progress, undo ? "UNDO" : "REDO");
            }
            MapHistoryValidationResult? validation = null;
            if (impact.Actors.Any(actor => actor.Fields is null || actor.Fields.Count != 0))
            {
                Dictionary<string, JsonObject> blueprints = sections["Blueprints"].ToDictionary(pair => pair.Key,
                    pair => Documents.Find("Blueprints", pair.Key)?.CaptureState().InternalData
                        ?? (JsonObject)pair.Value.DeepClone(), StringComparer.Ordinal);
                MapHistoryValidationContext context = new(ProjectPath, blueprints, GetProjectEnumCatalog());
                progress?.Report(new EditorOperationProgress("EDIT_OPERATION_VALIDATING", document.Path));
                validation = await Task.Run(() => context.Validate(document.Key, impact.Actors));
                if (!await Task.Run(validation.IsCurrent))
                    return new HistoryResult(false, LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            }
            if (!current())
                return new HistoryResult(false, LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            DocumentReferenceResult references = new(
                (validation?.References.References ?? []).Concat(impact.AutoTiles).ToArray(),
                validation?.References.GeneralMemberTypes ?? new Dictionary<string, string>(StringComparer.Ordinal));
            if (references.References.Count != 0)
                validateMapHistoryReferences(references);
            if (impact.Tags.Count != 0)
                validateMapHistoryTags(document.Key, target.InternalData!, impact.Tags
                    .Select(actor => getString(actor.Actor["tag"]) ?? string.Empty)
                    .Where(tag => tag.Length != 0).ToHashSet(StringComparer.Ordinal));
            if (!current())
                return new HistoryResult(false, LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            HistoryResult result = Documents.ReplayPrepared(document, undo, entry, () =>
            {
                Documents.Restore(document, target);
                Documents.AfterChangeNotifications(refreshDocumentStatus);
                Maps.NotifyMapContentChanged(document.Key, new MapDataEditedEventArgs(document.Key, edits));
                return new HistoryResult(true) { Changes = [document.Path] };
            });
            return result;
        }
        catch (Exception exception) when (exception is InvalidDataException or IOException or InvalidOperationException or UnauthorizedAccessException)
        {
            return new HistoryResult(false, exception.Message);
        }
    }

    private void validateMapHistoryReferences(DocumentReferenceResult references)
    {
        HashSet<string> referencedMembers = references.References.Where(reference => reference.StoragePath is not null)
            .Select(reference => reference.Target).ToHashSet(StringComparer.Ordinal);
        HashSet<string> unavailableMembers = new(StringComparer.Ordinal);
        foreach ((string typeKey, string memberId) in RenamedReferences.GeneralMembers)
        {
            string id = ReferenceIdentity.GeneralMemberNodeId(typeKey, memberId);
            if (referencedMembers.Contains(id)
                && (!sections["General"].TryGetValue(typeKey, out JsonObject? data)
                    || data["members"] is not JsonObject members || !members.ContainsKey(memberId)))
                unavailableMembers.Add(id);
        }
        validateRestoredReferenceTargets(references, unavailableMembers);
        foreach (ReferenceRecord reference in references.References)
        {
            ReferenceNode? node = ReferenceIndexSnapshot.ParseNode(reference.Target);
            if (node is null)
                continue;
            bool exists = node.Type switch
            {
                "asset" => AssetPaths.TryResolveAsset(node.Key, out _),
                "generalMember" => references.GeneralMemberTypes.TryGetValue(node.Id, out string? type)
                    && sections["General"].TryGetValue(type, out JsonObject? data)
                    && data["members"] is JsonObject members
                    && members.ContainsKey(node.Id[("generalMember:".Length + type.Length + 1)..]),
                "map" => Maps.containsMapKey(node.Key),
                "worldMap" => sections["WorldMaps"].ContainsKey(node.Key),
                "blueprint" => sections["Blueprints"].ContainsKey(BlueprintReference.NormalizeKey(node.Key)),
                _ => !ReferenceIndexSnapshot.DataRoots.TryGetValue(node.Type, out string? section)
                    || sections[section].ContainsKey(node.Key),
            };
            if (!exists)
                throw new InvalidDataException($"Undo/Redo cannot restore a reference to an unavailable resource: {reference.Target}");
        }
    }

    private static IReadOnlySet<string> getNewMapHistoryTags(JsonObject source, JsonObject target)
    {
        Dictionary<string, int> counts = new(StringComparer.Ordinal);
        foreach (MapDataService.MapActorCollection collection in WorldDataService.enumerateActorCollections(source))
            foreach (JsonObject actor in collection.Actors.OfType<JsonObject>())
                if (getString(actor["tag"]) is string tag)
                    counts[tag] = counts.GetValueOrDefault(tag) + 1;
        HashSet<string> added = new(StringComparer.Ordinal);
        foreach (MapDataService.MapActorCollection collection in WorldDataService.enumerateActorCollections(target))
            foreach (JsonObject actor in collection.Actors.OfType<JsonObject>())
                if (getString(actor["tag"]) is string tag && tag.Length != 0)
                {
                    if (counts.GetValueOrDefault(tag) == 0)
                        added.Add(tag);
                    else
                        counts[tag]--;
                }
        return added;
    }

    private void validateMapHistoryTags(string mapKey, JsonObject candidate, IReadOnlySet<string> changed)
    {
        if (changed.Count == 0)
            return;
        HashSet<string> seen = new(StringComparer.Ordinal);
        foreach (MapDataService.MapActorCollection collection in WorldDataService.enumerateActorCollections(candidate))
            foreach (JsonObject actor in collection.Actors.OfType<JsonObject>())
                if (getString(actor["tag"]) is string tag && changed.Contains(tag) && !seen.Add(tag))
                    throw new InvalidDataException($"Undo/Redo cannot restore a duplicate actor tag: {tag}");
        int separator = mapKey.IndexOf('/');
        if (separator > 0 && sections["WorldMaps"].ContainsKey(mapKey[..separator]))
            foreach (string tag in changed)
                if (Maps.hasWorldActorTag(mapKey[..separator], tag, mapKey))
                    throw new InvalidDataException($"Undo/Redo cannot restore a duplicate actor tag: {tag}");
    }
}
