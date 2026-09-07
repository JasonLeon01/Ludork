using System;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintGraphUnresolvedConnection
{
    public BlueprintGraphUnresolvedConnection(int originalIndex, JsonNode? rawData, Guid? sourceNodeId, Guid? targetNodeId)
    {
        OriginalIndex = originalIndex;
        RawData = rawData?.DeepClone();
        SourceNodeId = sourceNodeId;
        TargetNodeId = targetNodeId;
    }

    public int OriginalIndex { get; }
    public JsonNode? RawData { get; }
    public Guid? SourceNodeId { get; }
    public Guid? TargetNodeId { get; }
}
