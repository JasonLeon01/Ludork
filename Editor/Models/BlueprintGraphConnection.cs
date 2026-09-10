using System;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintGraphConnection
{
    public BlueprintGraphConnection(
        Guid id,
        int? originalIndex,
        BlueprintGraphEndpoint source,
        BlueprintGraphEndpoint target,
        Guid sourcePortId,
        Guid targetPortId,
        BlueprintGraphPortKind kind,
        int sourcePinIndex,
        int targetPinIndex,
        JsonObject rawData)
    {
        Id = id;
        OriginalIndex = originalIndex;
        Source = source;
        Target = target;
        SourcePortId = sourcePortId;
        TargetPortId = targetPortId;
        Kind = kind;
        SourcePinIndex = sourcePinIndex;
        TargetPinIndex = targetPinIndex;
        RawData = (JsonObject)rawData.DeepClone();
    }

    public Guid Id { get; }
    public int? OriginalIndex { get; }
    public BlueprintGraphEndpoint Source { get; }
    public BlueprintGraphEndpoint Target { get; }
    public Guid SourcePortId { get; }
    public Guid TargetPortId { get; }
    public BlueprintGraphPortKind Kind { get; }
    public int SourcePinIndex { get; }
    public int TargetPinIndex { get; }
    public JsonObject RawData { get; }
}
