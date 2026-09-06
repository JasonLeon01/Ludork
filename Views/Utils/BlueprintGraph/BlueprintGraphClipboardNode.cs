using Ludork.Models;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils.BlueprintGraph;

internal sealed class BlueprintGraphClipboardNode
{
    private BlueprintGraphClipboardNode(
        BlueprintGraphNodeDefinition definition,
        JsonObject rawData,
        JsonArray parameters,
        double x,
        double y,
        bool isResolved)
    {
        Definition = definition;
        RawData = rawData;
        Parameters = parameters;
        X = x;
        Y = y;
        IsResolved = isResolved;
    }

    public BlueprintGraphNodeDefinition Definition { get; }
    public JsonObject RawData { get; }
    public JsonArray Parameters { get; }
    public double X { get; }
    public double Y { get; }
    public bool IsResolved { get; }

    public static BlueprintGraphClipboardNode FromModel(BlueprintGraphNode node)
    {
        IReadOnlyList<BlueprintGraphPortDefinition> ports = node.Inputs
            .Concat(node.Outputs)
            .Select(port => new BlueprintGraphPortDefinition(
                port.Name,
                port.Kind,
                port.Direction,
                port.PinIndex,
                port.TypeName,
                port.ParameterIndex,
                port.SupportsEditor,
                port.Value,
                port.Meta))
            .ToArray();
        BlueprintGraphNodeDefinition definition = new(
            node.NodeFunction,
            node.Title,
            ports,
            description: node.Description);
        JsonArray parameters = node.Parameters.DeepClone() as JsonArray ?? [];
        foreach (BlueprintGraphPort port in node.Inputs)
        {
            if (port.Kind != BlueprintGraphPortKind.Params
                || port.ParameterIndex is not int parameterIndex)
            {
                continue;
            }
            while (parameters.Count <= parameterIndex)
                parameters.Add(null);
            parameters[parameterIndex] = port.Value?.DeepClone();
        }
        return new BlueprintGraphClipboardNode(
            definition,
            node.RawData.DeepClone() as JsonObject ?? [],
            parameters,
            node.X,
            node.Y,
            node.IsResolved);
    }
}
