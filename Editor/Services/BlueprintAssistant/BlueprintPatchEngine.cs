using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Ludork.Services.BlueprintAssistant;

internal static class BlueprintPatchEngine
{
    public static bool TryApply(
        JsonObject candidate,
        JsonElement patch,
        out JsonObject result,
        out string error)
    {
        JsonElement operations = patch;
        if (patch.ValueKind == JsonValueKind.Object
            && patch.TryGetProperty("ops", out JsonElement wrappedOperations))
        {
            operations = wrappedOperations;
        }
        if (operations.ValueKind != JsonValueKind.Array
            || operations.GetArrayLength() == 0)
        {
            result = candidate;
            error = "Patch operations must be a non-empty array.";
            return false;
        }
        int index = 0;
        foreach (JsonElement operation in operations.EnumerateArray())
        {
            if (operation.ValueKind != JsonValueKind.Object
                || !operation.TryGetProperty("op", out JsonElement operationNameValue)
                || operationNameValue.ValueKind != JsonValueKind.String)
            {
                result = candidate;
                error = $"ops[{index}] must declare a string op.";
                return false;
            }
            string operationName = operationNameValue.GetString() ?? string.Empty;
            if (!applyPatchOperation(candidate, operationName, operation, index, out error))
            {
                result = candidate;
                return false;
            }
            index++;
        }
        result = candidate;
        error = string.Empty;
        return true;
    }

    private static bool applyPatchOperation(
        JsonObject candidate,
        string operationName,
        JsonElement operation,
        int operationIndex,
        out string error)
    {
        switch (operationName)
        {
            case "updateLink":
                if (!validateOperationFields(
                        operation,
                        operationIndex,
                        ["op", "event", "linkIndex", "left", "right", "leftOutPin", "rightInPin", "linkType"],
                        ["op", "event", "linkIndex"],
                        out error))
                {
                    return false;
                }
                if (!validateLinkUpdate(operation, operationIndex, out error))
                    return false;
                return updateGraphItem(
                    candidate,
                    operation,
                    operationIndex,
                    "links",
                    "linkIndex",
                    ["left", "right", "leftOutPin", "rightInPin", "linkType"],
                    out error);
            case "updateNode":
                if (!validateOperationFields(
                        operation,
                        operationIndex,
                        ["op", "event", "nodeIndex", "nodeFunction", "params", "pos"],
                        ["op", "event", "nodeIndex"],
                        out error))
                {
                    return false;
                }
                if (!validateNodeUpdate(operation, operationIndex, out error))
                    return false;
                return updateGraphItem(
                    candidate,
                    operation,
                    operationIndex,
                    "nodes",
                    "nodeIndex",
                    ["nodeFunction", "params", "pos"],
                    out error);
            case "setStartNode":
                if (!validateOperationFields(
                        operation,
                        operationIndex,
                        ["op", "event", "index"],
                        ["op", "event", "index"],
                        out error))
                {
                    return false;
                }
                return setStartNode(candidate, operation, operationIndex, out error);
            case "replaceEventGraph":
                if (!validateOperationFields(
                        operation,
                        operationIndex,
                        ["op", "event", "nodes", "links"],
                        ["op", "event", "nodes", "links"],
                        out error))
                {
                    return false;
                }
                return replaceEventGraph(candidate, operation, operationIndex, out error);
            case "setAttrs":
                if (!validateOperationFields(
                        operation,
                        operationIndex,
                        ["op", "attrs"],
                        ["op", "attrs"],
                        out error))
                {
                    return false;
                }
                return setAttributes(candidate, operation, operationIndex, out error);
            default:
                error = $"ops[{operationIndex}] has unknown op \"{operationName}\".";
                return false;
        }
    }

