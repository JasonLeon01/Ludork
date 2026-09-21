using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class BlueprintDataService
{
    internal const string BlueprintClassPrefix = "Data.Blueprints.";

    public bool UpdateBlueprintAttributes(
        string key,
        IReadOnlyDictionary<string, JsonNode?> updates,
        IEnumerable<string> removals)
    {
        if (!blueprintDocuments.TryGetValue(ProjectDataStore.normalizeJsonKey(key), out JsonObject? blueprint))
            return false;
        JsonObject? current = blueprint["attrs"] as JsonObject;
        JsonObject next = current?.DeepClone() as JsonObject ?? [];
        foreach (string name in removals)
            next.Remove(name);
        foreach (KeyValuePair<string, JsonNode?> pair in updates)
            next[pair.Key] = pair.Value?.DeepClone();
        if (current is null && next.Count == 0 || JsonNode.DeepEquals(current, next))
            return false;
        blueprintDocuments.RecordChange(key);
        blueprint["attrs"] = next;
        store.refreshModifiedState();
        return true;
    }

    public bool UpdateBlueprintParent(string key, string parent)
    {
        string value = parent.Trim();
        if (value.Length == 0
            || !blueprintDocuments.TryGetValue(ProjectDataStore.normalizeJsonKey(key), out JsonObject? blueprint)
            || string.Equals(ProjectDataStore.getString(blueprint["parent"]), value, StringComparison.Ordinal))
        {
            return false;
        }
        blueprintDocuments.RecordChange(key);
        blueprint["parent"] = value;
        store.refreshModifiedState();
        return true;
    }

    public IReadOnlyList<string> GetBlueprintGraphNames(string key)
    {
        string normalizedKey = ProjectDataStore.normalizeJsonKey(key);
        if (!blueprintDocuments.TryGetValue(normalizedKey, out JsonObject? blueprint))
            return [];
        List<string> result = [];
        collectBlueprintGraphNames(blueprint, result, new HashSet<string>(StringComparer.Ordinal) { normalizedKey });
        return result;
    }

    public bool UpdateBlueprintEventGraph(string key, string eventName, BlueprintGraphSaveResult result)
    {
        return blueprintDocuments.TryGetValue(ProjectDataStore.normalizeJsonKey(key), out JsonObject? blueprint)
            && !string.IsNullOrWhiteSpace(eventName)
            && updateEventGraph("Blueprints", key, blueprint, "graph", eventName, result);
    }

    public bool AddBlueprintEvent(string key, string name)
    {
        string eventName = name.Trim();
        if (eventName.Length == 0 || char.IsDigit(eventName[0])
            || !blueprintDocuments.TryGetValue(ProjectDataStore.normalizeJsonKey(key), out JsonObject? blueprint)
            || GetBlueprintGraphNames(key).Contains(eventName, StringComparer.Ordinal))
        {
            return false;
        }
        JsonObject graph = blueprint["graph"]?.DeepClone() as JsonObject ?? [];
        ensureBlueprintObject(graph, "nodeGraph")[eventName] = createBlueprintEventGraph();
        ensureBlueprintObject(graph, "startNodes")[eventName] = null;
        blueprintDocuments.RecordChange(key);
        blueprint["graph"] = graph;
        store.refreshModifiedState();
        return true;
    }

    public bool RenameBlueprintEvent(string key, string oldName, string newName)
    {
        string eventName = newName.Trim();
        if (eventName.Length == 0 || char.IsDigit(eventName[0])
            || string.Equals(oldName, eventName, StringComparison.Ordinal)
            || !blueprintDocuments.TryGetValue(ProjectDataStore.normalizeJsonKey(key), out JsonObject? blueprint)
            || blueprint["graph"] is not JsonObject current
            || current["nodeGraph"] is not JsonObject nodeGraph || !nodeGraph.ContainsKey(oldName)
            || GetBlueprintGraphNames(key).Contains(eventName, StringComparer.Ordinal))
        {
            return false;
        }
        JsonObject graph = (JsonObject)current.DeepClone();
        graph["nodeGraph"] = renameBlueprintGraphKey(nodeGraph, oldName, eventName);
        if (current["startNodes"] is JsonObject startNodes)
            graph["startNodes"] = renameBlueprintGraphKey(startNodes, oldName, eventName);
        blueprintDocuments.RecordChange(key);
        blueprint["graph"] = graph;
        store.refreshModifiedState();
        return true;
    }

    public bool DeleteBlueprintEvent(string key, string name)
    {
        if (!blueprintDocuments.TryGetValue(ProjectDataStore.normalizeJsonKey(key), out JsonObject? blueprint)
            || blueprint["graph"] is not JsonObject current)
        {
            return false;
        }
        JsonObject graph = (JsonObject)current.DeepClone();
        bool removed = graph["nodeGraph"] is JsonObject nodeGraph && nodeGraph.Remove(name);
        if (graph["startNodes"] is JsonObject startNodes)
            removed |= startNodes.Remove(name);
        if (!removed)
            return false;
        blueprintDocuments.RecordChange(key);
        blueprint["graph"] = graph;
        store.refreshModifiedState();
        return true;
    }

    internal bool updateEventGraph(
        string section,
        string key,
        JsonObject target,
        string propertyName,
        string eventName,
        BlueprintGraphSaveResult result)
    {
        JsonObject? current = target[propertyName] as JsonObject;
        JsonObject? nodeGraph = current?["nodeGraph"] as JsonObject;
        JsonObject? startNodes = current?["startNodes"] as JsonObject;
        JsonObject eventGraph = nodeGraph?[eventName] as JsonObject ?? createBlueprintEventGraph();
        if (JsonNode.DeepEquals(eventGraph, result.EventGraph)
            && JsonNode.DeepEquals(startNodes?[eventName], result.StartNode))
        {
            return false;
        }
        JsonObject graph = current?.DeepClone() as JsonObject ?? [];
        ensureBlueprintObject(graph, "nodeGraph")[eventName] = result.EventGraph.DeepClone();
        ensureBlueprintObject(graph, "startNodes")[eventName] = result.StartNode?.DeepClone();
        store.RecordDocumentSnapshot(section, key);
        target[propertyName] = graph;
        store.refreshModifiedState();
        return true;
    }

    internal void collectBlueprintGraphNames(JsonObject blueprint, ICollection<string> result, ISet<string> visited)
    {
        string? parent = ProjectDataStore.getString(blueprint["parent"]);
        if (parent?.StartsWith(BlueprintClassPrefix, StringComparison.Ordinal) == true)
        {
            string parentKey = parent[BlueprintClassPrefix.Length..].Replace('.', '/');
            if (visited.Add(parentKey)
                && blueprintDocuments.TryGetValue(parentKey, out JsonObject? parentBlueprint))
            {
                collectBlueprintGraphNames(parentBlueprint, result, visited);
            }
        }
        if (blueprint["graph"] is not JsonObject graph || graph["nodeGraph"] is not JsonObject nodeGraph)
            return;
        foreach (string name in nodeGraph.Select(entry => entry.Key))
        {
            if (!result.Contains(name, StringComparer.Ordinal))
                result.Add(name);
        }
    }

    internal static JsonObject ensureBlueprintObject(JsonObject parent, string name)
    {
        if (parent[name] is JsonObject value)
            return value;
        value = [];
        parent[name] = value;
        return value;
    }

    internal static JsonObject createBlueprintEventGraph()
    {
        return new JsonObject { ["nodes"] = new JsonArray(), ["links"] = new JsonArray() };
    }

    internal static JsonObject renameBlueprintGraphKey(JsonObject source, string oldName, string newName)
    {
        JsonObject result = [];
        foreach (KeyValuePair<string, JsonNode?> entry in source)
            result[entry.Key == oldName ? newName : entry.Key] = entry.Value?.DeepClone();
        return result;
    }

}
