using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintGraphNodeDefinition
{
    public BlueprintGraphNodeDefinition(
        string runtimePath,
        IReadOnlyList<BlueprintGraphPortDefinition> ports,
        JsonObject? meta = null,
        IReadOnlyList<string>? runtimeAliases = null,
        string? metadataPath = null,
        string? memberName = null,
        LuaTypeReference? declaringType = null,
        bool isParent = false,
        bool isContextRelevant = false,
        bool isLatent = false)
    {
        RuntimePath = runtimePath;
        Ports = ports;
        Meta = meta?.DeepClone() as JsonObject ?? [];
        RuntimeAliases = runtimeAliases ?? [runtimePath];
        MetadataPath = metadataPath ?? runtimePath;
        MemberName = memberName ?? runtimePath.Split('.').LastOrDefault() ?? runtimePath;
        DeclaringType = declaringType;
        IsParent = isParent;
        IsContextRelevant = isContextRelevant;
        IsLatent = isLatent;
    }

    public string RuntimePath { get; }
    public IReadOnlyList<BlueprintGraphPortDefinition> Ports { get; }
    public JsonObject Meta { get; }
    public IReadOnlyList<string> RuntimeAliases { get; }
    public string MetadataPath { get; }
    public string MemberName { get; }
    public LuaTypeReference? DeclaringType { get; }
    public bool IsParent { get; }
    public bool IsContextRelevant { get; }
    public bool IsLatent { get; }
}
