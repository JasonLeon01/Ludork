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
    private const string BlueprintPrefix = "Data.Blueprints.";
    private readonly ProjectDataStore gameData;
    private readonly EditorDocument? resourceDocument;
    private readonly string? initialBlueprintKey;
    private readonly string? initialGeneralTypeKey;
    private string? blueprintKey => Kind == BlueprintEditorDocumentKind.Blueprint ? resourceDocument?.Key ?? initialBlueprintKey : null;
    private string? generalTypeKey => Kind == BlueprintEditorDocumentKind.GeneralDataAbility ? resourceDocument?.Key ?? initialGeneralTypeKey : null;
    private JsonObject? sourceData;
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
        string key = NormalizeBlueprintKey(reference);
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
        if (key.Length == 0 || !gameData.Blueprints.BlueprintsData.ContainsKey(key))
            return false;
        return string.Equals(resourceDocument?.Key, key, StringComparison.Ordinal);
    }

    public bool Reload()
    {
        sourceData = resourceDocument?.Data;
        requiredEvents.Clear();
        if (Kind == BlueprintEditorDocumentKind.Blueprint)
        {
            if (blueprintKey is null
                || !gameData.Blueprints.BlueprintsData.TryGetValue(blueprintKey, out BlueprintDefinitionSnapshot? blueprint))
            {
                data = [];
                return false;
            }
            data = blueprint.ToJson();
            return true;
        }

        GeneralDataTypeSnapshot? typeData = getGeneralTypeData();
        JsonObject? member = getGeneralMember();
        if (typeData is null || member is null)
        {
            data = [];
            return false;
        }
        foreach (string name in typeData.Events)
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
        return CommitAttributes(new Dictionary<string, JsonNode?>(), [name]);
    }

    public bool AddEvent(string name)
    {
        if (!CanEditGraphEvents || blueprintKey is null || !mutate(() => gameData.Blueprints.AddBlueprintEvent(blueprintKey, name)))
        {
            return false;
        }
        string eventName = name.Trim();
        JsonObject graph = ensureGraph(data);
        ensureObject(graph, "nodeGraph")[eventName] = createEmptyEventGraph();
        ensureObject(graph, "startNodes")[eventName] = null;
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
            sourceData = resourceDocument?.Data;
        }
    }

    private void onResourceChanged(object? sender, EventArgs args)
    {
        if (committing || JsonNode.DeepEquals(sourceData, resourceDocument?.Data))
            return;
        Reload();
        Changed?.Invoke(this, EventArgs.Empty);
        ExternalChanged?.Invoke(this, EventArgs.Empty);
    }

    private JsonObject? getStoredBlueprint()
    {
        return blueprintKey is not null
            && gameData.Blueprints.BlueprintsData.TryGetValue(blueprintKey, out BlueprintDefinitionSnapshot? blueprint)
            ? blueprint.ToJson()
            : null;
    }

    private GeneralDataTypeSnapshot? getGeneralTypeData()
    {
        return generalTypeKey is not null
            && gameData.General.GeneralData.TryGetValue(generalTypeKey, out GeneralDataTypeSnapshot? typeData)
            ? typeData
            : null;
    }

    private JsonObject? getGeneralMember()
    {
        return generalMemberId is not null
            && getGeneralTypeData()?.Members.TryGetValue(generalMemberId, out JsonObject? member) == true
            ? member
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
