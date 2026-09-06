using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintGraphSaveResult
{
    public BlueprintGraphSaveResult(JsonObject eventGraph, JsonNode? startNode)
    {
        EventGraph = eventGraph;
        StartNode = startNode?.DeepClone();
    }

    public JsonObject EventGraph { get; }
    public JsonNode? StartNode { get; }
}
