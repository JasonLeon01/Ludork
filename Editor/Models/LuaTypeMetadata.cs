using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

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
