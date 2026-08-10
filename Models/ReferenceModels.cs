using System.Collections.Generic;

namespace Ludork.Models;

public enum ReferenceDirection
{
    References,
    ReferencedBy,
}

public sealed record ReferenceNode(string Id, string Type, string Key);

public sealed record ReferenceRecord(
    string Source,
    string Target,
    string Kind,
    string Path
);

public sealed record ReferenceTreeItem(
    ReferenceRecord Reference,
    ReferenceTreeNode Child
);

public sealed record ReferenceTreeNode(
    string NodeId,
    IReadOnlyList<ReferenceTreeItem> Items,
    bool Cycle
);

public sealed record ReferenceImpact(
    IReadOnlyList<string> NodeIds,
    IReadOnlyList<ReferenceRecord> Incoming
)
{
    public bool HasExternalReferences => Incoming.Count != 0;
}
