using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public enum LuaNodeMemberKind
{
    Function,
    Event,
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
        LuaTypeReference declaringType,
        bool moduleReturn = false
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
        ModuleReturn = moduleReturn;
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
    public bool ModuleReturn { get; }
    public string RuntimePath
    {
        get
        {
            string? moduleName = DeclaringType.ModuleName;
            if (moduleName is null)
                return $"{DeclaringType.TypeName}.{Name}";
            int separator = moduleName.LastIndexOf('.');
            string moduleTypeName = separator < 0 ? moduleName : moduleName[(separator + 1)..];
            return ModuleReturn || string.Equals(moduleTypeName, DeclaringType.TypeName, StringComparison.Ordinal)
                ? $"{moduleName}.{Name}"
                : $"{moduleName}.{DeclaringType.TypeName}.{Name}";
        }
    }
}
