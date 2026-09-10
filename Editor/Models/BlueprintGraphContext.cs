using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed record BlueprintGraphContext(JsonObject Data, string? BlueprintKey);
