using System.Collections.Generic;
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

public sealed class LuaTypeMetadata
{
    public LuaTypeMetadata(
        LuaTypeReference type,
        IReadOnlyList<string> attrs,
        IReadOnlyList<LuaTypeReference> bases,
        IReadOnlyDictionary<string, BlueprintFieldMetadata> fields,
        JsonObject meta,
        IReadOnlyList<string> invalidVars,
        JsonObject rectRangeVars
    ) : this(
        type,
        attrs,
        bases,
        fields,
        meta,
        invalidVars,
        rectRangeVars,
        [],
        new Dictionary<string, LuaNodeMemberMetadata>()
    )
    {
    }

    public LuaTypeMetadata(
        LuaTypeReference type,
        IReadOnlyList<string> attrs,
        IReadOnlyList<LuaTypeReference> bases,
        IReadOnlyDictionary<string, BlueprintFieldMetadata> fields,
        JsonObject meta,
        IReadOnlyList<string> invalidVars,
        JsonObject rectRangeVars,
        IReadOnlyList<string> memberNames,
        IReadOnlyDictionary<string, LuaNodeMemberMetadata> members
    )
    {
        Type = type;
        Attrs = attrs;
        Bases = bases;
        Fields = fields;
        Meta = (JsonObject)meta.DeepClone();
        InvalidVars = invalidVars;
        RectRangeVars = (JsonObject)rectRangeVars.DeepClone();
        MemberNames = memberNames;
        Members = members;
    }

    public LuaTypeReference Type { get; }
    public IReadOnlyList<string> Attrs { get; }
    public IReadOnlyList<LuaTypeReference> Bases { get; }
    public IReadOnlyDictionary<string, BlueprintFieldMetadata> Fields { get; }
    public JsonObject Meta { get; }
    public IReadOnlyList<string> InvalidVars { get; }
    public JsonObject RectRangeVars { get; }
    public IReadOnlyList<string> MemberNames { get; }
    public IReadOnlyDictionary<string, LuaNodeMemberMetadata> Members { get; }
}
