using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    public bool MoveMapActors(string mapKey, IReadOnlyList<MapActorPositionEdit> positions)
    {
        if (positions.Count == 0 || getMap(mapKey) is not JsonObject map)
            return false;
        HashSet<MapActorReference> targets = [];
        Dictionary<string, Dictionary<string, int>> layerTagIndices = new(StringComparer.Ordinal);
        List<JsonDataEdit> edits = [];
        foreach (MapActorPositionEdit position in positions)
        {
            if (!targets.Add(position.Actor)
                || !tryFindEditableBatchActor(map, position.Actor, layerTagIndices, out int index)
                || !isMapPositionValid(map, position.X, position.Y))
                return false;
            edits.Add(new(JsonDataEdit.Operation.Set,
                ["actors", position.Actor.LayerName, index, "position"], new JsonArray(position.X, position.Y)));
        }
        return commitMapEdits(mapKey, map, edits);
    }

    public bool ApplyMapActorOverrides(string mapKey, IReadOnlyList<MapActorOverrideEdit> overrides)
        => ApplyMapActorOverrides(mapKey, overrides, null);

    internal bool ApplyMapActorOverrides(string mapKey, IReadOnlyList<MapActorOverrideEdit> overrides, object? origin)
    {
        if (overrides.Count == 0 || getMap(mapKey) is not JsonObject map
            || map["BPClassVarChanged"] is not null and not JsonObject)
            return false;
        Dictionary<string, Dictionary<string, int>> layerTagIndices = new(StringComparer.Ordinal);
        foreach (MapActorOverrideEdit change in overrides)
        {
            if (string.IsNullOrEmpty(change.Name)
                || !tryFindEditableBatchActor(map, change.Actor, layerTagIndices, out _)
                || map["BPClassVarChanged"]?[change.Actor.Tag] is not null and not JsonObject)
                return false;
        }
        JsonObject? root = map["BPClassVarChanged"] as JsonObject;
        Dictionary<string, ActorOverrideChanges> changes = new(StringComparer.Ordinal);
        foreach (MapActorOverrideEdit change in overrides)
        {
            if (!changes.TryGetValue(change.Actor.Tag, out ActorOverrideChanges? actorChanges))
            {
                actorChanges = new(root?[change.Actor.Tag] as JsonObject, root?.ContainsKey(change.Actor.Tag) == true);
                changes.Add(change.Actor.Tag, actorChanges);
            }
            bool exists = actorChanges.TryGetValue(change.Name, out JsonNode? previous);
            bool changed = change.Remove ? exists
                : !exists || !JsonNode.DeepEquals(previous, change.Value);
            if (!changed)
                continue;
            actorChanges.Set(change.Name, !change.Remove, change.Value);
            if (change.Name == "material")
                actorChanges.Set("materialParameters", true, new JsonObject());
        }
        int rootCount = root?.Count ?? 0;
        List<JsonDataEdit> edits = [];
        foreach ((string tag, ActorOverrideChanges actorChanges) in changes)
        {
            bool existed = root?.ContainsKey(tag) == true;
            rootCount += (actorChanges.Exists ? 1 : 0) - (existed ? 1 : 0);
            if (!actorChanges.Exists)
            {
                if (existed)
                    edits.Add(new(JsonDataEdit.Operation.Remove, ["BPClassVarChanged", tag]));
                continue;
            }
            foreach ((string name, (bool exists, JsonNode? value)) in actorChanges.Fields)
                edits.Add(new(exists ? JsonDataEdit.Operation.Set : JsonDataEdit.Operation.Remove,
                    ["BPClassVarChanged", tag, name], value));
        }
        if (rootCount == 0)
            edits = [new(JsonDataEdit.Operation.Remove, ["BPClassVarChanged"])];
        return commitMapEdits(mapKey, map, edits, origin);
    }

    private sealed class ActorOverrideChanges(JsonObject? original, bool exists)
    {
        private int count = original?.Count ?? 0;
        public bool Exists { get; private set; } = exists;
        public Dictionary<string, (bool Exists, JsonNode? Value)> Fields { get; } = new(StringComparer.Ordinal);

        public bool TryGetValue(string name, out JsonNode? value)
        {
            if (Fields.TryGetValue(name, out (bool Exists, JsonNode? Value) field))
            {
                value = field.Value;
                return field.Exists;
            }
            value = null;
            return original?.TryGetPropertyValue(name, out value) == true;
        }

        public void Set(string name, bool present, JsonNode? value)
        {
            bool previous = TryGetValue(name, out _);
            count += (present ? 1 : 0) - (previous ? 1 : 0);
            Fields[name] = (present, present ? value : null);
            Exists = count != 0;
        }
    }

    public bool DeleteMapActors(string mapKey, IReadOnlyList<MapActorReference> actors)
    {
        if (actors.Count == 0 || getMap(mapKey) is not JsonObject map
            || map["BPClassVarChanged"] is not null and not JsonObject)
            return false;
        HashSet<MapActorReference> targets = [];
        Dictionary<string, Dictionary<string, int>> layerTagIndices = new(StringComparer.Ordinal);
        Dictionary<string, List<int>> indices = new(StringComparer.Ordinal);
        foreach (MapActorReference actor in actors)
        {
            if (!targets.Add(actor) || !tryFindEditableBatchActor(map, actor, layerTagIndices, out int index))
                return false;
            if (!indices.TryGetValue(actor.LayerName, out List<int>? layerIndices))
            {
                layerIndices = [];
                indices.Add(actor.LayerName, layerIndices);
            }
            layerIndices.Add(index);
        }
        List<JsonDataEdit> edits = [];
        foreach (KeyValuePair<string, List<int>> layer in indices)
        {
            foreach (int index in layer.Value.OrderDescending())
                edits.Add(new(JsonDataEdit.Operation.Remove, ["actors", layer.Key, index]));
        }
        if (map["BPClassVarChanged"] is JsonObject overrides)
        {
            JsonObject root = (JsonObject)overrides.DeepClone();
            foreach (MapActorReference actor in actors)
                root.Remove(actor.Tag);
            edits.AddRange(createBatchActorOverridesEdits(map, root));
        }
        return commitMapEdits(mapKey, map, edits);
    }

    public IReadOnlyList<MapActorReference>? PasteMapActors(string mapKey, string layerName,
        IReadOnlyList<MapActorPasteEntry> entries)
    {
        if (entries.Count == 0 || getMap(mapKey) is not JsonObject map
            || map["layers"]?[layerName] is not JsonObject layer
            || !(layer["visible"]?.GetValue<bool?>() ?? true)
            || map["actors"] is not null and not JsonObject
            || map["actors"]?[layerName] is not null and not JsonArray
            || map["BPClassVarChanged"] is not null and not JsonObject)
            return null;
        JsonArray? actors = map["actors"]?[layerName] as JsonArray;
        HashSet<(int X, int Y)> occupied = [];
        if (actors is not null)
        {
            foreach (JsonObject actor in actors.OfType<JsonObject>())
            {
                if (actor["position"] is JsonArray { Count: >= 2 } position
                    && int.TryParse(position[0]?.ToString(), out int x)
                    && int.TryParse(position[1]?.ToString(), out int y))
                    occupied.Add((x, y));
            }
        }
        foreach (MapActorPasteEntry entry in entries)
        {
            if (!isMapPositionValid(map, entry.X, entry.Y)
                || !occupied.Add((entry.X, entry.Y))
                || string.IsNullOrEmpty(ProjectDataStore.getString(entry.ActorData["bp"])))
                return null;
        }
        JsonArray candidates = actors?.DeepClone() as JsonArray ?? [];
        JsonObject root = map["BPClassVarChanged"]?.DeepClone() as JsonObject ?? [];
        HashSet<string> reservedTags = new(StringComparer.Ordinal);
        List<MapActorReference> references = [];
        foreach (MapActorPasteEntry entry in entries)
        {
            string blueprint = ProjectDataStore.getString(entry.ActorData["bp"])!;
            string defaultTag = MapTagService.CreateDefault(store, mapKey, blueprint, entry.X, entry.Y);
            string tag = MapTagService.MakeUnique(defaultTag,
                candidate => reservedTags.Contains(candidate) || MapTagService.Exists(store, mapKey, candidate));
            reservedTags.Add(tag);
            JsonObject candidate = (JsonObject)entry.ActorData.DeepClone();
            candidate["tag"] = tag;
            candidate["position"] = new JsonArray(entry.X, entry.Y);
            candidates.Add(candidate);
            if (entry.Overrides is not null)
                root[tag] = entry.Overrides.DeepClone();
            references.Add(new MapActorReference(layerName, tag));
        }
        List<JsonDataEdit> edits = [new(JsonDataEdit.Operation.Set, ["actors", layerName], candidates)];
        if (entries.Any(entry => entry.Overrides is not null))
            edits.AddRange(createBatchActorOverridesEdits(map, root));
        return commitMapEdits(mapKey, map, edits) ? references.AsReadOnly() : null;
    }

    private static bool tryFindEditableBatchActor(JsonObject map, MapActorReference reference,
        Dictionary<string, Dictionary<string, int>> layerTagIndices, out int actorIndex)
    {
        actorIndex = -1;
        if (string.IsNullOrWhiteSpace(reference.Tag))
            return false;
        if (!layerTagIndices.TryGetValue(reference.LayerName, out Dictionary<string, int>? tagIndices))
        {
            if (map["layers"]?[reference.LayerName] is not JsonObject layer
                || !(layer["visible"]?.GetValue<bool?>() ?? true)
                || map["actors"]?[reference.LayerName] is not JsonArray actors)
                return false;
            tagIndices = new(actors.Count, StringComparer.Ordinal);
            for (int index = 0; index < actors.Count; index++)
            {
                if (actors[index] is not JsonObject actor
                    || ProjectDataStore.getString(actor["tag"]) is not string tag
                    || string.IsNullOrWhiteSpace(tag))
                    continue;
                if (!tagIndices.TryAdd(tag, index))
                    tagIndices[tag] = -1;
            }
            layerTagIndices.Add(reference.LayerName, tagIndices);
        }
        if (!tagIndices.TryGetValue(reference.Tag, out int resolvedIndex) || resolvedIndex < 0)
            return false;
        actorIndex = resolvedIndex;
        return true;
    }

    private static IReadOnlyList<JsonDataEdit> createBatchActorOverridesEdits(JsonObject map, JsonObject root)
    {
        if (root.Count == 0)
            return [new(JsonDataEdit.Operation.Remove, ["BPClassVarChanged"])];
        JsonObject? previous = map["BPClassVarChanged"] as JsonObject;
        List<JsonDataEdit> edits = [];
        foreach (KeyValuePair<string, JsonNode?> entry in root)
        {
            if (previous is null || !previous.ContainsKey(entry.Key) || !JsonNode.DeepEquals(previous[entry.Key], entry.Value))
                edits.Add(new(JsonDataEdit.Operation.Set, ["BPClassVarChanged", entry.Key], entry.Value));
        }
        if (previous is not null)
            foreach (string tag in previous.Select(entry => entry.Key).Where(tag => !root.ContainsKey(tag)))
                edits.Add(new(JsonDataEdit.Operation.Remove, ["BPClassVarChanged", tag]));
        return edits;
    }
}
