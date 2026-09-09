using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services.UiAssets;

public sealed class UiAssetDependencyGraph
{
    public sealed record Reference(string TargetKey, string Path);

    public sealed record CycleIssue(string AssetKey, string Path, string Message);

    private readonly Dictionary<string, JsonObject> assets =
        new Dictionary<string, JsonObject>(StringComparer.Ordinal);
    private readonly Dictionary<string, IReadOnlyList<Reference>> references =
        new Dictionary<string, IReadOnlyList<Reference>>(StringComparer.Ordinal);

    public UiAssetDependencyGraph(
        IReadOnlyDictionary<string, JsonObject> assets,
        string? workingKey = null,
        JsonObject? workingAsset = null)
    {
        foreach (KeyValuePair<string, JsonObject> pair in assets)
        {
            string logicalKey = UiAssetSchema.ToLogicalAssetKey(pair.Key);
            if (logicalKey.Length != 0)
                this.assets[logicalKey] = (JsonObject)pair.Value.DeepClone();
        }
        if (workingKey is not null
            && UiAssetSchema.NormalizeAssetKey(workingKey).Length != 0
            && workingAsset is not null)
        {
            this.assets[workingKey] = (JsonObject)workingAsset.DeepClone();
        }
        foreach (KeyValuePair<string, JsonObject> pair in this.assets)
            references[pair.Key] = collectReferences(pair.Value);
    }

    public bool TryGetAsset(string assetKey, out JsonObject? asset)
    {
        asset = assets.TryGetValue(assetKey, out JsonObject? snapshot)
            ? (JsonObject)snapshot.DeepClone()
            : null;
        return asset is not null;
    }

    public IReadOnlyList<Reference> GetReferences(string assetKey)
    {
        return references.TryGetValue(assetKey, out IReadOnlyList<Reference>? assetReferences)
            ? assetReferences
            : Array.Empty<Reference>();
    }

    public bool CanReach(string assetKey, string targetKey)
    {
        if (!assets.ContainsKey(assetKey))
            return false;
        HashSet<string> visited = new HashSet<string>(StringComparer.Ordinal);
        Stack<string> pending = new Stack<string>();
        pending.Push(assetKey);
        while (pending.Count != 0)
        {
            string currentKey = pending.Pop();
            if (string.Equals(currentKey, targetKey, StringComparison.Ordinal))
                return true;
            if (!visited.Add(currentKey))
                continue;
            foreach (Reference reference in GetReferences(currentKey))
                pending.Push(reference.TargetKey);
        }
        return false;
    }

    public bool WouldCreateCycle(string assetKey, string targetKey)
    {
        return string.Equals(assetKey, targetKey, StringComparison.Ordinal)
            || CanReach(targetKey, assetKey);
    }

    public IReadOnlyDictionary<string, JsonObject> CollectDependencies(string assetKey)
    {
        Dictionary<string, JsonObject> dependencies =
            new Dictionary<string, JsonObject>(StringComparer.Ordinal);
        HashSet<string> visited = new HashSet<string>(StringComparer.Ordinal) { assetKey };
        collectDependencies(assetKey, dependencies, visited);
        return dependencies;
    }

    public IReadOnlyList<CycleIssue> FindCycles(string? assetKey = null)
    {
        List<CycleIssue> issues = [];
        Dictionary<string, int> states = new Dictionary<string, int>(StringComparer.Ordinal);
        List<string> stack = [];
        if (assetKey is not null)
        {
            if (assets.ContainsKey(assetKey))
                visitCycles(assetKey, states, stack, issues);
        }
        else
        {
            foreach (string key in assets.Keys.OrderBy(value => value, StringComparer.Ordinal))
                visitCycles(key, states, stack, issues);
        }
        return issues.ToArray();
    }

    private void collectDependencies(
        string assetKey,
        IDictionary<string, JsonObject> dependencies,
        ISet<string> visited)
    {
        foreach (Reference reference in GetReferences(assetKey))
        {
            if (!visited.Add(reference.TargetKey)
                || !assets.TryGetValue(reference.TargetKey, out JsonObject? dependency))
            {
                continue;
            }
            dependencies[reference.TargetKey] = (JsonObject)dependency.DeepClone();
            collectDependencies(reference.TargetKey, dependencies, visited);
        }
    }

    private void visitCycles(
        string assetKey,
        IDictionary<string, int> states,
        IList<string> stack,
        ICollection<CycleIssue> issues)
    {
        if (states.TryGetValue(assetKey, out int state) && state != 0)
            return;
        states[assetKey] = 1;
        stack.Add(assetKey);
        foreach (Reference reference in GetReferences(assetKey))
        {
            if (!assets.ContainsKey(reference.TargetKey))
                continue;
            if (states.TryGetValue(reference.TargetKey, out int targetState) && targetState == 1)
            {
                int start = stack.IndexOf(reference.TargetKey);
                string cycle = string.Join(" -> ", stack.Skip(start).Append(reference.TargetKey));
                issues.Add(new CycleIssue(
                    assetKey,
                    reference.Path,
                    $"Nested UI asset cycle: {cycle}"));
                continue;
            }
            visitCycles(reference.TargetKey, states, stack, issues);
        }
        stack.RemoveAt(stack.Count - 1);
        states[assetKey] = 2;
    }

    private static IReadOnlyList<Reference> collectReferences(JsonObject asset)
    {
        List<Reference> result = [];
        if (asset["root"] is not JsonObject root)
            return result.AsReadOnly();
        Stack<(JsonObject Node, string Path)> pending = new Stack<(JsonObject Node, string Path)>();
        pending.Push((root, "root"));
        while (pending.Count != 0)
        {
            (JsonObject node, string path) = pending.Pop();
            if (node["controlId"] is JsonValue scalar
                && scalar.TryGetValue(out string? controlId)
                && controlId is not null
                && UiAssetSchema.TryGetProjectAssetKey(controlId, out string targetKey))
            {
                result.Add(new Reference(targetKey, path + ".controlId"));
            }
            if (node["children"] is not JsonArray children)
                continue;
            for (int index = children.Count - 1; index >= 0; index--)
            {
                if (children[index] is JsonObject child)
                    pending.Push((child, $"{path}.children[{index}]"));
            }
        }
        return result.AsReadOnly();
    }
}
