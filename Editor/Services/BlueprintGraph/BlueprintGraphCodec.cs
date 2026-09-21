using Ludork.Models;
using Ludork.Services;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public static class BlueprintGraphCodec
{
    private static readonly IReadOnlyDictionary<string, BlueprintGraphNodeDefinition> EmptyDefinitionLookup =
        new Dictionary<string, BlueprintGraphNodeDefinition>(StringComparer.Ordinal);

    public static BlueprintGraphDocument Load(
        string eventName,
        JsonObject eventGraph,
        JsonNode? startNode = null,
        BlueprintNodeDefinitionSet? definitionSet = null,
        IReadOnlyList<BlueprintGraphEventParameterDefinition>? eventParameters = null)
    {
        IReadOnlyDictionary<string, BlueprintGraphNodeDefinition> definitionsByPath =
            definitionSet?.RuntimeLookup ?? EmptyDefinitionLookup;
        Dictionary<string, BlueprintGraphEventParameterDefinition> parametersByKey = (eventParameters ?? [])
            .GroupBy(parameter => parameter.ExternalKey, StringComparer.Ordinal)
            .ToDictionary(group => group.Key, group => group.Last(), StringComparer.Ordinal);
        BlueprintGraphDocument document = new(eventName, eventGraph);
        Guid graphId = createStableId(eventName);
        Dictionary<int, BlueprintGraphNode> nodesByIndex = [];
        Dictionary<Guid, BlueprintGraphNode> nodesById = [];
        Dictionary<string, BlueprintGraphNode> externalNodes = new(StringComparer.Ordinal);
        JsonArray nodes = eventGraph["nodes"] as JsonArray ?? [];
        for (int index = 0; index < nodes.Count; index++)
        {
            JsonObject rawNode = nodes[index] as JsonObject ?? [];
            string nodeFunction = getString(rawNode["nodeFunction"]) ?? string.Empty;
            definitionsByPath.TryGetValue(nodeFunction, out BlueprintGraphNodeDefinition? definition);
            JsonArray position = rawNode["pos"] as JsonArray ?? [];
            JsonArray parameters = rawNode["params"] as JsonArray ?? [];
            Guid nodeId = createStableId($"{eventName}:node:{index}:{nodeFunction}");
            BlueprintGraphNode node = new(
                nodeId,
                index,
                nodeFunction,
                BlueprintNodeDisplayText.GetGraphTitle(nodeFunction, definition),
                getNumber(position.ElementAtOrDefault(0)),
                getNumber(position.ElementAtOrDefault(1)),
                definition is not null,
                false,
                null,
                rawNode,
                parameters);
            if (definition is not null)
                addDefinitionPorts(node, definition, parameters);
            addParameterPorts(node, parameters.Count);
            document.Nodes.Add(node);
            nodesByIndex[index] = node;
            nodesById[node.Id] = node;
        }

        foreach (BlueprintGraphEventParameterDefinition parameter in (eventParameters ?? [])
            .OrderBy(parameter => parameter.Index))
        {
            getEndpoint(
                JsonValue.Create(parameter.ExternalKey),
                eventName,
                nodesByIndex,
                externalNodes,
                parametersByKey,
                nodesById,
                document);
        }

        JsonArray links = eventGraph["links"] as JsonArray ?? [];
        for (int index = 0; index < links.Count; index++)
        {
            if (links[index] is not JsonObject rawLink)
            {
                document.UnresolvedConnections.Add(new BlueprintGraphUnresolvedConnection(index, links[index], null, null));
                continue;
            }
            Guid? sourceNodeIdReference = getNodeReference(rawLink["left"], nodesByIndex);
            Guid? targetNodeIdReference = getNodeReference(rawLink["right"], nodesByIndex);
            string? kindName = getString(rawLink["linkType"]);
            if (kindName is not "Exec" and not "Params"
                || !tryGetInteger(rawLink["leftOutPin"], out int sourcePinIndex) || sourcePinIndex < 0
                || !tryGetInteger(rawLink["rightInPin"], out int targetPinIndex) || targetPinIndex < 0
                || !tryGetInteger(rawLink["right"], out int _))
            {
                document.UnresolvedConnections.Add(new BlueprintGraphUnresolvedConnection(index, rawLink, sourceNodeIdReference, targetNodeIdReference));
                continue;
            }
            BlueprintGraphEndpoint? source = getEndpoint(
                rawLink["left"],
                eventName,
                nodesByIndex,
                externalNodes,
                parametersByKey,
                nodesById,
                document);
            BlueprintGraphEndpoint? target = getEndpoint(
                rawLink["right"],
                eventName,
                nodesByIndex,
                externalNodes,
                parametersByKey,
                nodesById,
                document);
            if (source?.NodeId is not Guid sourceNodeId
                || target?.NodeId is not Guid targetNodeId
                || !nodesById.TryGetValue(sourceNodeId, out BlueprintGraphNode? sourceNode)
                || !nodesById.TryGetValue(targetNodeId, out BlueprintGraphNode? targetNode))
            {
                document.UnresolvedConnections.Add(new BlueprintGraphUnresolvedConnection(index, rawLink, sourceNodeIdReference, targetNodeIdReference));
                continue;
            }
            BlueprintGraphPortKind kind = kindName == "Exec"
                ? BlueprintGraphPortKind.Exec
                : BlueprintGraphPortKind.Params;
            BlueprintGraphPort sourcePort = getOrAddPort(
                sourceNode,
                BlueprintGraphPortDirection.Output,
                kind,
                sourcePinIndex);
            BlueprintGraphPort targetPort = getOrAddPort(
                targetNode,
                BlueprintGraphPortDirection.Input,
                kind,
                targetPinIndex);
            Guid connectionId = createIndexedId(graphId, 1, index);
            BlueprintGraphConnection connection = new(
                connectionId,
                index,
                source,
                target,
                sourcePort.Id,
                targetPort.Id,
                kind,
                sourcePinIndex,
                targetPinIndex,
                rawLink);
            document.AddLoadedConnection(connection);
        }

        BlueprintGraphEndpoint? start = getEndpoint(
            startNode,
            eventName,
            nodesByIndex,
            externalNodes,
            parametersByKey,
            nodesById,
            document);
        document.SetLoadedStart(start, startNode);
        return document;
    }

    public static BlueprintGraphSaveResult Save(BlueprintGraphDocument document)
    {
        JsonObject eventGraph = (JsonObject)document.RawEventGraph.DeepClone();
        JsonArray? originalNodes = document.RawEventGraph["nodes"] as JsonArray;
        JsonArray nodes = [];
        Dictionary<Guid, int> nodeIndices = [];
        foreach (BlueprintGraphNode node in document.Nodes.Where(node => !node.IsVirtual))
        {
            nodeIndices[node.Id] = nodes.Count;
            if (node.OriginalIndex is int sourceIndex && originalNodes is not null
                && sourceIndex >= 0 && sourceIndex < originalNodes.Count && originalNodes[sourceIndex] is not JsonObject
                && string.IsNullOrEmpty(node.NodeFunction))
            {
                nodes.Add(originalNodes[sourceIndex]?.DeepClone());
                continue;
            }
            JsonObject rawNode = (JsonObject)node.RawData.DeepClone();
            if (node.OriginalIndex is null
                || !string.Equals(getString(rawNode["nodeFunction"]) ?? string.Empty, node.NodeFunction, StringComparison.Ordinal))
            {
                rawNode["nodeFunction"] = node.NodeFunction;
            }
            JsonArray parameters = (JsonArray)node.Parameters.DeepClone();
            bool parametersChanged = node.OriginalIndex is null;
            foreach (BlueprintGraphPort port in node.Inputs)
            {
                if (port.Kind != BlueprintGraphPortKind.Params || port.ParameterIndex is not int parameterIndex)
                    continue;
                if (node.OriginalIndex is not null && !port.IsValueModified)
                {
                    continue;
                }
                while (parameters.Count <= parameterIndex)
                    parameters.Add(null);
                parameters[parameterIndex] = port.Value?.DeepClone();
                parametersChanged = true;
            }
            if (parametersChanged)
                rawNode["params"] = parameters;
            JsonArray? position = rawNode["pos"] as JsonArray;
            if (node.OriginalIndex is null
                || !node.X.Equals(getNumber(position?.ElementAtOrDefault(0)))
                || !node.Y.Equals(getNumber(position?.ElementAtOrDefault(1))))
            {
                rawNode["pos"] = new JsonArray(node.X, node.Y);
            }
            nodes.Add(rawNode);
        }

        SortedDictionary<int, JsonNode?> originalLinks = [];
        List<JsonNode?> newLinks = [];
        foreach (BlueprintGraphConnection connection in document.Connections)
        {
            JsonObject rawLink = (JsonObject)connection.RawData.DeepClone();
            rawLink["left"] = serializeEndpoint(connection.Source, nodeIndices);
            rawLink["right"] = serializeEndpoint(connection.Target, nodeIndices);
            rawLink["leftOutPin"] = connection.SourcePinIndex;
            rawLink["rightInPin"] = connection.TargetPinIndex;
            rawLink["linkType"] = connection.Kind == BlueprintGraphPortKind.Exec ? "Exec" : "Params";
            if (connection.OriginalIndex is int originalIndex)
                originalLinks[originalIndex] = rawLink;
            else
                newLinks.Add(rawLink);
        }
        foreach (BlueprintGraphUnresolvedConnection connection in document.UnresolvedConnections)
        {
            if (connection.SourceNodeId is Guid sourceNodeId && !nodeIndices.ContainsKey(sourceNodeId)
                || connection.TargetNodeId is Guid targetNodeId && !nodeIndices.ContainsKey(targetNodeId))
            {
                continue;
            }
            JsonNode? rawLink = connection.RawData?.DeepClone();
            if (rawLink is JsonObject link)
            {
                if (connection.SourceNodeId is Guid source)
                    link["left"] = nodeIndices[source];
                if (connection.TargetNodeId is Guid target)
                    link["right"] = nodeIndices[target];
            }
            originalLinks[connection.OriginalIndex] = rawLink;
        }
        JsonArray links = new(originalLinks.Values.Concat(newLinks).ToArray());
        if (nodes.Count != 0 || eventGraph["nodes"] is JsonArray)
            eventGraph["nodes"] = nodes;
        if (links.Count != 0 || eventGraph["links"] is JsonArray)
            eventGraph["links"] = links;
        JsonNode? serializedStart = document.Start is null
            ? document.UnresolvedStartNode?.DeepClone()
            : serializeEndpoint(document.Start, nodeIndices);
        return new BlueprintGraphSaveResult(eventGraph, serializedStart);
    }

    public static void SaveInto(
        BlueprintGraphDocument document,
        JsonObject targetEventGraph,
        JsonObject startNodes)
    {
        BlueprintGraphSaveResult result = Save(document);
        targetEventGraph.Clear();
        foreach (KeyValuePair<string, JsonNode?> entry in result.EventGraph)
            targetEventGraph[entry.Key] = entry.Value?.DeepClone();
        if (result.StartNode is not null || startNodes.ContainsKey(document.EventName))
            startNodes[document.EventName] = result.StartNode?.DeepClone();
    }

    private static void addDefinitionPorts(
        BlueprintGraphNode node,
        BlueprintGraphNodeDefinition definition,
        JsonArray parameters)
    {
        foreach (BlueprintGraphPortDefinition portDefinition in definition.Ports)
        {
            JsonNode? value = portDefinition.ParameterIndex is int parameterIndex
                && parameterIndex >= 0
                && parameterIndex < parameters.Count
                ? parameters[parameterIndex]
                : portDefinition.DefaultValue;
            node.AddPort(new BlueprintGraphPort(
                createPortId(node.Id, portDefinition.Direction, portDefinition.Kind, portDefinition.PinIndex),
                node.Id,
                portDefinition.Name,
                portDefinition.Kind,
                portDefinition.Direction,
                portDefinition.PinIndex,
                portDefinition.TypeName,
                portDefinition.ParameterIndex,
                portDefinition.SupportsEditor,
                value,
                portDefinition.Meta));
        }
    }

    private static void addParameterPorts(BlueprintGraphNode node, int parameterCount)
    {
        for (int index = 0; index < parameterCount; index++)
        {
            if (node.FindPort(
                BlueprintGraphPortDirection.Input,
                BlueprintGraphPortKind.Params,
                index) is not null)
            {
                continue;
            }
            JsonNode? value = index < node.Parameters.Count ? node.Parameters[index] : null;
            node.AddPort(new BlueprintGraphPort(
                createPortId(
                    node.Id,
                    BlueprintGraphPortDirection.Input,
                    BlueprintGraphPortKind.Params,
                    index),
                node.Id,
                $"Param {index + 1}",
                BlueprintGraphPortKind.Params,
                BlueprintGraphPortDirection.Input,
                index,
                "any",
                index,
                true,
                value));
        }
    }

    private static BlueprintGraphPort getOrAddPort(
        BlueprintGraphNode node,
        BlueprintGraphPortDirection direction,
        BlueprintGraphPortKind kind,
        int pinIndex)
    {
        BlueprintGraphPort? existing = node.FindPort(direction, kind, pinIndex);
        if (existing is not null)
            return existing;
        int? parameterIndex = direction == BlueprintGraphPortDirection.Input
            && kind == BlueprintGraphPortKind.Params
            ? pinIndex
            : null;
        JsonNode? value = parameterIndex is int index && index < node.Parameters.Count
            ? node.Parameters[index]
            : null;
        BlueprintGraphPort port = new(
            createPortId(node.Id, direction, kind, pinIndex),
            node.Id,
            createPortName(direction, kind, pinIndex),
            kind,
            direction,
            pinIndex,
            "any",
            parameterIndex,
            parameterIndex is not null,
            value);
        node.AddPort(port);
        return port;
    }

    private static BlueprintGraphEndpoint? getEndpoint(
        JsonNode? value,
        string eventName,
        IReadOnlyDictionary<int, BlueprintGraphNode> nodesByIndex,
        IDictionary<string, BlueprintGraphNode> externalNodes,
        IReadOnlyDictionary<string, BlueprintGraphEventParameterDefinition> parametersByKey,
        IDictionary<Guid, BlueprintGraphNode> nodesById,
        BlueprintGraphDocument document)
    {
        if (tryGetInteger(value, out int nodeIndex))
        {
            return nodesByIndex.TryGetValue(nodeIndex, out BlueprintGraphNode? node)
                ? BlueprintGraphEndpoint.Node(node.Id)
                : null;
        }
        string? externalKey = getString(value);
        if (externalKey is null)
            return null;
        if (!externalNodes.TryGetValue(externalKey, out BlueprintGraphNode? externalNode))
        {
            parametersByKey.TryGetValue(
                externalKey,
                out BlueprintGraphEventParameterDefinition? parameterDefinition);
            int externalIndex = parameterDefinition?.Index ?? externalNodes.Count;
            Guid nodeId = createStableId($"{eventName}:external:{externalKey}");
            string parameterName = parameterDefinition?.Name ?? externalKey;
            string parameterType = parameterDefinition?.TypeName ?? "any";
            string displayTitle = $"{EditorDisplayName.Format(parameterName)} ({parameterType})";
            externalNode = new BlueprintGraphNode(
                nodeId,
                null,
                externalKey,
                displayTitle,
                0,
                externalIndex * 64,
                parameterDefinition is not null,
                true,
                externalKey,
                [],
                [],
                $"Event parameter: {displayTitle}");
            BlueprintGraphPort output = new(
                createPortId(
                    nodeId,
                    BlueprintGraphPortDirection.Output,
                    BlueprintGraphPortKind.Params,
                    0),
                nodeId,
                parameterName,
                BlueprintGraphPortKind.Params,
                BlueprintGraphPortDirection.Output,
                0,
                parameterType,
                null,
                false,
                null);
            externalNode.AddPort(output);
            externalNodes[externalKey] = externalNode;
            nodesById[externalNode.Id] = externalNode;
            document.Nodes.Add(externalNode);
        }
        return BlueprintGraphEndpoint.External(externalKey, externalNode.Id);
    }

    private static Guid? getNodeReference(JsonNode? value, IReadOnlyDictionary<int, BlueprintGraphNode> nodesByIndex)
    {
        return tryGetInteger(value, out int index) && nodesByIndex.TryGetValue(index, out BlueprintGraphNode? node)
            ? node.Id
            : null;
    }

    private static JsonNode serializeEndpoint(
        BlueprintGraphEndpoint endpoint,
        IReadOnlyDictionary<Guid, int> nodeIndices)
    {
        if (endpoint.ExternalKey is string externalKey)
            return JsonValue.Create(externalKey);
        if (endpoint.NodeId is not Guid nodeId || !nodeIndices.TryGetValue(nodeId, out int nodeIndex))
            throw new InvalidOperationException("The blueprint graph contains an endpoint without a node.");
        return JsonValue.Create(nodeIndex);
    }

    private static Guid createPortId(
        Guid nodeId,
        BlueprintGraphPortDirection direction,
        BlueprintGraphPortKind kind,
        int pinIndex)
    {
        Span<byte> bytes = stackalloc byte[16];
        nodeId.TryWriteBytes(bytes);
        bytes[10] ^= (byte)((int)direction + 1);
        bytes[11] ^= (byte)((int)kind + 1);
        BinaryPrimitives.WriteInt32LittleEndian(bytes[12..], pinIndex + 1);
        return new Guid(bytes);
    }

    private static Guid createIndexedId(Guid namespaceId, byte kind, int index)
    {
        Span<byte> bytes = stackalloc byte[16];
        namespaceId.TryWriteBytes(bytes);
        bytes[11] ^= kind;
        BinaryPrimitives.WriteInt32LittleEndian(bytes[12..], index + 1);
        return new Guid(bytes);
    }

    private static Guid createStableId(string value)
    {
        int byteCount = Encoding.UTF8.GetByteCount(value);
        Span<byte> input = byteCount <= 512 ? stackalloc byte[byteCount] : new byte[byteCount];
        Encoding.UTF8.GetBytes(value, input);
        Span<byte> hash = stackalloc byte[32];
        SHA256.HashData(input, hash);
        hash[6] = (byte)((hash[6] & 0x0f) | 0x50);
        hash[8] = (byte)((hash[8] & 0x3f) | 0x80);
        return new Guid(hash[..16]);
    }

    private static string createPortName(
        BlueprintGraphPortDirection direction,
        BlueprintGraphPortKind kind,
        int pinIndex)
    {
        if (kind == BlueprintGraphPortKind.Exec)
            return direction == BlueprintGraphPortDirection.Input ? "In" : $"Out {pinIndex + 1}";
        return direction == BlueprintGraphPortDirection.Input
            ? $"Param {pinIndex + 1}"
            : $"Result {pinIndex + 1}";
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    private static bool tryGetInteger(JsonNode? value, out int result)
    {
        if (value is JsonValue scalar)
        {
            if (scalar.TryGetValue(out result))
                return true;
            if (scalar.TryGetValue(out long longValue)
                && longValue >= int.MinValue
                && longValue <= int.MaxValue)
            {
                result = (int)longValue;
                return true;
            }
        }
        result = 0;
        return false;
    }

    private static double getNumber(JsonNode? value)
    {
        if (value is JsonValue scalar)
        {
            if (scalar.TryGetValue(out double number))
                return number;
            if (scalar.TryGetValue(out decimal decimalValue))
                return decimal.ToDouble(decimalValue);
            if (scalar.TryGetValue(out long integer))
                return integer;
        }
        return 0;
    }
}
