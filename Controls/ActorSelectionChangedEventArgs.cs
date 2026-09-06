using System;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class ActorSelectionChangedEventArgs(
    string mapKey,
    string? layerName,
    int? index,
    JsonObject? actorData) : EventArgs
{
    public string MapKey { get; } = mapKey;
    public string? LayerName { get; } = layerName;
    public int? Index { get; } = index;
    public JsonObject? ActorData { get; } = actorData;
    public string? BlueprintReference { get; } = actorData?["bp"]?.GetValue<string>();
}
