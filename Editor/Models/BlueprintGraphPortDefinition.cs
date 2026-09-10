using System.Text.Json.Nodes;

namespace Ludork.Models;

public enum BlueprintGraphPortKind
{
    Exec,
    Params,
}

public enum BlueprintGraphPortDirection
{
    Input,
    Output,
}

public sealed class BlueprintGraphPortDefinition
{
    public BlueprintGraphPortDefinition(
        string name,
        BlueprintGraphPortKind kind,
        BlueprintGraphPortDirection direction,
        int pinIndex,
        string typeName = "any",
        int? parameterIndex = null,
        bool supportsEditor = false,
        JsonNode? defaultValue = null,
        JsonObject? meta = null)
    {
        Name = name;
        Kind = kind;
        Direction = direction;
        PinIndex = pinIndex;
        TypeName = typeName;
        ParameterIndex = parameterIndex;
        SupportsEditor = supportsEditor;
        DefaultValue = defaultValue?.DeepClone();
        Meta = meta?.DeepClone() as JsonObject ?? [];
    }

    public string Name { get; }
    public BlueprintGraphPortKind Kind { get; }
    public BlueprintGraphPortDirection Direction { get; }
    public int PinIndex { get; }
    public string TypeName { get; }
    public int? ParameterIndex { get; }
    public bool SupportsEditor { get; }
    public JsonNode? DefaultValue { get; }
    public JsonObject Meta { get; }
}
