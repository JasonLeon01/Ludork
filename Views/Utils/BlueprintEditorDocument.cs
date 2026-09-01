using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils;

public enum BlueprintEditorDocumentKind
{
    Blueprint,
    GeneralDataAbility,
}

public sealed class BlueprintEditorDocument
{
    private const string BlueprintPrefix = "Data.Blueprints.";
    private readonly GameDataService gameData;
    private string? blueprintKey;
    private readonly string? generalTypeKey;
    private readonly string? generalMemberId;
    private readonly List<string> requiredEvents = [];
    private JsonObject data = [];

    private BlueprintEditorDocument(
        GameDataService gameData,
        BlueprintEditorDocumentKind kind,
        string? blueprintKey,
        string? generalTypeKey,
        string? generalMemberId)
    {
        this.gameData = gameData;
        Kind = kind;
        this.blueprintKey = blueprintKey;
        this.generalTypeKey = generalTypeKey;
        this.generalMemberId = generalMemberId;
        Reload();
    }

    public event EventHandler? Changed;

    public BlueprintEditorDocumentKind Kind { get; }
    public JsonObject Data => data;
    public IReadOnlyList<string> RequiredEvents => requiredEvents;
    public bool CanEditAttributes => Kind == BlueprintEditorDocumentKind.Blueprint;
    public bool CanEditGraphEvents => Kind == BlueprintEditorDocumentKind.Blueprint;
    public bool IsGraphOnly => Kind == BlueprintEditorDocumentKind.GeneralDataAbility;
    public string? BlueprintKey => blueprintKey;
    public string DocumentKey => Kind == BlueprintEditorDocumentKind.Blueprint
        ? "Blueprint:" + blueprintKey
        : GetGeneralDocumentKey(generalTypeKey ?? string.Empty, generalMemberId ?? string.Empty);
    public string Title => Kind == BlueprintEditorDocumentKind.Blueprint
        ? blueprintKey ?? string.Empty
        : $"General/{generalTypeKey}/{generalMemberId}";

    public static BlueprintEditorDocument? CreateBlueprint(GameDataService gameData, string reference)
    {
        string key = NormalizeBlueprintKey(reference);
        return key.Length != 0 && gameData.BlueprintsData.ContainsKey(key)
            ? new BlueprintEditorDocument(
                gameData,
                BlueprintEditorDocumentKind.Blueprint,
                key,
                null,
                null)
            : null;
    }

    public static BlueprintEditorDocument? CreateGeneralData(
        GameDataService gameData,
        string typeKey,
        string memberId)
    {
        if (!gameData.GeneralData.TryGetValue(typeKey, out JsonObject? typeData)
            || typeData["events"] is not JsonArray events
            || !events.Any(value => !string.IsNullOrWhiteSpace(getString(value)))
            || typeData["members"]?[memberId] is not JsonObject)
        {
            return null;
        }
        return new BlueprintEditorDocument(
            gameData,
            BlueprintEditorDocumentKind.GeneralDataAbility,
            null,
            typeKey,
            memberId);
    }

    public static string GetGeneralDocumentPrefix(string typeKey)
    {
        return $"GeneralData:{typeKey.Length}:{typeKey}:";
    }

    public static string GetGeneralDocumentKey(string typeKey, string memberId)
    {
        return GetGeneralDocumentPrefix(typeKey) + $"{memberId.Length}:{memberId}";
    }

