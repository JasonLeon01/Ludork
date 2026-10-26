using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed class MapHistoryReplayImpact
{
    private MapHistoryReplayImpact(IReadOnlyList<MapHistoryActorChange> actors,
        IReadOnlyList<ReferenceRecord> autoTiles, IReadOnlyList<MapHistoryActorChange> tags)
    {
        Actors = actors;
        AutoTiles = autoTiles;
        Tags = tags;
    }

    internal IReadOnlyList<MapHistoryActorChange> Actors { get; }
    internal IReadOnlyList<ReferenceRecord> AutoTiles { get; }
    internal IReadOnlyList<MapHistoryActorChange> Tags { get; }
    internal bool NeedsValidation => Actors.Any(actor => actor.Fields is null || actor.Fields.Count != 0)
        || AutoTiles.Count != 0 || Tags.Count != 0;

    internal static MapHistoryReplayImpact? Create(string mapKey, JsonObject source, JsonObject target,
        IReadOnlyList<JsonDataEdit> edits)
    {
        Dictionary<string, HashSet<(int X, int Y)>?> autoTiles = new(StringComparer.Ordinal);
        HashSet<string> actorLayers = new(StringComparer.Ordinal);
        HashSet<string> structuralLayers = new(StringComparer.Ordinal);
        Dictionary<string, HashSet<int>> renamedActors = new(StringComparer.Ordinal);
        HashSet<string> overrideTags = new(StringComparer.Ordinal);
        bool allActors = false;
        bool allOverrides = false;
        foreach (JsonDataEdit edit in edits)
        {
            IReadOnlyList<object> path = edit.Path;
            if (path[0] is "layers")
            {
                if (path.Count is not (3 or 5) || path[1] is not string layer
                    || path[2] is not ("tiles" or "autoTiles")
                    || edit.Kind == JsonDataEdit.Operation.Insert
                    || path.Count == 5 && (path[3] is not int y || y < 0 || path[4] is not int x || x < 0
                        || edit.Kind != JsonDataEdit.Operation.Set)
                    || source["layers"] is not JsonObject beforeLayers || beforeLayers[layer] is not JsonObject
                    || target["layers"] is not JsonObject afterLayers || afterLayers[layer] is not JsonObject)
                    return null;
                if (path[2] is not "autoTiles")
                    continue;
                if (path.Count == 3)
                    autoTiles[layer] = null;
                else if (!autoTiles.TryGetValue(layer, out HashSet<(int X, int Y)>? cells))
                    autoTiles[layer] = [((int)path[4], (int)path[3])];
                else
                    cells?.Add(((int)path[4], (int)path[3]));
            }
            else if (path[0] is "actors")
            {
                if (path.Count == 1)
                {
                    if (edit.Kind == JsonDataEdit.Operation.Insert)
                        return null;
                    allActors = true;
                    continue;
                }
                if (path.Count > 4 || path[1] is not string layer
                    || path.Count > 2 && (path[2] is not int index || index < 0)
                    || path.Count == 4 && (path[3] is not ("position" or "tag") || edit.Kind == JsonDataEdit.Operation.Insert)
                    || path.Count == 2 && edit.Kind == JsonDataEdit.Operation.Insert)
                    return null;
                actorLayers.Add(layer);
                if (path.Count <= 3)
                    structuralLayers.Add(layer);
                else if (path[3] is "tag")
                {
                    if (!renamedActors.TryGetValue(layer, out HashSet<int>? indices))
                        renamedActors[layer] = indices = [];
                    indices.Add((int)path[2]);
                }
            }
            else if (path[0] is "BPClassVarChanged")
            {
                if (path.Count > 3 || edit.Kind == JsonDataEdit.Operation.Insert
                    || path.Skip(1).Any(segment => segment is not string))
                    return null;
                if (path.Count == 1)
                    allOverrides = true;
                else
                    overrideTags.Add((string)path[1]);
            }
            else
                return null;
        }
        List<ReferenceRecord> references = [];
        foreach (KeyValuePair<string, HashSet<(int X, int Y)>?> area in autoTiles)
        {
            JsonNode? before = source["layers"]![area.Key]!["autoTiles"];
            JsonNode? after = target["layers"]![area.Key]!["autoTiles"];
            if (before is not null and not JsonArray || after is not null and not JsonArray)
                return null;
            if (area.Value is HashSet<(int X, int Y)> cells)
            {
                foreach ((int x, int y) in cells)
                    if (!addAutoTileReference(mapKey, area.Key, before as JsonArray, after as JsonArray, x, y, references))
                        return null;
            }
            else if (after is JsonArray rows)
            {
                for (int y = 0; y < rows.Count; y++)
                {
                    if (rows[y] is not null and not JsonArray)
                        return null;
                    if (rows[y] is not JsonArray row)
                        continue;
                    for (int x = 0; x < row.Count; x++)
                        if (!addAutoTileReference(mapKey, area.Key, before as JsonArray, rows, x, y, references))
                            return null;
                }
            }
        }
        if (!allActors && actorLayers.Count == 0 && !allOverrides && overrideTags.Count == 0)
            return new MapHistoryReplayImpact([], references.AsReadOnly(), []);
        if (!allActors && structuralLayers.Count == 0 && renamedActors.Count == 0
            && !allOverrides && overrideTags.Count == 0)
        {
            if (source["actors"] is not JsonObject beforeActors || target["actors"] is not JsonObject afterActors)
                return null;
            foreach (JsonDataEdit edit in edits.Where(edit => edit.Path[0] is "actors"))
            {
                string layer = (string)edit.Path[1];
                int index = (int)edit.Path[2];
                if (beforeActors[layer] is not JsonArray before || afterActors[layer] is not JsonArray after
                    || index >= before.Count || index >= after.Count
                    || before[index] is not JsonObject || after[index] is not JsonObject)
                    return null;
            }
            return new MapHistoryReplayImpact([], references.AsReadOnly(), []);
        }
        if (source["actors"] is not null and not JsonObject || target["actors"] is not null and not JsonObject
            || source["BPClassVarChanged"] is not null and not JsonObject
            || target["BPClassVarChanged"] is not null and not JsonObject
            || hasLegacyActors(source["layers"] as JsonObject) || hasLegacyActors(target["layers"] as JsonObject))
            return null;
        JsonObject? sourceActors = source["actors"] as JsonObject;
        JsonObject? targetActors = target["actors"] as JsonObject;
        JsonObject? sourceOverrides = source["BPClassVarChanged"] as JsonObject;
        JsonObject? targetOverrides = target["BPClassVarChanged"] as JsonObject;
        if (allActors)
        {
            actorLayers.UnionWith(sourceActors?.Select(pair => pair.Key) ?? []);
            actorLayers.UnionWith(targetActors?.Select(pair => pair.Key) ?? []);
            structuralLayers.UnionWith(actorLayers);
        }
        if (allOverrides)
        {
            overrideTags.UnionWith(sourceOverrides?.Select(pair => pair.Key) ?? []);
            overrideTags.UnionWith(targetOverrides?.Select(pair => pair.Key) ?? []);
        }
        overrideTags.RemoveWhere(tag => sameProperty(sourceOverrides, targetOverrides, tag));
        includeOverrideLayers(sourceActors, overrideTags, actorLayers);
        includeOverrideLayers(targetActors, overrideTags, actorLayers);
        List<MapHistoryActorChange> actorChanges = [];
        List<MapHistoryActorChange> tags = [];
        HashSet<string> targetTags = new(StringComparer.Ordinal);
        foreach (string layer in actorLayers)
        {
            if (!readActors(sourceActors?[layer], out ActorState[] before)
                || !readActors(targetActors?[layer], out ActorState[] after))
                return null;
            Dictionary<string, ActorState> previous = before.ToDictionary(actor => actor.Tag, StringComparer.Ordinal);
            Dictionary<int, ActorState> renamed = [];
            if (renamedActors.TryGetValue(layer, out HashSet<int>? indices))
            {
                if (structuralLayers.Contains(layer))
                    return null;
                foreach (int index in indices)
                {
                    if (index >= before.Length || index >= after.Length)
                        return null;
                    ActorState oldActor = before[index];
                    ActorState newActor = after[index];
                    if (oldActor.Tag == newActor.Tag)
                        continue;
                    if (!sameActorContent(oldActor.Actor, newActor.Actor))
                        return null;
                    renamed[index] = oldActor;
                    previous.Remove(oldActor.Tag);
                }
            }
            foreach (ActorState current in after)
            {
                targetTags.Add(current.Tag);
                ActorState? original = renamed.GetValueOrDefault(current.Index)
                    ?? previous.GetValueOrDefault(current.Tag);
                if (original is not null && original.Tag == current.Tag
                    && sameActorContent(original.Actor, current.Actor)
                    && JsonNode.DeepEquals(sourceOverrides?[original.Tag], targetOverrides?[current.Tag]))
                    continue;
                if (!readOverrides(targetOverrides, current.Tag, out JsonObject? nextOverrides))
                    return null;
                if (original is null)
                {
                    MapHistoryActorChange restored = new(layer, current.Index, current.Actor, nextOverrides, null);
                    actorChanges.Add(restored);
                    tags.Add(restored);
                    continue;
                }
                if (!readOverrides(sourceOverrides, original.Tag, out JsonObject? oldOverrides))
                    return null;
                HashSet<string> fields = new(oldOverrides?.Select(pair => pair.Key) ?? [], StringComparer.Ordinal);
                fields.UnionWith(nextOverrides?.Select(pair => pair.Key) ?? []);
                fields.RemoveWhere(field => sameProperty(oldOverrides, nextOverrides, field));
                bool contentChanged = !sameActorContent(original.Actor, current.Actor);
                MapHistoryActorChange change = new(layer, current.Index, current.Actor, nextOverrides, contentChanged ? null : fields);
                if (contentChanged || fields.Count != 0)
                    actorChanges.Add(change);
                if (original.Tag != current.Tag)
                    tags.Add(change);
            }
        }
        if (overrideTags.Any(tag => targetOverrides?[tag] is not null && !targetTags.Contains(tag)))
            return null;
        return new MapHistoryReplayImpact(actorChanges.AsReadOnly(), references.AsReadOnly(), tags.AsReadOnly());
    }

    private static bool addAutoTileReference(string mapKey, string layer, JsonArray? before, JsonArray? after,
        int x, int y, ICollection<ReferenceRecord> references)
    {
        if (!readCell(before, x, y, out JsonNode? previous) || !readCell(after, x, y, out JsonNode? current))
            return false;
        if (JsonNode.DeepEquals(previous, current) || current is null)
            return true;
        if (current is not JsonValue scalar || !scalar.TryGetValue(out string? key))
            return false;
        if (!string.IsNullOrWhiteSpace(key))
            references.Add(new ReferenceRecord(ReferenceIdentity.NodeId("map", mapKey),
                ReferenceIdentity.NodeId("autoTile", key), "autoTile", $"Maps/{mapKey}.layers.{layer}.autoTiles[{y}][{x}]",
                StoragePath: $"/layers/{layer.Replace("~", "~0").Replace("/", "~1")}/autoTiles/{y}/{x}"));
        return true;
    }

    private static bool readCell(JsonArray? rows, int x, int y, out JsonNode? cell)
    {
        cell = null;
        if (rows is null || y >= rows.Count || rows[y] is null)
            return true;
        if (rows[y] is not JsonArray row)
            return false;
        if (x < row.Count)
            cell = row[x];
        return true;
    }

    private static void includeOverrideLayers(JsonObject? actors, IReadOnlySet<string> tags, ISet<string> layers)
    {
        if (actors is null || tags.Count == 0)
            return;
        foreach (KeyValuePair<string, JsonNode?> layer in actors)
            if (layer.Value is JsonArray entries && entries.OfType<JsonObject>()
                .Any(actor => JsonScalar.String(actor["tag"]) is string tag && tags.Contains(tag)))
                layers.Add(layer.Key);
    }

    private static bool hasLegacyActors(JsonObject? layers)
        => layers?.Any(layer => layer.Value is JsonObject data
            && (data["actors"] is not null and not JsonArray || data["actors"] is JsonArray { Count: > 0 })) == true;

    private static bool readActors(JsonNode? value, out ActorState[] actors)
    {
        actors = [];
        if (value is null)
            return true;
        if (value is not JsonArray entries)
            return false;
        List<ActorState> result = [];
        HashSet<string> tags = new(StringComparer.Ordinal);
        for (int index = 0; index < entries.Count; index++)
        {
            if (entries[index] is not JsonObject actor || JsonScalar.String(actor["tag"]) is not string tag
                || string.IsNullOrWhiteSpace(tag) || !tags.Add(tag))
                return false;
            result.Add(new ActorState(index, tag, actor));
        }
        actors = result.ToArray();
        return true;
    }

    private static bool readOverrides(JsonObject? overrides, string tag, out JsonObject? value)
    {
        JsonNode? node = overrides?[tag];
        value = node as JsonObject;
        return node is null or JsonObject;
    }

    private static bool sameProperty(JsonObject? before, JsonObject? after, string name)
    {
        JsonNode? oldValue = null;
        JsonNode? newValue = null;
        bool oldExists = before?.TryGetPropertyValue(name, out oldValue) == true;
        bool newExists = after?.TryGetPropertyValue(name, out newValue) == true;
        return oldExists == newExists && JsonNode.DeepEquals(oldValue, newValue);
    }

    private static bool sameActorContent(JsonObject before, JsonObject after)
    {
        return before.Select(pair => pair.Key).Concat(after.Select(pair => pair.Key)).Distinct(StringComparer.Ordinal)
            .Where(name => name is not ("position" or "tag")).All(name => sameProperty(before, after, name));
    }

    private sealed record ActorState(int Index, string Tag, JsonObject Actor);
}
