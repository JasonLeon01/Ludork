using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Services;

internal sealed class LuaMetadataCatalog
{
    private readonly LuaMetadataFileCache files;
    private readonly LuaMetadataHierarchy hierarchy;
    private readonly Dictionary<NodeMemberCacheKey, IReadOnlyList<LuaNodeMemberMetadata>> nodeMemberCache = [];
    private readonly Dictionary<LuaNodeMemberKind, IReadOnlyList<LuaNodeMemberMetadata>> enumeratedNodeMemberCache = [];
    private IReadOnlyList<LuaNodeMemberMetadata>? enumeratedNodeMembers;
    private IReadOnlyList<LuaTypeMetadata>? enumeratedTypes;

    public LuaMetadataCatalog(LuaMetadataFileCache files, LuaMetadataHierarchy hierarchy)
    {
        this.files = files;
        this.hierarchy = hierarchy;
        files.Invalidated += clear;
    }

    private void clear()
    {
        nodeMemberCache.Clear();
        enumeratedNodeMemberCache.Clear();
        enumeratedNodeMembers = null;
        enumeratedTypes = null;
    }

    public IReadOnlyList<LuaNodeMemberMetadata> GetNodeMembers(
        LuaTypeReference type,
        LuaNodeMemberKind? kind = null,
        bool includeInherited = true
    )
    {
        NodeMemberCacheKey cacheKey = new(type, kind, includeInherited);
        if (nodeMemberCache.TryGetValue(cacheKey, out IReadOnlyList<LuaNodeMemberMetadata>? cached))
            return cached;
        LuaTypeMetadata? declaredType = files.GetType(type);
        if (declaredType is null)
        {
            IReadOnlyList<LuaNodeMemberMetadata> empty = Array.Empty<LuaNodeMemberMetadata>();
            nodeMemberCache[cacheKey] = empty;
            return empty;
        }
        IReadOnlyList<LuaTypeMetadata> resolvedHierarchy = includeInherited
            ? hierarchy.ResolveMro(declaredType.Type)
            : [declaredType];
        Dictionary<string, LuaNodeMemberMetadata> merged = new(StringComparer.Ordinal);
        List<string> order = [];
        foreach (LuaTypeMetadata metadata in resolvedHierarchy.Reverse())
        {
            foreach (string name in metadata.MemberNames)
            {
                if (!metadata.Members.TryGetValue(name, out LuaNodeMemberMetadata? member))
                    continue;
                if (!merged.ContainsKey(name))
                    order.Add(name);
                merged[name] = member;
            }
        }
        IReadOnlyList<LuaNodeMemberMetadata> result = order
            .Select(name => merged[name])
            .Where(member => kind is null || member.Kind == kind)
            .ToArray();
        nodeMemberCache[cacheKey] = result;
        return result;
    }

    public IReadOnlyList<LuaNodeMemberMetadata> EnumerateNodeMembers(LuaNodeMemberKind? kind = null)
    {
        if (kind is LuaNodeMemberKind memberKind
            && enumeratedNodeMemberCache.TryGetValue(memberKind, out IReadOnlyList<LuaNodeMemberMetadata>? filtered))
        {
            return filtered;
        }
        if (enumeratedNodeMembers is null)
        {
            List<LuaNodeMemberMetadata> result = [];
            foreach (LuaTypeMetadata metadata in files.ReadAllTypes())
            {
                foreach (string name in metadata.MemberNames)
                {
                    if (!metadata.Members.TryGetValue(name, out LuaNodeMemberMetadata? member))
                        continue;
                    result.Add(member);
                }
            }
            enumeratedNodeMembers = result.ToArray();
        }
        if (kind is null)
            return enumeratedNodeMembers;
        IReadOnlyList<LuaNodeMemberMetadata> nextFiltered = enumeratedNodeMembers
            .Where(member => member.Kind == kind)
            .ToArray();
        enumeratedNodeMemberCache[kind.Value] = nextFiltered;
        return nextFiltered;
    }

    public IReadOnlyList<LuaTypeMetadata> EnumerateTypes()
    {
        if (enumeratedTypes is not null)
            return enumeratedTypes;
        Dictionary<string, LuaTypeMetadata> result = new(StringComparer.Ordinal);
        foreach (LuaTypeMetadata metadata in files.ReadAllTypes())
            result[metadata.Type.QualifiedName] = metadata;
        enumeratedTypes = result.Values
            .OrderBy(metadata => metadata.Type.QualifiedName, StringComparer.Ordinal)
            .ToArray();
        return enumeratedTypes;
    }

    private readonly record struct NodeMemberCacheKey(
        LuaTypeReference Type,
        LuaNodeMemberKind? Kind,
        bool IncludeInherited);

}
