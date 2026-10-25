using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Plugin.Abstractions;

public sealed record PluginActorBlueprintSummary(string Key, string DisplayName);

public sealed record PluginMapPassage(bool Passable, bool Up, bool Right, bool Down, bool Left);

public sealed record PluginMapRegion(int X, int Y, int Width, int Height);

public sealed record PluginMapActorPlacement(string BlueprintReference, int X, int Y);

public sealed record PluginMapActorRegionWriteRequest(
    string MapKey,
    string LayerName,
    string BaseRevision,
    PluginMapRegion Region,
    string PreserveBaseClassReference,
    IReadOnlyList<PluginMapActorPlacement> Placements);

public sealed record PluginProjectScriptResult(bool Success, JsonObject? Data, string Error);
