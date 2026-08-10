using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public enum LuaNodeMemberKind
{
    Function,
    Event,
}

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

public sealed class LuaNodeReturnMetadata
{
    public LuaNodeReturnMetadata(string name, LuaTypeReference type)
    {
        Name = name;
        Type = type;
    }

    public string Name { get; }
    public LuaTypeReference Type { get; }
}

public sealed class LuaNodeMemberMetadata
{
    public LuaNodeMemberMetadata(
        string name,
        LuaNodeMemberKind kind,
        IReadOnlyList<LuaNodeParameterMetadata> parameters,
        IReadOnlyList<LuaNodeReturnMetadata> returns,
        IReadOnlyList<string> executionOutputs,
        JsonObject execSplit,
        JsonNode? latent,
        IReadOnlyList<string> latentOutputs,
        JsonObject latentStates,
        JsonNode? loopNode,
        bool pure,
        JsonObject meta,
        LuaTypeReference declaringType
    )
    {
        Name = name;
        Kind = kind;
        Parameters = parameters;
        Returns = returns;
        ExecutionOutputs = executionOutputs;
        ExecSplit = (JsonObject)execSplit.DeepClone();
        Latent = latent?.DeepClone();
        LatentOutputs = latentOutputs;
        LatentStates = (JsonObject)latentStates.DeepClone();
        LoopNode = loopNode?.DeepClone();
        Pure = pure;
        Meta = (JsonObject)meta.DeepClone();
        DeclaringType = declaringType;
    }

    public string Name { get; }
    public LuaNodeMemberKind Kind { get; }
    public IReadOnlyList<LuaNodeParameterMetadata> Parameters { get; }
    public IReadOnlyList<LuaNodeReturnMetadata> Returns { get; }
    public IReadOnlyList<string> ExecutionOutputs { get; }
    public JsonObject ExecSplit { get; }
    public JsonNode? Latent { get; }
    public bool IsLatent
    {
        get
        {
            if (Latent is JsonValue value && value.TryGetValue(out bool enabled))
                return enabled;
            return Latent is not null;
        }
    }
    public IReadOnlyList<string> LatentOutputs { get; }
    public JsonObject LatentStates { get; }
    public JsonNode? LoopNode { get; }
    public bool Pure { get; }
    public JsonObject Meta { get; }
    public LuaTypeReference DeclaringType { get; }
    public string RuntimePath
    {
        get
        {
            string? moduleName = DeclaringType.ModuleName;
            if (moduleName is null)
                return $"{DeclaringType.TypeName}.{Name}";
            int separator = moduleName.LastIndexOf('.');
            string moduleTypeName = separator < 0 ? moduleName : moduleName[(separator + 1)..];
            return string.Equals(moduleTypeName, DeclaringType.TypeName, StringComparison.Ordinal)
                ? $"{moduleName}.{Name}"
                : $"{moduleName}.{DeclaringType.TypeName}.{Name}";
        }
    }
}
