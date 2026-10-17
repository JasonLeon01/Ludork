using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using Ludork.Models;

namespace Ludork.Controls;

public sealed class ActorSelectionChangedEventArgs(
    string mapKey,
    string? layerName,
    int? index,
    JsonObject? actorData,
    IReadOnlyList<MapActorReference>? actors = null,
    MapActorReference? primaryActor = null,
    bool refreshValues = false) : EventArgs
{
    public string MapKey { get; } = mapKey;
    public string? LayerName { get; } = layerName;
    public int? Index { get; } = index;
    public JsonObject? ActorData { get; } = actorData;
    public IReadOnlyList<MapActorReference> Actors { get; } = actors ?? [];
    public MapActorReference? PrimaryActor { get; } = primaryActor;
    public bool RefreshValues { get; } = refreshValues;
    public string? BlueprintReference { get; } = actorData?["bp"]?.GetValue<string>();
}
