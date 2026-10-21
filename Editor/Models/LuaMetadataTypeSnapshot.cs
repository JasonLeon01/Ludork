using System;
using System.Collections.Frozen;
using System.Collections.Generic;
using System.IO;
using System.Threading;

namespace Ludork.Models;

public sealed class LuaMetadataTypeSnapshot
{
    private readonly FrozenDictionary<string, LuaMetadataType> types;
    private readonly FrozenDictionary<string, LuaEnumDefinition> enums;
    private readonly FrozenDictionary<string, FrozenSet<string>> ancestors;
    private readonly FrozenDictionary<string, FrozenSet<string>> compatibleTargets;

    internal LuaMetadataTypeSnapshot(
        IReadOnlyDictionary<string, LuaMetadataType> types,
        IReadOnlyDictionary<string, LuaEnumDefinition> enums,
        IReadOnlyDictionary<string, IReadOnlySet<string>> ancestors,
        CancellationToken cancellationToken)
    {
        this.types = types.ToFrozenDictionary(StringComparer.Ordinal);
        this.enums = enums.ToFrozenDictionary(StringComparer.Ordinal);
        this.ancestors = ancestors.ToFrozenDictionary(
            pair => pair.Key, pair => pair.Value.ToFrozenSet(StringComparer.Ordinal), StringComparer.Ordinal);
        Dictionary<string, FrozenSet<string>> relationships = new(this.types.Count, StringComparer.Ordinal);
        Func<string, string, bool> isDerivedType = isDerived;
        Func<LuaMetadataType, LuaEnumDefinition> resolve = resolveEnum;
        foreach (KeyValuePair<string, LuaMetadataType> source in this.types)
        {
            cancellationToken.ThrowIfCancellationRequested();
            HashSet<string> targets = new(StringComparer.Ordinal);
            foreach (KeyValuePair<string, LuaMetadataType> target in this.types)
            {
                cancellationToken.ThrowIfCancellationRequested();
                if (source.Value.IsAssignableTo(target.Value, isDerivedType, resolve))
                    targets.Add(target.Key);
            }
            relationships.Add(source.Key, targets.ToFrozenSet(StringComparer.Ordinal));
        }
        compatibleTargets = relationships.ToFrozenDictionary(StringComparer.Ordinal);
    }

    public bool IsTypeAssignable(string source, string target)
    {
        if (compatibleTargets.TryGetValue(source, out FrozenSet<string>? targets) && types.ContainsKey(target))
            return targets.Contains(target);
        LuaMetadataType sourceType = types.GetValueOrDefault(source) ?? LuaMetadataType.Parse(source);
        LuaMetadataType targetType = types.GetValueOrDefault(target) ?? LuaMetadataType.Parse(target);
        return sourceType.IsAssignableTo(targetType, isDerived, resolveEnum);
    }

    private bool isDerived(string source, string target)
        => ancestors.TryGetValue(source, out FrozenSet<string>? parents) && parents.Contains(target);

    private LuaEnumDefinition resolveEnum(LuaMetadataType schema)
    {
        if (!enums.TryGetValue(schema.ToString(), out LuaEnumDefinition? definition))
            throw new InvalidDataException($"Enum type was not captured: {schema}");
        return definition;
    }
}
