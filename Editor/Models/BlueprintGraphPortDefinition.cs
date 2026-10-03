using System;
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
    private readonly JsonNode? defaultValue;

    public BlueprintGraphPortDefinition(
        string name,
        BlueprintGraphPortKind kind,
        BlueprintGraphPortDirection direction,
        int pinIndex,
        string typeName = "any",
        int? parameterIndex = null,
        bool supportsEditor = false,
        JsonNode? defaultValue = null,
        JsonObject? meta = null,
        Func<string, LuaEnumDefinition>? resolveEnum = null,
        Func<JsonNode?>? createDefault = null)
    {
        Name = name;
        Kind = kind;
        Direction = direction;
        PinIndex = pinIndex;
        TypeName = typeName;
        ParameterIndex = parameterIndex;
        SupportsEditor = supportsEditor;
        this.defaultValue = defaultValue?.DeepClone();
        ResolveEnum = resolveEnum;
        CreateDefault = createDefault;
        Meta = meta?.DeepClone() as JsonObject ?? [];
    }

    public string Name { get; }
    public BlueprintGraphPortKind Kind { get; }
    public BlueprintGraphPortDirection Direction { get; }
    public int PinIndex { get; }
    public string TypeName { get; }
    public int? ParameterIndex { get; }
    public bool SupportsEditor { get; }
    public Func<string, LuaEnumDefinition>? ResolveEnum { get; }
    public Func<JsonNode?>? CreateDefault { get; }
    public JsonNode? DefaultValue => CreateDefault is null ? defaultValue?.DeepClone() : CreateDefault();
    public JsonObject Meta { get; }
}
