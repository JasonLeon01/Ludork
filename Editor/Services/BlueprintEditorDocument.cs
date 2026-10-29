using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public enum BlueprintEditorDocumentKind
{
    Blueprint,
    GeneralDataAbility,
}

public sealed class BlueprintEditorDocument : IDisposable
{
    private readonly ProjectDataStore gameData;
    private readonly EditorDocument? resourceDocument;
    private readonly string? initialBlueprintKey;
    private readonly string? initialGeneralTypeKey;
    private string? blueprintKey => Kind == BlueprintEditorDocumentKind.Blueprint ? resourceDocument?.Key ?? initialBlueprintKey : null;
    private string? generalTypeKey => Kind == BlueprintEditorDocumentKind.GeneralDataAbility ? resourceDocument?.Key ?? initialGeneralTypeKey : null;
    private long sourceRevision;
    private bool committing;
    private readonly string? generalMemberId;
    private readonly List<string> requiredEvents = [];
    private JsonObject data = [];

    private BlueprintEditorDocument(
        ProjectDataStore gameData,
        BlueprintEditorDocumentKind kind,
        string? blueprintKey,
        string? generalTypeKey,
        string? generalMemberId)
    {
        this.gameData = gameData;
        Kind = kind;
        initialBlueprintKey = blueprintKey;
        initialGeneralTypeKey = generalTypeKey;
        resourceDocument = gameData.GetDocument(kind == BlueprintEditorDocumentKind.Blueprint ? "Blueprints" : "General",
            blueprintKey ?? generalTypeKey ?? string.Empty);
        if (resourceDocument is not null)
            resourceDocument.Changed += onResourceChanged;
        this.generalMemberId = generalMemberId;
        Reload();
    }

    public event EventHandler? Changed;
    public event EventHandler? ExternalChanged;
    internal event EventHandler<BlueprintGraphParametersChangedEventArgs>? GraphParametersChanged;
    public EditorDocument? ResourceDocument => resourceDocument;

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

    public static BlueprintEditorDocument? CreateBlueprint(ProjectDataStore gameData, string reference)
    {
        string key = BlueprintReference.NormalizeKey(reference);
        return key.Length != 0 && gameData.Blueprints.BlueprintsData.ContainsKey(key)
            ? new BlueprintEditorDocument(
                gameData,
                BlueprintEditorDocumentKind.Blueprint,
                key,
                null,
                null)
            : null;
    }

