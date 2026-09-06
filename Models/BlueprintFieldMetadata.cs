using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintFieldMetadata
{
    public BlueprintFieldMetadata(
        string name,
        LuaTypeReference type,
        bool hasDefaultValue,
        JsonNode? defaultValue,
        bool component,
        JsonObject meta,
        LuaTypeReference declaringType
    )
    {
        Name = name;
        Type = type;
        HasDefaultValue = hasDefaultValue;
        DefaultValue = defaultValue?.DeepClone();
        Component = component;
        Meta = (JsonObject)meta.DeepClone();
        DeclaringType = declaringType;
    }

    public string Name { get; }
    public LuaTypeReference Type { get; }
    public bool HasDefaultValue { get; }
    public JsonNode? DefaultValue { get; }
    public bool Component { get; }
    public JsonObject Meta { get; }
    public LuaTypeReference DeclaringType { get; }
}
