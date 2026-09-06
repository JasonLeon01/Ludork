using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class ResolvedBlueprintField
{
    public ResolvedBlueprintField(
        string name,
        LuaTypeReference type,
        JsonNode? value,
        JsonNode? blueprintDefaultValue,
        BlueprintFieldMetadata? metadata,
        bool isUnknown,
        bool hasBlueprintDefaultValue = true
    )
    {
        Name = name;
        Type = type;
        Value = value?.DeepClone();
        BlueprintDefaultValue = blueprintDefaultValue?.DeepClone();
        Metadata = metadata;
        IsUnknown = isUnknown;
        HasBlueprintDefaultValue = hasBlueprintDefaultValue;
    }

    public string Name { get; }
    public LuaTypeReference Type { get; }
    public JsonNode? Value { get; }
    public JsonNode? BlueprintDefaultValue { get; }
    public BlueprintFieldMetadata? Metadata { get; }
    public bool IsUnknown { get; }
    public bool HasBlueprintDefaultValue { get; }
}
