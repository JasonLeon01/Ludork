using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

internal sealed record BlueprintAttributeChange(
    IReadOnlyDictionary<string, JsonNode?> Updates,
    IReadOnlyList<string> Removals,
    IReadOnlyList<string> StaleFields,
    string? Error)
{
    public static BlueprintAttributeChange Failed(string error) => new(
        new Dictionary<string, JsonNode?>(), [], [], error);
}

internal sealed record BlueprintGraphEditorData(
    BlueprintGraphDocument Document,
    IReadOnlyList<BlueprintGraphNodeDefinition> Definitions);

internal sealed record BlueprintEditorTabItem(string Label, string? EventName, bool IsPreview)
{
    public override string ToString() => Label;
}