    private static bool updateGraphItem(
        JsonObject candidate,
        JsonElement operation,
        int operationIndex,
        string collectionName,
        string indexName,
        IReadOnlyList<string> fields,
        out string error)
    {
        if (!tryGetEventGraph(
                candidate,
                operation,
                operationIndex,
                out JsonObject? eventGraph,
                out error))
        {
            return false;
        }
        if (!operation.TryGetProperty(indexName, out JsonElement indexValue)
            || !indexValue.TryGetInt32(out int itemIndex)
            || eventGraph![collectionName] is not JsonArray items
            || itemIndex < 0
            || itemIndex >= items.Count
            || items[itemIndex] is not JsonObject item)
        {
            error = $"ops[{operationIndex}] has an invalid {indexName}.";
            return false;
        }
        foreach (string field in fields)
        {
            if (operation.TryGetProperty(field, out JsonElement value))
                item[field] = JsonNode.Parse(value.GetRawText());
        }
        error = string.Empty;
        return true;
    }

    private static bool validateLinkUpdate(
        JsonElement operation,
        int operationIndex,
        out string error)
    {
        bool hasUpdate = false;
        foreach (string field in new[] { "left", "right", "leftOutPin", "rightInPin" })
        {
            if (!operation.TryGetProperty(field, out JsonElement value))
                continue;
            hasUpdate = true;
            bool valid = field == "left"
                ? value.ValueKind == JsonValueKind.String || value.TryGetInt32(out int _)
                : value.TryGetInt32(out int _);
            if (!valid)
            {
                error = $"ops[{operationIndex}].{field} has an invalid type.";
                return false;
            }
        }
        if (operation.TryGetProperty("linkType", out JsonElement linkType))
        {
            hasUpdate = true;
            if (linkType.ValueKind != JsonValueKind.String
                || linkType.GetString() is not "Exec" and not "Params")
            {
                error = $"ops[{operationIndex}].linkType must be \"Exec\" or \"Params\".";
                return false;
            }
        }
        error = hasUpdate
            ? string.Empty
            : $"ops[{operationIndex}] does not contain a link update.";
        return hasUpdate;
    }

    private static bool validateNodeUpdate(
        JsonElement operation,
        int operationIndex,
        out string error)
    {
        bool hasUpdate = false;
        if (operation.TryGetProperty("nodeFunction", out JsonElement nodeFunction))
        {
            hasUpdate = true;
            if (nodeFunction.ValueKind != JsonValueKind.String
                || string.IsNullOrWhiteSpace(nodeFunction.GetString()))
            {
                error = $"ops[{operationIndex}].nodeFunction must be a non-empty string.";
                return false;
            }
        }
        if (operation.TryGetProperty("params", out JsonElement parameters))
        {
            hasUpdate = true;
            if (parameters.ValueKind != JsonValueKind.Array)
            {
                error = $"ops[{operationIndex}].params must be an array.";
                return false;
            }
        }
        if (operation.TryGetProperty("pos", out JsonElement position))
        {
            hasUpdate = true;
            if (position.ValueKind != JsonValueKind.Array
                || position.GetArrayLength() != 2
                || position.EnumerateArray().Any(value =>
                    value.ValueKind != JsonValueKind.Number))
            {
                error = $"ops[{operationIndex}].pos must contain two numbers.";
                return false;
            }
        }
        error = hasUpdate
            ? string.Empty
            : $"ops[{operationIndex}] does not contain a node update.";
        return hasUpdate;
    }

    private static bool setStartNode(
        JsonObject candidate,
        JsonElement operation,
        int operationIndex,
        out string error)
    {
        if (!tryGetEventName(operation, operationIndex, out string eventName, out error)
            || !operation.TryGetProperty("index", out JsonElement indexValue)
            || indexValue.ValueKind is not JsonValueKind.Number and not JsonValueKind.Null)
        {
            if (error.Length == 0)
                error = $"ops[{operationIndex}] requires a numeric or null index.";
            return false;
        }
        if (candidate["graph"]?["nodeGraph"]?[eventName]?["nodes"] is not JsonArray nodes)
        {
            error = $"ops[{operationIndex}] references a missing event graph.";
            return false;
        }
        if (indexValue.ValueKind == JsonValueKind.Number
            && (!indexValue.TryGetInt32(out int index)
                || index < 0
                || index >= nodes.Count))
        {
            error = $"ops[{operationIndex}] start node index is out of range.";
            return false;
        }
        if (candidate["graph"] is not JsonObject graph)
        {
            error = $"ops[{operationIndex}] cannot apply because graph is missing.";
            return false;
        }
        JsonObject startNodes = graph["startNodes"] as JsonObject ?? [];
        graph["startNodes"] = startNodes;
        startNodes[eventName] = JsonNode.Parse(indexValue.GetRawText());
        error = string.Empty;
        return true;
    }