    public static BlueprintEditorDocument? CreateGeneralData(
        ProjectDataStore gameData,
        string typeKey,
        string memberId)
    {
        if (!gameData.General.GeneralData.TryGetValue(typeKey, out GeneralDataTypeSnapshot? typeData)
            || typeData.Events.Count == 0
            || !typeData.Members.ContainsKey(memberId))
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

    public bool RekeyBlueprint(string reference)
    {
        if (Kind != BlueprintEditorDocumentKind.Blueprint)
            return false;
        string key = BlueprintReference.NormalizeKey(reference);
        if (key.Length == 0 || !gameData.Blueprints.BlueprintsData.ContainsKey(key))
            return false;
        return string.Equals(resourceDocument?.Key, key, StringComparison.Ordinal);
    }

    public bool Reload()
    {
        sourceRevision = resourceDocument?.Revision ?? 0;
        requiredEvents.Clear();
        if (Kind == BlueprintEditorDocumentKind.Blueprint)
        {
            if (resourceDocument?.InternalData is not JsonObject blueprint)
            {
                data = [];
                return false;
            }
            data = (JsonObject)blueprint.DeepClone();
            return true;
        }

        JsonObject? typeData = resourceDocument?.InternalData;
        JsonObject? member = generalMemberId is null ? null : typeData?["members"]?[generalMemberId] as JsonObject;
        if (typeData is null || member is null)
        {
            data = [];
            return false;
        }
        IEnumerable<string> eventNames = (typeData["events"] as JsonArray ?? []).OfType<JsonValue>()
            .Select(value => value.TryGetValue(out string? text) ? text : null)
            .Where(text => !string.IsNullOrWhiteSpace(text)).Cast<string>();
        foreach (string name in eventNames)
        {
            if (!requiredEvents.Contains(name, StringComparer.Ordinal))
                requiredEvents.Add(name);
        }
        data = new JsonObject
        {
            ["attrs"] = new JsonObject(),
            ["graph"] = member["_graph"]?.DeepClone(),
        };
        return true;
    }

    public IReadOnlyList<string> GetGraphNames()
    {
        return Kind == BlueprintEditorDocumentKind.Blueprint
            ? gameData.Blueprints.GetBlueprintGraphNames(blueprintKey ?? string.Empty)
            : requiredEvents.ToArray();
    }

    public JsonObject GetEventGraph(string eventName)
    {
        return getNodeGraph(data)?[eventName] as JsonObject ?? createEmptyEventGraph();
    }

    public bool CommitEventGraph(string eventName, BlueprintGraphSaveResult result)
    {
        JsonObject? currentGraph = data["graph"] as JsonObject;
        JsonObject? currentStartNodes = currentGraph?["startNodes"] as JsonObject;
        if (JsonNode.DeepEquals(GetEventGraph(eventName), result.EventGraph)
            && JsonNode.DeepEquals(currentStartNodes?[eventName], result.StartNode))
        {
            return false;
        }
        bool changed = Kind == BlueprintEditorDocumentKind.Blueprint
            ? blueprintKey is not null && mutate(() => gameData.Blueprints.UpdateBlueprintEventGraph(blueprintKey, eventName, result))
            : generalTypeKey is not null && generalMemberId is not null
                && mutate(() => gameData.General.UpdateGeneralMemberEventGraph(generalTypeKey, generalMemberId, eventName, result));
        if (!changed)
            return false;
        JsonObject graph = ensureGraph(data);
        ensureObject(graph, "nodeGraph")[eventName] = result.EventGraph.DeepClone();
        ensureObject(graph, "startNodes")[eventName] = result.StartNode?.DeepClone();
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    internal bool CommitGraph(string eventName, BlueprintGraphDocument graphDocument)
    {
        IReadOnlyList<JsonDataEdit>? changes = BlueprintGraphCodec.GetParameterEdits(graphDocument);
        if (changes is null || getNodeGraph(data)?[eventName] is not JsonObject)
        {
            bool changed = CommitEventGraph(eventName, BlueprintGraphCodec.Save(graphDocument));
            graphDocument.AcceptChanges();
            return changed;
        }
        object[] localPrefix = ["graph", "nodeGraph", eventName];
        object[] resourcePrefix = Kind == BlueprintEditorDocumentKind.Blueprint
            ? localPrefix : ["members", generalMemberId!, "_graph", "nodeGraph", eventName];
        JsonDataEdit[] edits = changes.Select(edit => edit.WithPath(resourcePrefix.Concat(edit.Path))).ToArray();
        bool applied = resourceDocument is not null && mutate(() => gameData.ApplyDocumentEdits(
            resourceDocument.Section, resourceDocument.Key, edits, origin: this));
        if (applied)
        {
            foreach (JsonDataEdit edit in changes)
                edit.WithPath(localPrefix.Concat(edit.Path)).Apply(data);
            Changed?.Invoke(this, EventArgs.Empty);
        }
        graphDocument.AcceptChanges();
        return applied;
    }

    public bool CommitAttribute(string name, JsonNode? value)
    {
        return CommitAttributes(new Dictionary<string, JsonNode?> { [name] = value }, []);
    }

    public bool CommitAttributes(
        IReadOnlyDictionary<string, JsonNode?> updates,
        IEnumerable<string> removals)
    {
        string[] removedNames = removals.ToArray();
        if (!CanEditAttributes || blueprintKey is null
            || !mutate(() => gameData.Blueprints.UpdateBlueprintAttributes(blueprintKey, updates, removedNames)))
        {
            return false;
        }
        JsonObject workingAttrs = ensureObject(data, "attrs");
        foreach (string name in removedNames)
            workingAttrs.Remove(name);
        foreach (KeyValuePair<string, JsonNode?> pair in updates)
            workingAttrs[pair.Key] = pair.Value?.DeepClone();
        if (!updates.ContainsKey("materialParameters") && (updates.ContainsKey("material") || removedNames.Contains("material")))
            workingAttrs["materialParameters"] = new JsonObject();
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool CommitParent(string parent)
    {
        if (!CanEditAttributes || blueprintKey is null || !mutate(() => gameData.Blueprints.UpdateBlueprintParent(blueprintKey, parent)))
            return false;
        data["parent"] = parent.Trim();
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool RemoveAttribute(string name)
    {
        if (!CanEditAttributes || blueprintKey is null
            || !mutate(() => gameData.Blueprints.RemoveBlueprintAttribute(blueprintKey, name)))
            return false;
        Reload();
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool AddAttribute(string name, JsonObject? definition, JsonNode? value)
    {
        if (!CanEditAttributes || blueprintKey is null
            || !mutate(() => gameData.Blueprints.AddBlueprintAttribute(blueprintKey, name, definition, value)))
            return false;
        Reload();
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool AddEvent(string name, BlueprintGraphSaveResult initial)
    {
        if (!CanEditGraphEvents || blueprintKey is null || !mutate(() => gameData.Blueprints.AddBlueprintEvent(blueprintKey, name, initial)))
        {
            return false;
        }
        string eventName = name.Trim();
        JsonObject graph = ensureGraph(data);
        ensureObject(graph, "nodeGraph")[eventName] = initial.EventGraph.DeepClone();
        ensureObject(graph, "startNodes")[eventName] = initial.StartNode?.DeepClone();
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool RenameEvent(string oldName, string newName)
    {
        if (!CanEditGraphEvents || blueprintKey is null
            || !mutate(() => gameData.Blueprints.RenameBlueprintEvent(blueprintKey, oldName, newName)))
        {
            return false;
        }
        string eventName = newName.Trim();
        JsonObject graph = ensureGraph(data);
        graph["nodeGraph"] = renameObjectKey(ensureObject(graph, "nodeGraph"), oldName, eventName);
        if (graph["startNodes"] is JsonObject startNodes)
            graph["startNodes"] = renameObjectKey(startNodes, oldName, eventName);
        JsonObject? storedGraph = getStoredBlueprint()?["graph"] as JsonObject;
        ensureObject(graph, "nodeGraph")[eventName] = storedGraph?["nodeGraph"]?[eventName]?.DeepClone();
        if (storedGraph?["startNodes"] is JsonObject storedStartNodes
            && storedStartNodes.TryGetPropertyValue(eventName, out JsonNode? startNode))
        {
            ensureObject(graph, "startNodes")[eventName] = startNode?.DeepClone();
        }
        else
        {
            (graph["startNodes"] as JsonObject)?.Remove(eventName);
        }
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool DeleteEvent(string name)
    {
        if (!CanEditGraphEvents || blueprintKey is null || !mutate(() => gameData.Blueprints.DeleteBlueprintEvent(blueprintKey, name)))
            return false;
        if (data["graph"] is JsonObject graph)
        {
            (graph["nodeGraph"] as JsonObject)?.Remove(name);
            (graph["startNodes"] as JsonObject)?.Remove(name);
        }
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public void Dispose()
    {
        if (resourceDocument is not null)
            resourceDocument.Changed -= onResourceChanged;
    }

    private bool mutate(Func<bool> operation)
    {
        committing = true;
        try
        {
            return operation();
        }
        finally
        {
            committing = false;
            sourceRevision = resourceDocument?.Revision ?? 0;
        }
    }

    private void onResourceChanged(object? sender, EventArgs args)
    {
        if (committing || resourceDocument is null || sourceRevision == resourceDocument.Revision)
            return;
        sourceRevision = resourceDocument.Revision;
        EditorDocumentLiveChange? change = resourceDocument.LastChange;
        if (change is null || ReferenceEquals(change.Origin, this))
            return;
        if (change.Edits is IReadOnlyList<JsonDataEdit> edits && synchronizeEdits(edits))
            return;
        Reload();
        Changed?.Invoke(this, EventArgs.Empty);
        ExternalChanged?.Invoke(this, EventArgs.Empty);
    }

    private bool synchronizeEdits(IReadOnlyList<JsonDataEdit> edits)
    {
        List<JsonDataEdit> localEdits = [];
        foreach (JsonDataEdit edit in edits)
        {
            if (Kind == BlueprintEditorDocumentKind.Blueprint)
                localEdits.Add(edit);
            else if (edit.Path.Count >= 3 && edit.Path[0] is "members" && edit.Path[1] is string member)
            {
                if (member != generalMemberId)
                    continue;
                if (edit.Path[2] is not "_graph")
                    continue;
                localEdits.Add(edit.WithPath(new object[] { "graph" }.Concat(edit.Path.Skip(3))));
            }
            else
                return false;
        }
        if (localEdits.Any(edit => !edit.CanApply(data)))
            return false;
        foreach (JsonDataEdit edit in localEdits)
            edit.Apply(data);
        if (localEdits.Count == 0)
            return true;
        Changed?.Invoke(this, EventArgs.Empty);
        if (localEdits.All(edit => edit.Kind == JsonDataEdit.Operation.Set && edit.Path.Count == 6
            && edit.Path[0] is "graph" && edit.Path[1] is "nodeGraph" && edit.Path[2] is string
            && edit.Path[3] is "nodes" && edit.Path[4] is int && edit.Path[5] is "params"))
        {
            foreach (IGrouping<string, JsonDataEdit> graph in localEdits.GroupBy(edit => (string)edit.Path[2]))
                GraphParametersChanged?.Invoke(this, new BlueprintGraphParametersChangedEventArgs(graph.Key,
                    graph.Select(edit => (int)edit.Path[4]).Distinct().ToArray()));
        }
        else
            ExternalChanged?.Invoke(this, EventArgs.Empty);
        return true;
    }

    private JsonObject? getStoredBlueprint()
    {
        return blueprintKey is not null
            && gameData.Blueprints.BlueprintsData.TryGetValue(blueprintKey, out BlueprintDefinitionSnapshot? blueprint)
            ? blueprint.ToJson()
            : null;
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
        return blueprint["graph"] is JsonObject graph ? graph["nodeGraph"] as JsonObject : null;
    }

    private static JsonObject ensureObject(JsonObject parent, string name)
    {
        if (parent[name] is JsonObject value)
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

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }
}
