using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class LuaNodeParameterMetadata
{
    public LuaNodeParameterMetadata(
        string name,
        LuaTypeReference type,
        bool hasDefaultValue,
        JsonNode? defaultValue
    )
    {
        Name = name;
        Type = type;
        HasDefaultValue = hasDefaultValue;
        DefaultValue = defaultValue?.DeepClone();
    }

    public string Name { get; }
    public LuaTypeReference Type { get; }
    public bool HasDefaultValue { get; }
    public JsonNode? DefaultValue { get; }
}
