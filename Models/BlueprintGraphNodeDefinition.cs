using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintGraphNodeDefinition
{
    public BlueprintGraphNodeDefinition(
        string runtimePath,
        string title,
        IReadOnlyList<BlueprintGraphPortDefinition> ports,
        JsonObject? meta = null,
        IReadOnlyList<string>? runtimeAliases = null,
        IReadOnlyList<string>? pickerPath = null,
        string? memberName = null,
        string? description = null,
        LuaTypeReference? declaringType = null,
        bool isParent = false,
        bool isContextRelevant = false,
        bool hasExplicitDisplayName = false,
        bool isLatent = false)
    {
        RuntimePath = runtimePath;
        Title = title;
        Ports = ports;
        Meta = meta?.DeepClone() as JsonObject ?? [];
        RuntimeAliases = runtimeAliases ?? [runtimePath];
        PickerPath = pickerPath ?? [];
        MemberName = memberName ?? runtimePath.Split('.').LastOrDefault() ?? runtimePath;
        Description = description ?? string.Empty;
        DeclaringType = declaringType;
        IsParent = isParent;
        IsContextRelevant = isContextRelevant;
        HasExplicitDisplayName = hasExplicitDisplayName;
        IsLatent = isLatent;
    }

    public string RuntimePath { get; }
    public string Title { get; }
    public IReadOnlyList<BlueprintGraphPortDefinition> Ports { get; }
    public JsonObject Meta { get; }
    public IReadOnlyList<string> RuntimeAliases { get; }
    public IReadOnlyList<string> PickerPath { get; }
    public string MemberName { get; }
    public string Description { get; }
    public LuaTypeReference? DeclaringType { get; }
    public bool IsParent { get; }
    public bool IsContextRelevant { get; }
    public bool HasExplicitDisplayName { get; }
    public bool IsLatent { get; }
}
