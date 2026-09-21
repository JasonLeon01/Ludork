using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed record BlueprintParameterTextDraft(string Text, JsonNode? Value, string? Error)
{
    public int? ParameterIndex { get; init; }
}
