using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal static class GeneralDataSchemaValidation
{
    public static IReadOnlyList<string> Validate(IReadOnlyDictionary<string, JsonObject> generalData)
    {
        List<string> errors = [];
        foreach (KeyValuePair<string, JsonObject> entry in generalData.OrderBy(pair => pair.Key, StringComparer.Ordinal))
            validateType(entry.Key, entry.Value, errors);
        return errors;
    }

    private static void validateType(string typeName, JsonObject typeData, ICollection<string> errors)
    {
        string path = "General/" + typeName;
        if (typeData.ContainsKey("linkedType"))
            errors.Add(path + ": linkedType is not supported");
        JsonObject? parameters = typeData["params"] as JsonObject;
        if (parameters is null)
            errors.Add(path + ": params must be an object");
        if (typeData["members"] is not JsonObject members)
        {
            errors.Add(path + ": members must be an object");
            return;
        }
        if (parameters is not null)
            validateAssetParameterDefinitions(path, parameters, errors);

        HashSet<string> eventNames = new(StringComparer.Ordinal);
        if (typeData.TryGetPropertyValue("events", out JsonNode? rawEvents))
        {
            if (rawEvents is not JsonArray events)
            {
                errors.Add(path + ": events must be an array");
                return;
            }
            if (events.Count == 0)
                errors.Add(path + ": events must be a non-empty array when present");
            for (int index = 0; index < events.Count; index++)
            {
                string? eventName = getString(events[index]);
                if (string.IsNullOrWhiteSpace(eventName))
                {
                    errors.Add(path + $": events[{index}] must be a non-empty string");
                    continue;
                }
                if (!eventNames.Add(eventName))
                    errors.Add(path + $": event '{eventName}' is duplicated");
            }
        }

        foreach (KeyValuePair<string, JsonNode?> memberEntry in members)
        {
            if (memberEntry.Value is not JsonObject member)
            {
                errors.Add(path + "/" + memberEntry.Key + ": member must be an object");
                continue;
            }
            if (parameters is not null)
                validateAssetPaths(path + "/" + memberEntry.Key, parameters, member, errors);
            validateMemberGraph(path + "/" + memberEntry.Key, member["_graph"], eventNames, errors);
        }
    }

    private static void validateAssetParameterDefinitions(
        string path,
        JsonObject parameters,
        ICollection<string> errors)
    {
        foreach (KeyValuePair<string, JsonNode?> entry in parameters)
        {
            if (entry.Value is not JsonObject definition
                || getString(definition["type"]) != "file")
            {
                continue;
            }
            if (getString(definition["defaultValue"]) is not "")
                errors.Add(path + $": file parameter '{entry.Key}' defaultValue must be empty");
            string? baseHint = getString(definition["base"]);
            if (!GameAssetPath.IsValidBaseHint(baseHint))
                errors.Add(path + $": file parameter '{entry.Key}' base is invalid");
        }
    }

    private static void validateAssetPaths(
        string path,
        JsonObject parameters,
        JsonObject member,
        ICollection<string> errors)
    {
        foreach (KeyValuePair<string, JsonNode?> entry in parameters)
        {
            if (entry.Value is not JsonObject definition)
                continue;
            string? type = getString(definition["type"]);
            if (type == "file")
            {
                validateAssetPath(path + "." + entry.Key, member[entry.Key], errors);
                continue;
            }
            if (type == "list"
                && getString(definition["itemType"]) == "file"
                && member[entry.Key] is JsonArray items)
            {
                for (int index = 0; index < items.Count; index++)
                    validateAssetPath(path + $".{entry.Key}[{index}]", items[index], errors);
                continue;
            }
            if (type == "dict"
                && getString(definition["valueType"]) == "file"
                && member[entry.Key] is JsonObject values)
            {
                foreach (KeyValuePair<string, JsonNode?> item in values)
                    validateAssetPath(path + $".{entry.Key}.{item.Key}", item.Value, errors);
            }
        }
    }

    private static void validateAssetPath(
        string path,
        JsonNode? value,
        ICollection<string> errors)
    {
        string? text = getString(value);
        if (!string.IsNullOrEmpty(text) && !GameAssetPath.IsCanonical(text))
            errors.Add(path + ": file value must use a canonical /Game/Assets/ path");
    }

    private static void validateMemberGraph(
        string path,
        JsonNode? rawGraph,
        IReadOnlySet<string> eventNames,
        ICollection<string> errors)
    {
        if (rawGraph is null)
            return;
        if (rawGraph is not JsonObject graph)
        {
            errors.Add(path + ": _graph must be an object");
            return;
        }
        if (graph["nodeGraph"] is not JsonObject nodeGraph)
        {
            errors.Add(path + ": _graph.nodeGraph must be an object");
            return;
        }
        if (graph["startNodes"] is not JsonObject startNodes)
        {
            errors.Add(path + ": _graph.startNodes must be an object");
            return;
        }

        foreach (KeyValuePair<string, JsonNode?> graphEntry in nodeGraph)
        {
            if (!eventNames.Contains(graphEntry.Key))
                errors.Add(path + $": graph event '{graphEntry.Key}' is not declared in events");
            if (graphEntry.Value is not JsonObject eventGraph)
            {
                errors.Add(path + $": _graph.nodeGraph['{graphEntry.Key}'] must be an object");
                continue;
            }
            if (eventGraph["nodes"] is not JsonArray)
                errors.Add(path + $": _graph.nodeGraph['{graphEntry.Key}'].nodes must be an array");
            if (eventGraph["links"] is not JsonArray)
                errors.Add(path + $": _graph.nodeGraph['{graphEntry.Key}'].links must be an array");
            if (!startNodes.ContainsKey(graphEntry.Key))
                errors.Add(path + $": _graph.startNodes is missing '{graphEntry.Key}'");
        }
        foreach (KeyValuePair<string, JsonNode?> startEntry in startNodes)
        {
            if (!eventNames.Contains(startEntry.Key))
                errors.Add(path + $": start event '{startEntry.Key}' is not declared in events");
            if (!nodeGraph.ContainsKey(startEntry.Key))
                errors.Add(path + $": _graph.nodeGraph is missing '{startEntry.Key}'");
            if (startEntry.Value is not null && !tryGetInteger(startEntry.Value, out int _))
                errors.Add(path + $": _graph.startNodes['{startEntry.Key}'] must be an integer or null");
        }
    }

    private static bool tryGetInteger(JsonNode? value, out int result)
    {
        result = 0;
        if (value is not JsonValue scalar)
            return false;
        if (scalar.TryGetValue(out result))
            return true;
        if (!scalar.TryGetValue(out long longValue)
            || longValue < int.MinValue
            || longValue > int.MaxValue)
        {
            return false;
        }
        result = (int)longValue;
        return true;
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }
}
