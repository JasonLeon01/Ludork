using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Collections.Frozen;
using System.IO;
using System.Linq;

namespace Ludork.Services;

public sealed class ReferenceIndexSnapshot
{
    internal static readonly IReadOnlyDictionary<string, string> DataRoots = new Dictionary<string, string>(StringComparer.Ordinal)
    {
        ["config"] = "Configs", ["tileset"] = "Tilesets", ["autoTile"] = "AutoTiles", ["map"] = "Maps",
        ["worldMap"] = "WorldMaps", ["commonFunction"] = "CommonFunctions", ["blueprint"] = "Blueprints",
        ["animation"] = "Animations", ["particle"] = "Particles", ["curve"] = "Curves", ["subtitle"] = "Subtitles",
        ["textConfig"] = "TextConfigs", ["uiAsset"] = "UI", ["general"] = "General",
    }.ToFrozenDictionary(StringComparer.Ordinal);

    private readonly string projectPath;
    private readonly FrozenDictionary<string, ReferenceNode> nodes;
    private readonly FrozenDictionary<string, string> paths;
    private readonly FrozenDictionary<string, IReadOnlyList<ReferenceRecord>> outgoing;
    private readonly FrozenDictionary<string, IReadOnlyList<ReferenceRecord>> incoming;
    internal IReadOnlyDictionary<string, string> GeneralMemberTypes { get; }
    internal IReadOnlyList<ReferenceNode> Nodes { get; }
    internal IReadOnlyList<ReferenceRecord> References { get; }
    internal IReadOnlySet<string> DeclaredNodes { get; }

    internal ReferenceIndexSnapshot(string projectPath, long version, IEnumerable<ReferenceNode> nodes,
        IEnumerable<ReferenceRecord> references, IReadOnlyDictionary<string, string> generalMemberTypes, IEnumerable<string> declaredNodes)
    {
        this.projectPath = projectPath;
        Version = version;
        DeclaredNodes = declaredNodes.ToFrozenSet(StringComparer.Ordinal);
        this.nodes = nodes.ToFrozenDictionary(node => node.Id, StringComparer.Ordinal);
        GeneralMemberTypes = generalMemberTypes.ToFrozenDictionary(StringComparer.Ordinal);
        paths = this.nodes.Keys.ToFrozenDictionary(id => id, id => ResolvePath(projectPath, id, GeneralMemberTypes), StringComparer.Ordinal);
        Nodes = Array.AsReadOnly(this.nodes.Values.OrderBy(node => node.Id, StringComparer.Ordinal).ToArray());
        References = Array.AsReadOnly(references.Distinct().OrderBy(record => record.Source, StringComparer.Ordinal)
            .ThenBy(record => record.Target, StringComparer.Ordinal).ThenBy(record => record.Path, StringComparer.Ordinal).ToArray());
        outgoing = References.GroupBy(record => record.Source).ToFrozenDictionary(group => group.Key,
            group => sort(group, record => record.Target), StringComparer.Ordinal);
        incoming = References.GroupBy(record => record.Target).ToFrozenDictionary(group => group.Key,
            group => sort(group, record => record.Source), StringComparer.Ordinal);
    }

    public long Version { get; }
    public ReferenceNode? GetNode(string id) => nodes.GetValueOrDefault(id) ?? ParseNode(id);
    public string GetNodePath(string id) => paths.GetValueOrDefault(id) ?? ResolvePath(projectPath, id, GeneralMemberTypes);
    public IReadOnlyList<ReferenceRecord> GetIncoming(string id) => incoming.GetValueOrDefault(id) ?? [];
    public IReadOnlyList<ReferenceRecord> GetOutgoing(string id) => outgoing.GetValueOrDefault(id) ?? [];

    public ReferenceTreeNode GetTree(string id, ReferenceDirection direction, int maxDepth = 5)
        => buildTree(id, direction, Math.Max(0, maxDepth), new HashSet<string>(StringComparer.Ordinal) { id });

    private ReferenceTreeNode buildTree(string id, ReferenceDirection direction, int depth, IReadOnlySet<string> stack)
    {
        IReadOnlyList<ReferenceRecord> records = direction == ReferenceDirection.ReferencedBy ? GetIncoming(id) : GetOutgoing(id);
        List<ReferenceTreeItem> items = [];
        foreach (ReferenceRecord record in records)
        {
            string childId = direction == ReferenceDirection.ReferencedBy ? record.Source : record.Target;
            bool cycle = stack.Contains(childId);
            ReferenceTreeNode child = !cycle && depth > 0
                ? buildTree(childId, direction, depth - 1, new HashSet<string>(stack, StringComparer.Ordinal) { childId })
                : new ReferenceTreeNode(childId, [], cycle);
            items.Add(new ReferenceTreeItem(record, child));
        }
        return new ReferenceTreeNode(id, items.AsReadOnly(), false);
    }

    private IReadOnlyList<ReferenceRecord> sort(IEnumerable<ReferenceRecord> records, Func<ReferenceRecord, string> selector)
        => Array.AsReadOnly(records.OrderBy(record => GetNode(selector(record))?.Type, StringComparer.Ordinal)
            .ThenBy(record => GetNode(selector(record))?.Key, StringComparer.Ordinal)
            .ThenBy(record => record.Path, StringComparer.Ordinal).ToArray());

    internal static string NodeId(string type, string key) => type == "subtitle"
        ? "asset:/Game/Assets/Subtitles/" + key.Replace('\\', '/') + ".json"
        : type + ":" + key.Replace('\\', '/');

    internal static ReferenceNode? ParseNode(string id)
    {
        int separator = id.IndexOf(':');
        return separator < 0 ? null : new ReferenceNode(id, id[..separator], id[(separator + 1)..]);
    }

    internal static string ResolvePath(string projectPath, string id, IReadOnlyDictionary<string, string> generalMemberTypes)
    {
        if (ParseNode(id) is not ReferenceNode node)
            return string.Empty;
        string key = node.Key;
        if (node.Type == "asset")
            return GameAssetPath.TryToProjectFile(projectPath, key, out string assetPath) ? assetPath : string.Empty;
        if (node.Type == "blueprint")
            key = BlueprintReference.NormalizeKey(key);
        if (node.Type == "generalMember")
            return generalMemberTypes.TryGetValue(id, out string? type)
                ? Path.Combine(projectPath, "Data", "General", type.Replace('/', Path.DirectorySeparatorChar) + ".json") : string.Empty;
        if (node.Type == "worldMap")
            return Path.Combine(projectPath, "Data", "Maps", key.Replace('/', Path.DirectorySeparatorChar), "_world.json");
        return DataRoots.TryGetValue(node.Type, out string? root)
            ? Path.Combine(projectPath, "Data", root, key.Replace('/', Path.DirectorySeparatorChar) + ".json") : string.Empty;
    }
}
