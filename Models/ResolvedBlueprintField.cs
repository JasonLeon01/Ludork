using System;
using System.Collections.Generic;
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

public sealed class ResolvedBlueprintClass
{
    private readonly IReadOnlyDictionary<string, ResolvedBlueprintField> fieldsByName;

    public ResolvedBlueprintClass(
        string classReference,
        string? terminalReference,
        LuaTypeReference? rootType,
        IReadOnlyList<ResolvedBlueprintField> fields,
        JsonObject meta,
        IReadOnlyList<string> invalidVars,
        JsonObject rectRangeVars,
        bool scriptMixin,
        bool hasBlueprintParent,
        bool parentScriptMixin,
        IReadOnlyList<string> declaredFieldNames,
        IReadOnlyList<string> localMixinFieldNames,
        string? scriptMixinError,
        long resolverRevision,
        long metadataRevision
    )
    {
        ClassReference = classReference;
        TerminalReference = terminalReference;
        RootType = rootType;
        Fields = fields;
        Meta = (JsonObject)meta.DeepClone();
        InvalidVars = invalidVars;
        RectRangeVars = (JsonObject)rectRangeVars.DeepClone();
        ScriptMixin = scriptMixin;
        HasBlueprintParent = hasBlueprintParent;
        ParentScriptMixin = parentScriptMixin;
        DeclaredFieldNames = declaredFieldNames;
        LocalMixinFieldNames = localMixinFieldNames;
        ScriptMixinError = scriptMixinError;
        ResolverRevision = resolverRevision;
        MetadataRevision = metadataRevision;
        Dictionary<string, ResolvedBlueprintField> lookup = new(StringComparer.Ordinal);
        foreach (ResolvedBlueprintField field in fields)
            lookup[field.Name] = field;
        fieldsByName = lookup;
    }

    public string ClassReference { get; }
    public string? TerminalReference { get; }
    public LuaTypeReference? RootType { get; }
    public IReadOnlyList<ResolvedBlueprintField> Fields { get; }
    public JsonObject Meta { get; }
    public IReadOnlyList<string> InvalidVars { get; }
    public JsonObject RectRangeVars { get; }
    public bool ScriptMixin { get; }
    public bool HasBlueprintParent { get; }
    public bool ParentScriptMixin { get; }
    public IReadOnlyList<string> DeclaredFieldNames { get; }
    public IReadOnlyList<string> LocalMixinFieldNames { get; }
    public string? ScriptMixinError { get; }
    public long ResolverRevision { get; }
    public long MetadataRevision { get; }

    public ResolvedBlueprintField? GetField(string name)
    {
        return fieldsByName.TryGetValue(name, out ResolvedBlueprintField? field) ? field : null;
    }

    public JsonNode? GetValue(string name)
    {
        return GetField(name)?.Value?.DeepClone();
    }
}
