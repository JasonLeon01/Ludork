using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class GeneralDataService
{
    public bool UpdateGeneralMemberValue(string typeKey, string memberId, string name, JsonNode? value)
    {
        if (!generalDocuments.TryGetValue(typeKey, out JsonObject? type)
            || type["params"] is not JsonObject parameters || !parameters.ContainsKey(name)
            || type["members"]?[memberId] is not JsonObject member
            || member.TryGetPropertyValue(name, out JsonNode? current) && JsonNode.DeepEquals(current, value))
        {
            return false;
        }
        JsonNode? next = value?.DeepClone();
        generalDocuments.RecordChange(typeKey);
        member[name] = next;
        store.refreshModifiedState();
        return true;
    }

    public bool CreateGeneralMember(string typeKey, string memberId)
    {
        if (string.IsNullOrWhiteSpace(memberId)
            || !generalDocuments.TryGetValue(typeKey, out JsonObject? type))
        {
            return false;
        }
        JsonObject members = type["members"] as JsonObject ?? new JsonObject();
        if (members.ContainsKey(memberId))
            return false;
        JsonObject member = [];
        if (type["params"] is JsonObject parameters)
        {
            foreach (KeyValuePair<string, JsonNode?> parameter in parameters)
            {
                if (parameter.Value is JsonObject definition)
                    member[parameter.Key] = createGeneralMemberDefaultValue(definition);
            }
        }
        generalDocuments.RecordChange(typeKey);
        members[memberId] = member;
        type["members"] = members;
        store.refreshModifiedState();
        return true;
    }

    public bool RenameGeneralMember(string typeKey, string oldId, string newId)
    {
        if (string.IsNullOrWhiteSpace(newId)
            || !generalDocuments.TryGetValue(typeKey, out JsonObject? type)
            || type["members"] is not JsonObject members
            || !members.ContainsKey(oldId) || members.ContainsKey(newId))
        {
            return false;
        }
        generalDocuments.RecordChange(typeKey);
        replaceGeneralObjectKey(members, oldId, newId, members[oldId]);
        store.refreshModifiedState();
        return true;
    }

    public bool DuplicateGeneralMember(string typeKey, string sourceId, string newId)
    {
        if (string.IsNullOrWhiteSpace(newId)
            || !generalDocuments.TryGetValue(typeKey, out JsonObject? type)
            || type["members"] is not JsonObject members
            || members[sourceId] is not JsonObject source || members.ContainsKey(newId))
        {
            return false;
        }
        JsonObject copy = (JsonObject)source.DeepClone();
        generalDocuments.RecordChange(typeKey);
        members[newId] = copy;
        store.refreshModifiedState();
        return true;
    }

    public bool DeleteGeneralMember(string typeKey, string memberId)
    {
        if (!generalDocuments.TryGetValue(typeKey, out JsonObject? type)
            || type["members"] is not JsonObject members || !members.ContainsKey(memberId))
        {
            return false;
        }
        generalDocuments.RecordChange(typeKey);
        members.Remove(memberId);
        store.refreshModifiedState();
        return true;
    }

    public bool AddGeneralParameter(string typeKey, string name, JsonObject definition)
    {
        if (string.IsNullOrWhiteSpace(name)
            || !generalDocuments.TryGetValue(typeKey, out JsonObject? type))
        {
            return false;
        }
        JsonObject parameters = type["params"] as JsonObject ?? new JsonObject();
        if (parameters.ContainsKey(name))
            return false;
        JsonObject next = (JsonObject)definition.DeepClone();
        JsonNode? defaultValue = createGeneralMemberDefaultValue(next);
        generalDocuments.RecordChange(typeKey);
        parameters[name] = next;
        type["params"] = parameters;
        foreach (JsonObject member in getGeneralMembers(type))
        {
            if (!member.ContainsKey(name))
                member[name] = defaultValue?.DeepClone();
        }
        store.refreshModifiedState();
        return true;
    }

    public bool UpdateGeneralParameter(
        string typeKey,
        string oldName,
        string newName,
        JsonObject definition,
        bool resetMemberValues)
    {
        if (string.IsNullOrWhiteSpace(newName)
            || !generalDocuments.TryGetValue(typeKey, out JsonObject? type)
            || type["params"] is not JsonObject parameters
            || parameters[oldName] is not JsonObject current
            || oldName != newName && parameters.ContainsKey(newName)
            || oldName != newName && getGeneralMembers(type).Any(member => member.ContainsKey(newName))
            || oldName == newName && JsonNode.DeepEquals(current, definition))
        {
            return false;
        }
        JsonObject next = (JsonObject)definition.DeepClone();
        JsonNode? defaultValue = resetMemberValues ? createGeneralMemberDefaultValue(next) : null;
        generalDocuments.RecordChange(typeKey);
        replaceGeneralObjectKey(parameters, oldName, newName, next);
        foreach (JsonObject member in getGeneralMembers(type))
        {
            if (member.ContainsKey(oldName))
            {
                JsonNode? nextValue = resetMemberValues ? defaultValue?.DeepClone() : member[oldName];
                replaceGeneralObjectKey(member, oldName, newName, nextValue);
            }
        }
        store.refreshModifiedState();
        return true;
    }

    public bool DeleteGeneralParameter(string typeKey, string name)
    {
        if (!generalDocuments.TryGetValue(typeKey, out JsonObject? type)
            || type["params"] is not JsonObject parameters || !parameters.ContainsKey(name))
        {
            return false;
        }
        generalDocuments.RecordChange(typeKey);
        parameters.Remove(name);
        foreach (JsonObject member in getGeneralMembers(type))
            member.Remove(name);
        store.refreshModifiedState();
        return true;
    }

    public bool UpdateGeneralParameterReference(string typeKey, string name, JsonObject? reference)
    {
        if (!generalDocuments.TryGetValue(typeKey, out JsonObject? type)
            || type["params"]?[name] is not JsonObject parameter
            || JsonNode.DeepEquals(parameter["reference"], reference))
        {
            return false;
        }
        JsonObject? next = reference?.DeepClone() as JsonObject;
        generalDocuments.RecordChange(typeKey);
        if (next is null)
            parameter.Remove("reference");
        else
            parameter["reference"] = next;
        store.refreshModifiedState();
        return true;
    }

    public bool AddGeneralEvent(string typeKey, string name)
    {
        string eventName = name.Trim();
        if (!isValidGeneralEventName(eventName)
            || !generalDocuments.TryGetValue(typeKey, out JsonObject? type)
            || type["events"] is JsonArray current && current.Any(value => ProjectDataStore.getString(value) == eventName))
        {
            return false;
        }
        generalDocuments.RecordChange(typeKey);
        JsonArray events = type["events"] as JsonArray ?? new JsonArray();
        events.Add(eventName);
        type["events"] = events;
        store.refreshModifiedState();
        return true;
    }

    public bool RenameGeneralEvent(string typeKey, string oldName, string newName)
    {
        string eventName = newName.Trim();
        if (!isValidGeneralEventName(eventName)
            || !generalDocuments.TryGetValue(typeKey, out JsonObject? type)
            || type["events"] is not JsonArray events
            || !events.Any(value => ProjectDataStore.getString(value) == oldName)
            || events.Any(value => ProjectDataStore.getString(value) == eventName))
        {
            return false;
        }
        foreach (JsonObject member in getGeneralMembers(type))
        {
            if (member["_graph"] is not JsonObject graph)
                continue;
            if (graph["nodeGraph"] is JsonObject nodeGraph
                && nodeGraph.ContainsKey(oldName) && nodeGraph.ContainsKey(eventName)
                || graph["startNodes"] is JsonObject startNodes
                && startNodes.ContainsKey(oldName) && startNodes.ContainsKey(eventName))
            {
                return false;
            }
        }
        generalDocuments.RecordChange(typeKey);
        for (int index = 0; index < events.Count; index++)
        {
            if (ProjectDataStore.getString(events[index]) == oldName)
                events[index] = eventName;
        }
        foreach (JsonObject member in getGeneralMembers(type))
        {
            if (member["_graph"] is not JsonObject graph)
                continue;
            if (graph["nodeGraph"] is JsonObject nodeGraph && nodeGraph.ContainsKey(oldName))
                replaceGeneralObjectKey(nodeGraph, oldName, eventName, nodeGraph[oldName]);
            if (graph["startNodes"] is JsonObject startNodes && startNodes.ContainsKey(oldName))
                replaceGeneralObjectKey(startNodes, oldName, eventName, startNodes[oldName]);
        }
        store.refreshModifiedState();
        return true;
    }

    public bool DeleteGeneralEvent(string typeKey, string name)
    {
        if (!generalDocuments.TryGetValue(typeKey, out JsonObject? type)
            || type["events"] is not JsonArray events || !events.Any(value => ProjectDataStore.getString(value) == name))
        {
            return false;
        }
        generalDocuments.RecordChange(typeKey);
        for (int index = events.Count - 1; index >= 0; index--)
        {
            if (ProjectDataStore.getString(events[index]) == name)
                events.RemoveAt(index);
        }
        if (events.Count == 0)
            type.Remove("events");
        foreach (JsonObject member in getGeneralMembers(type))
        {
            if (member["_graph"] is not JsonObject graph)
                continue;
            (graph["nodeGraph"] as JsonObject)?.Remove(name);
            (graph["startNodes"] as JsonObject)?.Remove(name);
        }
        store.refreshModifiedState();
        return true;
    }

    internal static bool isValidGeneralEventName(string name)
    {
        return name.Length != 0 && !char.IsDigit(name[0]);
    }

    internal static IEnumerable<JsonObject> getGeneralMembers(JsonObject type)
    {
        return type["members"] is JsonObject members
            ? members.Select(entry => entry.Value).OfType<JsonObject>()
            : [];
    }

    internal static void replaceGeneralObjectKey(JsonObject data, string oldName, string newName, JsonNode? value)
    {
        List<KeyValuePair<string, JsonNode?>> entries = data.ToList();
        data.Clear();
        foreach (KeyValuePair<string, JsonNode?> entry in entries)
            data.Add(entry.Key == oldName ? newName : entry.Key, entry.Key == oldName ? value : entry.Value);
    }

    internal static JsonNode? createGeneralMemberDefaultValue(JsonObject definition)
    {
        JsonNode? typeNode = definition["type"];
        string type = typeNode is JsonValue scalar && scalar.TryGetValue(out string? text)
            ? text ?? "string"
            : typeNode is null ? "string" : LuaMetadataType.Parse(typeNode).ToString();
        JsonNode? defaultValue = definition["defaultValue"];
        LuaMetadataType schema = LuaMetadataType.Parse(type);
        if (schema.Kind == LuaMetadataTypeKind.Union || type.StartsWith("Tuple[", StringComparison.Ordinal)
            || typeNode is JsonObject)
        {
            return defaultValue?.DeepClone() ?? LuaMetadataValueDefaults.Create(schema, _ => null);
        }
        return type switch
        {
            "int" => defaultValue?.GetValue<long?>() ?? 0,
            "float" => defaultValue?.GetValue<double?>() ?? 0.0,
            "bool" => defaultValue?.GetValue<bool?>() ?? false,
            "list" => defaultValue is JsonArray array ? array.DeepClone() : new JsonArray(),
            "dict" => defaultValue is JsonObject dictionary ? dictionary.DeepClone() : new JsonObject(),
            "file" => JsonValue.Create(string.Empty),
            _ when type.StartsWith("sf.", StringComparison.Ordinal) => defaultValue?.DeepClone()
                ?? LuaMetadataValueDefaults.Create(schema, _ => JsonValue.Create(string.Empty))
                ?? JsonValue.Create(string.Empty),
            _ => JsonValue.Create(defaultValue?.GetValue<string>() ?? string.Empty),
        };
    }

}