    public static string NormalizeBlueprintKey(string reference)
    {
        string value = reference?.Trim() ?? string.Empty;
        if (value.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
            value = value[BlueprintPrefix.Length..].Replace('.', '/');
        value = value.Replace('\\', '/').Trim('/');
        if (value.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
            value = value[..^5];
        return value;
    }

    public bool RekeyBlueprint(string reference)
    {
        if (Kind != BlueprintEditorDocumentKind.Blueprint)
            return false;
        string key = NormalizeBlueprintKey(reference);
        if (key.Length == 0 || !gameData.BlueprintsData.ContainsKey(key))
            return false;
        blueprintKey = key;
        return true;
    }

    public bool Reload()
    {
        requiredEvents.Clear();
        if (Kind == BlueprintEditorDocumentKind.Blueprint)
        {
            if (blueprintKey is null
                || !gameData.BlueprintsData.TryGetValue(blueprintKey, out JsonObject? blueprint))
            {
                data = [];
                return false;
            }
            data = (JsonObject)blueprint.DeepClone();
            data["graph"] = normalizeGraph(data["graph"], []);
            return true;
        }

        JsonObject? typeData = getGeneralTypeData();
        JsonObject? member = getGeneralMember();
        if (typeData is null || member is null)
        {
            data = [];
            return false;
        }
        if (typeData["events"] is JsonArray events)
        {
            foreach (JsonNode? value in events)
            {
                string? name = getString(value);
                if (!string.IsNullOrWhiteSpace(name) && !requiredEvents.Contains(name, StringComparer.Ordinal))
                    requiredEvents.Add(name);
            }
        }
        data = new JsonObject
        {
            ["attrs"] = new JsonObject(),
            ["graph"] = normalizeGraph(member["_graph"], requiredEvents),
        };
        return true;
    }

    public IReadOnlyList<string> GetGraphNames()
    {
        List<string> result = [];
        if (Kind == BlueprintEditorDocumentKind.Blueprint)
            collectInheritedGraphNames(data["parent"]?.GetValue<string>(), result, new HashSet<string>(StringComparer.Ordinal));
        else
        {
            addUnique(result, requiredEvents);
            return result;
        }
        if (getNodeGraph(data) is JsonObject nodeGraph)
            addUnique(result, nodeGraph.Select(entry => entry.Key));
        return result;
    }

    public JsonObject GetEventGraph(string eventName)
    {
        JsonObject graph = ensureGraph(data);
        JsonObject nodeGraph = ensureObject(graph, "nodeGraph");
        JsonObject startNodes = ensureObject(graph, "startNodes");
        if (nodeGraph[eventName] is not JsonObject eventGraph)
        {
            eventGraph = createEmptyEventGraph();
            nodeGraph[eventName] = eventGraph;
        }
        ensureArray(eventGraph, "nodes");
        ensureArray(eventGraph, "links");
        if (!startNodes.ContainsKey(eventName))
            startNodes[eventName] = null;
        return eventGraph;
    }

    public bool CommitAttribute(string name, JsonNode? value)
    {
        if (!CanEditAttributes || getStoredBlueprint() is not JsonObject stored)
            return false;
        JsonObject storedAttrs = ensureObject(stored, "attrs");
        bool exists = storedAttrs.TryGetPropertyValue(name, out JsonNode? current);
        if (exists && JsonNode.DeepEquals(current, value))
            return false;
        gameData.RecordSnapshot();
        storedAttrs[name] = value?.DeepClone();
        JsonObject workingAttrs = ensureObject(data, "attrs");
        workingAttrs[name] = value?.DeepClone();
        gameData.refreshModifiedState();
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool CommitAttributes(
        IReadOnlyDictionary<string, JsonNode?> updates,
        IEnumerable<string> removals)
    {
        if (!CanEditAttributes || getStoredBlueprint() is not JsonObject stored)
            return false;
        JsonObject storedAttrs = ensureObject(stored, "attrs");
        List<string> removedNames = removals
            .Where(storedAttrs.ContainsKey)
            .Distinct(StringComparer.Ordinal)
            .ToList();
        List<KeyValuePair<string, JsonNode?>> changedValues = updates
            .Where(pair => !storedAttrs.TryGetPropertyValue(pair.Key, out JsonNode? current)
                || !JsonNode.DeepEquals(current, pair.Value))
            .ToList();
        if (removedNames.Count == 0 && changedValues.Count == 0)
            return false;

        gameData.RecordSnapshot();
        JsonObject workingAttrs = ensureObject(data, "attrs");
        foreach (string name in removedNames)
        {
            storedAttrs.Remove(name);
            workingAttrs.Remove(name);
        }
        foreach (KeyValuePair<string, JsonNode?> pair in changedValues)
        {
            storedAttrs[pair.Key] = pair.Value?.DeepClone();
            workingAttrs[pair.Key] = pair.Value?.DeepClone();
        }
        gameData.refreshModifiedState();
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool CommitParent(string parent)
    {
        string value = parent.Trim();
        if (!CanEditAttributes || value.Length == 0 || getStoredBlueprint() is not JsonObject stored)
            return false;
        string current = stored["parent"]?.GetValue<string>() ?? string.Empty;
        if (string.Equals(current, value, StringComparison.Ordinal))
            return false;
        gameData.RecordSnapshot();
        stored["parent"] = value;
        data["parent"] = value;
        gameData.refreshModifiedState();
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool RemoveAttribute(string name)
    {
        if (!CanEditAttributes || getStoredBlueprint() is not JsonObject stored
            || stored["attrs"] is not JsonObject storedAttrs || !storedAttrs.ContainsKey(name))
        {
            return false;
        }
        gameData.RecordSnapshot();
        storedAttrs.Remove(name);
        if (data["attrs"] is JsonObject workingAttrs)
            workingAttrs.Remove(name);
        gameData.refreshModifiedState();
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool CommitGraph()
    {
        JsonObject graph = normalizeGraph(data["graph"], requiredEvents);
        JsonObject? target = Kind == BlueprintEditorDocumentKind.Blueprint
            ? getStoredBlueprint()
            : getGeneralMember();
        if (target is null)
            return false;
        string propertyName = Kind == BlueprintEditorDocumentKind.Blueprint ? "graph" : "_graph";
        if (JsonNode.DeepEquals(target[propertyName], graph))
            return false;
        gameData.RecordSnapshot();
        target[propertyName] = graph.DeepClone();
        gameData.refreshModifiedState();
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool AddEvent(string name)
    {
        if (!CanEditGraphEvents)
            return false;
        string eventName = name.Trim();
        if (eventName.Length == 0 || char.IsDigit(eventName[0])
            || GetGraphNames().Contains(eventName, StringComparer.Ordinal))
        {
            return false;
        }
        GetEventGraph(eventName);
        return CommitGraph();
    }

    public bool RenameEvent(string oldName, string newName)
    {
        if (!CanEditGraphEvents)
            return false;
        string eventName = newName.Trim();
        if (eventName.Length == 0 || char.IsDigit(eventName[0])
            || string.Equals(oldName, eventName, StringComparison.Ordinal)
            || GetGraphNames().Contains(eventName, StringComparer.Ordinal))
        {
            return false;
        }
        JsonObject graph = ensureGraph(data);
        if (graph["nodeGraph"] is not JsonObject nodeGraph || !nodeGraph.ContainsKey(oldName))
            return false;
        graph["nodeGraph"] = renameObjectKey(nodeGraph, oldName, eventName);
        if (graph["startNodes"] is JsonObject startNodes)
            graph["startNodes"] = renameObjectKey(startNodes, oldName, eventName);
        return CommitGraph();
    }

    public bool DeleteEvent(string name)
    {
        if (!CanEditGraphEvents)
            return false;
        JsonObject graph = ensureGraph(data);
        bool removed = graph["nodeGraph"] is JsonObject nodeGraph && nodeGraph.Remove(name);
        if (graph["startNodes"] is JsonObject startNodes)
            removed |= startNodes.Remove(name);
        return removed && CommitGraph();
    }

    private JsonObject? getStoredBlueprint()
    {
        return blueprintKey is not null
            && gameData.BlueprintsData.TryGetValue(blueprintKey, out JsonObject? blueprint)
            ? blueprint
            : null;
    }

    private JsonObject? getGeneralTypeData()
    {
        return generalTypeKey is not null
            && gameData.GeneralData.TryGetValue(generalTypeKey, out JsonObject? typeData)
            ? typeData
            : null;
    }

    private JsonObject? getGeneralMember()
    {
        return generalMemberId is not null
            && getGeneralTypeData()?["members"]?[generalMemberId] is JsonObject member
            ? member
            : null;
    }

    private void collectInheritedGraphNames(
        string? reference,
        ICollection<string> result,
        ISet<string> visited)
    {
        if (string.IsNullOrWhiteSpace(reference)
            || !reference.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
        {
            return;
        }
        string key = NormalizeBlueprintKey(reference);
        if (!visited.Add(key) || !gameData.BlueprintsData.TryGetValue(key, out JsonObject? blueprint))
            return;
        collectInheritedGraphNames(blueprint["parent"]?.GetValue<string>(), result, visited);
        if (getNodeGraph(blueprint) is JsonObject nodeGraph)
            addUnique(result, nodeGraph.Select(entry => entry.Key));
    }

    private static JsonObject normalizeGraph(JsonNode? source, IEnumerable<string> events)
    {
        JsonObject graph = source is JsonObject sourceObject
            ? (JsonObject)sourceObject.DeepClone()
            : [];
        JsonObject nodeGraph = ensureObject(graph, "nodeGraph");
        JsonObject startNodes = ensureObject(graph, "startNodes");
        foreach (KeyValuePair<string, JsonNode?> entry in nodeGraph.ToArray())
        {
            JsonObject eventGraph = entry.Value as JsonObject ?? createEmptyEventGraph();
            ensureArray(eventGraph, "nodes");
            ensureArray(eventGraph, "links");
            if (!ReferenceEquals(entry.Value, eventGraph))
                nodeGraph[entry.Key] = eventGraph;
            if (!startNodes.ContainsKey(entry.Key))
                startNodes[entry.Key] = null;
        }
        foreach (string eventName in events)
        {
            if (nodeGraph[eventName] is not JsonObject eventGraph)
            {
                eventGraph = createEmptyEventGraph();
                nodeGraph[eventName] = eventGraph;
            }
            ensureArray(eventGraph, "nodes");
            ensureArray(eventGraph, "links");
            if (!startNodes.ContainsKey(eventName))
                startNodes[eventName] = null;
        }
        return graph;
    }

    private static JsonObject ensureGraph(JsonObject blueprint)
    {
        if (blueprint["graph"] is JsonObject graph)
            return graph;
        graph = [];
        blueprint["graph"] = graph;
        return graph;
    }

    private static JsonObject? getNodeGraph(JsonObject blueprint)
    {
        return blueprint["graph"]?["nodeGraph"] as JsonObject;
    }

    private static JsonObject ensureObject(JsonObject parent, string name)
    {
        if (parent[name] is JsonObject value)
            return value;
        value = [];
        parent[name] = value;
        return value;
    }

    private static JsonArray ensureArray(JsonObject parent, string name)
    {
        if (parent[name] is JsonArray value)
            return value;
        value = [];
        parent[name] = value;
        return value;
    }

    private static JsonObject createEmptyEventGraph()
    {
        return new JsonObject
        {
            ["nodes"] = new JsonArray(),
            ["links"] = new JsonArray(),
        };
    }

    private static JsonObject renameObjectKey(JsonObject source, string oldName, string newName)
    {
        JsonObject result = [];
        foreach (KeyValuePair<string, JsonNode?> entry in source)
            result[entry.Key == oldName ? newName : entry.Key] = entry.Value?.DeepClone();
        return result;
    }

    private static void addUnique(ICollection<string> target, IEnumerable<string> values)
    {
        foreach (string value in values)
        {
            if (!target.Contains(value, StringComparer.Ordinal))
                target.Add(value);
        }
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }
}
