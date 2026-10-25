using Avalonia.Threading;
using Ludork.Models;
using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.Plugins;

internal sealed partial class MapEditorHostBridge
{
    public IReadOnlyList<PluginActorBlueprintSummary> ListActorBlueprints()
    {
        return runOnUiThread(() =>
        {
            List<PluginActorBlueprintSummary> result = [];
            using IDisposable batch = gameData.BlueprintClasses.BeginBatch();
            foreach (string key in gameData.Blueprints.BlueprintsData.Keys.Order(StringComparer.Ordinal))
            {
                string reference = BlueprintReference.ToReference(key);
                if (gameData.BlueprintClasses.IsDerivedFrom(reference, "Engine.Actor"))
                    result.Add(new PluginActorBlueprintSummary(reference, key));
            }
            return (IReadOnlyList<PluginActorBlueprintSummary>)result.AsReadOnly();
        });
    }

    public PluginMapActorSnapshot ReadActorBlueprintPreview(string blueprintReference)
    {
        return runOnUiThread(() =>
        {
            string reference = BlueprintReference.ToReference(blueprintReference);
            if (!gameData.Blueprints.ContainsReference(reference)
                || !gameData.BlueprintClasses.IsDerivedFrom(reference, "Engine.Actor"))
                throw new ArgumentException("The selected Blueprint is not an Actor.", nameof(blueprintReference));
            JsonObject actor = new() { ["bp"] = reference, ["position"] = new JsonArray(0, 0) };
            JsonObject map = new() { ["actors"] = new JsonObject { ["preview"] = new JsonArray(actor) } };
            return readActors(map, "preview", 1, 1)[0];
        });
    }

    public Task<PluginProjectScriptResult> RunProjectScriptAsync(
        string scriptPath, JsonObject request, CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(request);
        return scriptRunner.RunAsync(scriptPath, (JsonObject)request.DeepClone(), cancellationToken);
    }

    public async Task<PluginMapWriteResult> ReplaceActorsInRegionAndSaveAsync(
        PluginMapActorRegionWriteRequest request, CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(request);
        cancellationToken.ThrowIfCancellationRequested();
        if (Dispatcher.UIThread.CheckAccess())
            return replaceActorsInRegionAndSave(request, cancellationToken);
        return await Dispatcher.UIThread.InvokeAsync(() => replaceActorsInRegionAndSave(request, cancellationToken));
    }

