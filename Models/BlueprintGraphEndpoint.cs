using System;

namespace Ludork.Models;

public sealed class BlueprintGraphEndpoint
{
    private BlueprintGraphEndpoint(Guid? nodeId, string? externalKey)
    {
        NodeId = nodeId;
        ExternalKey = externalKey;
    }

    public Guid? NodeId { get; }
    public string? ExternalKey { get; }
    public bool IsExternal => ExternalKey is not null;

    public static BlueprintGraphEndpoint Node(Guid nodeId)
    {
        return new BlueprintGraphEndpoint(nodeId, null);
    }

    public static BlueprintGraphEndpoint External(string externalKey, Guid nodeId)
    {
        return new BlueprintGraphEndpoint(nodeId, externalKey);
    }
}
