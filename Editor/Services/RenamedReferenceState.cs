using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Services;

internal sealed class RenamedReferenceState
{
    internal static readonly RenamedReferenceState Empty = new([], new HashSet<string>(StringComparer.Ordinal));
    private readonly HashSet<(string TypeKey, string MemberId)> generalMembers;
    private readonly HashSet<string> assetReferences;

    private RenamedReferenceState(HashSet<(string TypeKey, string MemberId)> generalMembers,
        HashSet<string> assetReferences)
    {
        this.generalMembers = generalMembers;
        this.assetReferences = assetReferences;
    }

    internal IEnumerable<(string TypeKey, string MemberId)> GeneralMembers
        => generalMembers.Select(member => member);

    internal bool ContainsAsset(string reference) => assetReferences.Contains(reference);

    internal RenamedReferenceState WithGeneralMemberRename(string typeKey, string oldId, string newId)
    {
        HashSet<(string TypeKey, string MemberId)> members = new(generalMembers)
        {
            (typeKey, oldId),
            (typeKey, newId),
        };
        return new RenamedReferenceState(members, assetReferences);
    }

    internal RenamedReferenceState WithGeneralTypeRename(string oldKey, string newKey)
    {
        (string TypeKey, string MemberId)[] renamed = generalMembers.Where(member => member.TypeKey == oldKey).ToArray();
        if (renamed.Length == 0)
            return this;
        HashSet<(string TypeKey, string MemberId)> members = new(generalMembers);
        foreach ((string _, string memberId) in renamed)
            members.Add((newKey, memberId));
        return new RenamedReferenceState(members, assetReferences);
    }

    internal RenamedReferenceState WithAssetReferences(IEnumerable<string> references)
    {
        HashSet<string> assets = new(assetReferences, StringComparer.Ordinal);
        assets.UnionWith(references);
        return assets.SetEquals(assetReferences) ? this : new RenamedReferenceState(generalMembers, assets);
    }
}