    private static bool validateOperationFields(
        JsonElement operation,
        int operationIndex,
        IReadOnlyList<string> allowed,
        IReadOnlyList<string> required,
        out string error)
    {
        foreach (JsonProperty property in operation.EnumerateObject())
        {
            if (!allowed.Contains(property.Name, StringComparer.Ordinal))
            {
                error = $"ops[{operationIndex}] contains unknown field \"{property.Name}\".";
                return false;
            }
        }
        foreach (string field in required)
        {
            if (!operation.TryGetProperty(field, out JsonElement _))
            {
                error = $"ops[{operationIndex}] is missing required field \"{field}\".";
                return false;
            }
        }
        error = string.Empty;
        return true;
    }

    private static bool replaceEventGraph(
        JsonObject candidate,
        JsonElement operation,
        int operationIndex,
        out string error)
    {
        if (!tryGetEventName(operation, operationIndex, out string eventName, out error)
            || !operation.TryGetProperty("nodes", out JsonElement nodes)
            || nodes.ValueKind != JsonValueKind.Array
            || !operation.TryGetProperty("links", out JsonElement links)
            || links.ValueKind != JsonValueKind.Array)
        {
            if (error.Length == 0)
                error = $"ops[{operationIndex}] requires nodes and links arrays.";
            return false;
        }
        if (candidate["graph"] is not JsonObject graph)
        {
            error = $"ops[{operationIndex}] cannot apply because graph is missing.";
            return false;
        }
        JsonObject nodeGraph = graph["nodeGraph"] as JsonObject ?? [];
        graph["nodeGraph"] = nodeGraph;
        nodeGraph[eventName] = new JsonObject
        {
            ["nodes"] = JsonNode.Parse(nodes.GetRawText()),
            ["links"] = JsonNode.Parse(links.GetRawText()),
        };
        error = string.Empty;
        return true;
    }

    private static bool setAttributes(
        JsonObject candidate,
        JsonElement operation,
        int operationIndex,
        out string error)
    {
        if (!operation.TryGetProperty("attrs", out JsonElement attributes)
            || attributes.ValueKind != JsonValueKind.Object
            || JsonNode.Parse(attributes.GetRawText()) is not JsonObject updates)
        {
            error = $"ops[{operationIndex}] requires an attrs object.";
            return false;
        }
        JsonObject target = candidate["attrs"] as JsonObject ?? [];
        candidate["attrs"] = target;
        foreach (KeyValuePair<string, JsonNode?> pair in updates)
            target[pair.Key] = pair.Value?.DeepClone();
        error = string.Empty;
        return true;
    }

    private static bool tryGetEventGraph(
        JsonObject candidate,
        JsonElement operation,
        int operationIndex,
        out JsonObject? eventGraph,
        out string error)
    {
        if (!tryGetEventName(operation, operationIndex, out string eventName, out error))
        {
            eventGraph = null;
            return false;
        }
        eventGraph = candidate["graph"]?["nodeGraph"]?[eventName] as JsonObject;
        if (eventGraph is null)
        {
            error = $"ops[{operationIndex}] references a missing event graph.";
            return false;
        }
        error = string.Empty;
        return true;
    }

    private static bool tryGetEventName(
        JsonElement operation,
        int operationIndex,
        out string eventName,
        out string error)
    {
        if (!operation.TryGetProperty("event", out JsonElement eventValue)
            || eventValue.ValueKind != JsonValueKind.String
            || string.IsNullOrWhiteSpace(eventValue.GetString()))
        {
            eventName = string.Empty;
            error = $"ops[{operationIndex}] requires a non-empty event.";
            return false;
        }
        eventName = eventValue.GetString()!;
        error = string.Empty;
        return true;
    }
}
