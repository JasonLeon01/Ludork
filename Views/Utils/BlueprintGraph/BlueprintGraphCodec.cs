using Ludork.Models;
using Ludork.Views.Utils;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils.BlueprintGraph;

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
                getNodeTitle(nodeFunction, definition),
                getNumber(position.ElementAtOrDefault(0)),
                getNumber(position.ElementAtOrDefault(1)),
                definition is not null,
                false,
                null,
                rawNode,
                parameters,
                definition?.Description);
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
                continue;
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
                continue;
            }
            BlueprintGraphPortKind kind = string.Equals(
                getString(rawLink["linkType"]),
                "Exec",
                StringComparison.OrdinalIgnoreCase)
                ? BlueprintGraphPortKind.Exec
                : BlueprintGraphPortKind.Params;
            int sourcePinIndex = getInteger(rawLink["leftOutPin"]);
            int targetPinIndex = getInteger(rawLink["rightInPin"]);
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
        document.Start = start;
        return document;
    }

    public static BlueprintGraphSaveResult Save(BlueprintGraphDocument document)
    {
        JsonObject eventGraph = (JsonObject)document.RawEventGraph.DeepClone();
        JsonArray nodes = [];
        Dictionary<Guid, int> nodeIndices = [];
        foreach (BlueprintGraphNode node in document.Nodes.Where(node => !node.IsVirtual))
        {
            nodeIndices[node.Id] = nodes.Count;
            JsonObject rawNode = (JsonObject)node.RawData.DeepClone();
            rawNode["nodeFunction"] = node.NodeFunction;
            JsonArray parameters = (JsonArray)node.Parameters.DeepClone();
            foreach (BlueprintGraphPort port in node.Inputs)
            {
                if (port.Kind != BlueprintGraphPortKind.Params || port.ParameterIndex is not int parameterIndex)
                    continue;
                if (node.OriginalIndex is not null
                    && parameterIndex >= parameters.Count
                    && !port.IsValueModified)
                {
                    continue;
                }
                while (parameters.Count <= parameterIndex)
                    parameters.Add(null);
                parameters[parameterIndex] = port.Value?.DeepClone();
            }
            rawNode["params"] = parameters;
            rawNode["pos"] = new JsonArray(node.X, node.Y);
            nodes.Add(rawNode);
        }

        JsonArray links = [];
        foreach (BlueprintGraphConnection connection in document.Connections)
        {
            JsonObject rawLink = (JsonObject)connection.RawData.DeepClone();
            rawLink["left"] = serializeEndpoint(connection.Source, nodeIndices);
            rawLink["right"] = serializeEndpoint(connection.Target, nodeIndices);
            rawLink["leftOutPin"] = connection.SourcePinIndex;
            rawLink["rightInPin"] = connection.TargetPinIndex;
            rawLink["linkType"] = connection.Kind == BlueprintGraphPortKind.Exec ? "Exec" : "Params";
            links.Add(rawLink);
        }
        eventGraph["nodes"] = nodes;
        eventGraph["links"] = links;
        JsonNode? serializedStart = document.Start is null
            ? null
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

    private static string getNodeTitle(
        string nodeFunction,
        BlueprintGraphNodeDefinition? definition)
    {
        if (definition?.HasExplicitDisplayName == true)
            return definition.Title;
        int separator = nodeFunction.LastIndexOf('.');
        string memberName = separator >= 0 && separator < nodeFunction.Length - 1
            ? nodeFunction[(separator + 1)..]
            : nodeFunction;
        string displayName = EditorDisplayName.Format(memberName);
        if (!nodeFunction.Contains('.', StringComparison.Ordinal)
            && !string.IsNullOrWhiteSpace(memberName))
        {
            return $"(parent){displayName}";
        }
        return displayName;
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    private static int getInteger(JsonNode? value)
    {
        return tryGetInteger(value, out int result) ? result : 0;
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