    private PluginMapWriteResult replaceActorsInRegionAndSave(
        PluginMapActorRegionWriteRequest request, CancellationToken cancellationToken)
    {
        JsonObject? map = gameData.Maps.ReadMapSnapshot(request.MapKey);
        if (map is null)
            return PluginMapWriteResult.Failed("The selected map no longer exists.", string.Empty);
        string revision = createRevision(map);
        if (!string.Equals(request.BaseRevision, revision, StringComparison.Ordinal))
            return PluginMapWriteResult.Conflicted(revision);
        if (map["layers"]?[request.LayerName] is not JsonObject || !canWriteLayer(request.MapKey, request.LayerName))
            return PluginMapWriteResult.Failed("The selected map layer is missing or hidden.", revision);
        int width = readInt(map["width"], 0);
        int height = readInt(map["height"], 0);
        PluginMapRegion region = request.Region;
        if (region is null || region.X < 0 || region.Y < 0 || region.Width <= 0 || region.Height <= 0
            || (long)region.X + region.Width > width || (long)region.Y + region.Height > height)
            return PluginMapWriteResult.Failed("The Actor region is outside the map.", revision);
        if (string.IsNullOrWhiteSpace(request.PreserveBaseClassReference)
            || gameData.Metadata.GetRuntimeClassType(request.PreserveBaseClassReference) is null)
            return PluginMapWriteResult.Failed("The preserved Actor class cannot be resolved.", revision);
        if (map["actors"] is not null and not JsonObject
            || map["actors"]?[request.LayerName] is not null and not JsonArray
            || map["BPClassVarChanged"] is not null and not JsonObject)
            return PluginMapWriteResult.Failed("The map Actor data is invalid.", revision);
        if (request.Placements is null || request.Placements.Count == 0)
            return PluginMapWriteResult.Failed("At least one Actor placement is required.", revision);

        Dictionary<string, int> tagCounts = new(StringComparer.Ordinal);
        foreach (JsonArray group in (map["actors"] as JsonObject ?? []).Select(entry => entry.Value).OfType<JsonArray>())
            foreach (JsonObject actor in group.OfType<JsonObject>())
            {
                string tag = readString(actor["tag"]);
                tagCounts[tag] = tagCounts.GetValueOrDefault(tag) + 1;
            }
        HashSet<string> removedTags = new(StringComparer.Ordinal);
        HashSet<(int X, int Y)> occupied = [];
        foreach (KeyValuePair<string, JsonNode?> group in map["actors"] as JsonObject ?? [])
        {
            if (group.Key == request.LayerName || group.Value is not JsonArray actors)
                continue;
            foreach (JsonObject actor in actors.OfType<JsonObject>())
                if (tryReadActorPosition(actor, width, height, out int x, out int y))
                    occupied.Add((x, y));
        }
        JsonArray keptActors = [];
        using IDisposable batch = gameData.BlueprintClasses.BeginBatch();
        foreach (JsonNode? node in map["actors"]?[request.LayerName] as JsonArray ?? [])
        {
            if (node is not JsonObject actor || !tryReadActorPosition(actor, width, height, out int x, out int y))
                return PluginMapWriteResult.Failed("An Actor in the selected layer has an invalid position.", revision);
            bool inside = contains(region, x, y);
            if (inside)
            {
                string tag = readString(actor["tag"]);
                string reference = readString(actor["bp"]);
                if (tag.Length == 0 || tagCounts.GetValueOrDefault(tag) != 1 || reference.Length == 0
                    || BlueprintReference.IsReference(reference) && !gameData.Blueprints.ContainsReference(reference))
                    return PluginMapWriteResult.Failed("An Actor in the selected region has an invalid class or non-unique tag.", revision);
                ResolvedBlueprintClass resolved = gameData.BlueprintClasses.Resolve(reference);
                if (resolved.RootType is null)
                    return PluginMapWriteResult.Failed("An Actor class in the selected region cannot be resolved.", revision);
                if (!gameData.BlueprintClasses.IsDerivedFrom(resolved, request.PreserveBaseClassReference))
                {
                    removedTags.Add(tag);
                    continue;
                }
            }
            keptActors.Add(actor.DeepClone());
            occupied.Add((x, y));
        }

        IReadOnlyList<IReadOnlyList<PluginMapPassage>> passage = readPassage(map, width, height);
        HashSet<string> reservedTags = new(StringComparer.Ordinal);
        foreach (PluginMapActorPlacement placement in request.Placements)
        {
            if (placement is null || !contains(region, placement.X, placement.Y)
                || !occupied.Add((placement.X, placement.Y)) || !passage[placement.Y][placement.X].Passable)
                return PluginMapWriteResult.Failed("An Actor placement is blocked, repeated or outside the selected region.", revision);
            string reference = BlueprintReference.ToReference(placement.BlueprintReference);
            if (!gameData.Blueprints.ContainsReference(reference)
                || !gameData.BlueprintClasses.IsDerivedFrom(reference, "Engine.Actor"))
                return PluginMapWriteResult.Failed("A selected Actor Blueprint no longer exists or is invalid.", revision);
            string defaultTag = MapTagService.CreateDefault(gameData, request.MapKey, reference, placement.X, placement.Y);
            string tag = MapTagService.MakeUnique(defaultTag,
                value => reservedTags.Contains(value) || MapTagService.Exists(gameData, request.MapKey, value));
            reservedTags.Add(tag);
            keptActors.Add(new JsonObject
            {
                ["bp"] = reference,
                ["tag"] = tag,
                ["position"] = new JsonArray(placement.X, placement.Y),
            });
        }
        JsonObject candidate = (JsonObject)map.DeepClone();
        if (candidate["actors"] is not JsonObject actorGroups)
        {
            actorGroups = [];
            candidate["actors"] = actorGroups;
        }
        actorGroups[request.LayerName] = keptActors;
        if (candidate["BPClassVarChanged"] is JsonObject overrides)
        {
            foreach (string tag in removedTags)
                overrides.Remove(tag);
            if (overrides.Count == 0)
                candidate.Remove("BPClassVarChanged");
        }
        cancellationToken.ThrowIfCancellationRequested();
        SaveResult result = gameData.SaveMapCandidate(request.MapKey, candidate);
        if (!result.Success)
            return PluginMapWriteResult.Failed(result.Details, revision);
        JsonObject savedMap = gameData.Maps.ReadMapSnapshot(request.MapKey)!;
        refreshMap(request.MapKey, request.LayerName, true);
        return PluginMapWriteResult.Completed(createRevision(savedMap));
    }

    private static bool contains(PluginMapRegion region, int x, int y)
        => x >= region.X && y >= region.Y && (long)x < (long)region.X + region.Width && (long)y < (long)region.Y + region.Height;
}
