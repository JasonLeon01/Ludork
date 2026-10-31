using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Services;

public sealed partial class MapOrderService
{
    private readonly ProjectDataStore store;
    private readonly Dictionary<string, IReadOnlyList<MapOrderNode>> trees = new(StringComparer.Ordinal);
    private readonly Dictionary<string, string> storedTexts = new(StringComparer.Ordinal);
    private readonly Dictionary<string, string> savedTexts = new(StringComparer.Ordinal);
    private static readonly string emptyText = serialize([]);

    internal MapOrderService(ProjectDataStore store)
    {
        this.store = store;
    }

    public bool IsModified => getActiveScopes().Any(isScopeModified);

    public static string GetScope(string mapKey)
    {
        int separator = mapKey.IndexOf('/');
        return separator < 0 ? string.Empty : mapKey[..separator];
    }

    public static string GetName(string mapKey)
    {
        int separator = mapKey.IndexOf('/');
        return separator < 0 ? mapKey : mapKey[(separator + 1)..];
    }

    public static string GetKey(string scope, string name) => scope.Length == 0 ? name : scope + "/" + name;

    public IReadOnlyList<MapOrderNode> GetTree(string scope)
    {
        IReadOnlyList<MapCatalogEntry> catalog = store.Maps.MapCatalog;
        if (scope.Length == 0)
        {
            string[] names = catalog
                .Where(entry => entry.Kind is MapCatalogEntryKind.StandaloneMap or MapCatalogEntryKind.WorldMap)
                .Select(entry => entry.Key).ToArray();
            HashSet<string> worlds = catalog.Where(entry => entry.Kind == MapCatalogEntryKind.WorldMap)
                .Select(entry => entry.Key).ToHashSet(StringComparer.Ordinal);
            return reconcile(getStoredTree(scope), names, worlds);
        }
        string[] children = catalog
            .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap
                && string.Equals(entry.WorldKey, scope, StringComparison.Ordinal))
            .Select(entry => GetName(entry.Key)).ToArray();
        return reconcile(getStoredTree(scope), children, new HashSet<string>(StringComparer.Ordinal));
    }

    public MapOrderNode? FindNode(string scope, string name) => findNode(GetTree(scope), name);

    public bool Move(string scope, string source, string? target, MapOrderDropPosition position)
    {
        IReadOnlyList<MapOrderNode> tree = GetTree(scope);
        MapOrderNode? node = findNode(tree, source);
        if (node is null || target is not null
            && (string.Equals(target, source, StringComparison.Ordinal) || findNode(node.Children, target) is not null))
            return false;
        IReadOnlyList<MapOrderNode>? result = insert(removeNode(tree, source), node, target, position);
        if (result is null)
            return false;
        setTree(scope, result);
        return true;
    }

    public void Transfer(string sourceScope, MapOrderNode subtree, string targetScope, string? target, MapOrderDropPosition position)
    {
        setTree(sourceScope, GetTree(sourceScope));
        HashSet<string> names = enumerateNames(subtree).ToHashSet(StringComparer.Ordinal);
        IReadOnlyList<MapOrderNode> tree = removeNames(GetTree(targetScope), names);
        setTree(targetScope, insert(tree, subtree, target, position) ?? [.. tree, subtree]);
    }

    public void RenameEntry(string scope, string oldName, string newName)
    {
        if (string.Equals(oldName, newName, StringComparison.Ordinal))
            return;
        IReadOnlyList<MapOrderNode> stored = getStoredTree(scope);
        IReadOnlyList<MapOrderNode> renamed = rename(stored, oldName, newName);
        if (!ReferenceEquals(stored, renamed))
            setTree(scope, renamed);
    }

    public void RenameScope(string oldScope, string newScope)
    {
        if (string.Equals(oldScope, newScope, StringComparison.Ordinal))
            return;
        if (trees.Remove(oldScope, out IReadOnlyList<MapOrderNode>? tree))
            trees[newScope] = tree;
        else
            trees.Remove(newScope);
        if (storedTexts.Remove(oldScope, out string? stored))
            storedTexts[newScope] = stored;
        else
            storedTexts.Remove(newScope);
        if (savedTexts.Remove(oldScope, out string? saved))
            savedTexts[newScope] = saved;
        else
            savedTexts.Remove(newScope);
        RenameEntry(string.Empty, oldScope, newScope);
        store.refreshDocumentStatus();
    }

    internal MapOrderState Capture() => new(
        new Dictionary<string, IReadOnlyList<MapOrderNode>>(trees, StringComparer.Ordinal),
        new Dictionary<string, string>(savedTexts, StringComparer.Ordinal));

    internal void Restore(MapOrderState state)
    {
        trees.Clear();
        storedTexts.Clear();
        savedTexts.Clear();
        foreach (KeyValuePair<string, IReadOnlyList<MapOrderNode>> tree in state.Trees)
        {
            trees[tree.Key] = tree.Value;
            storedTexts[tree.Key] = serialize(tree.Value);
        }
        foreach (KeyValuePair<string, string> saved in state.Saved)
            savedTexts[saved.Key] = saved.Value;
    }

    private IEnumerable<string> getActiveScopes() => store.Maps.MapCatalog
        .Where(entry => entry.Kind == MapCatalogEntryKind.WorldMap)
        .Select(entry => entry.Key).Prepend(string.Empty);

    private bool isScopeModified(string scope) => !string.Equals(
        storedTexts.GetValueOrDefault(scope, emptyText), savedTexts.GetValueOrDefault(scope, emptyText), StringComparison.Ordinal);

    private IReadOnlyList<MapOrderNode> getStoredTree(string scope) => trees.GetValueOrDefault(scope) ?? [];

    private void setTree(string scope, IReadOnlyList<MapOrderNode> tree)
    {
        trees[scope] = tree;
        storedTexts[scope] = serialize(tree);
        store.refreshDocumentStatus();
    }

    private static IReadOnlyList<MapOrderNode> reconcile(
        IReadOnlyList<MapOrderNode> stored, IReadOnlyList<string> names, IReadOnlySet<string> worlds)
    {
        HashSet<string> remaining = names.ToHashSet(StringComparer.Ordinal);
        List<MapOrderNode> lifted = [];
        List<MapOrderNode> visit(IReadOnlyList<MapOrderNode> nodes, bool topLevel)
        {
            List<MapOrderNode> result = [];
            foreach (MapOrderNode node in nodes)
            {
                if (!remaining.Remove(node.Name))
                {
                    result.AddRange(visit(node.Children, topLevel));
                    continue;
                }
                if (worlds.Contains(node.Name))
                {
                    (topLevel ? result : lifted).Add(new MapOrderNode(node.Name, []));
                    result.AddRange(visit(node.Children, topLevel));
                    continue;
                }
                result.Add(new MapOrderNode(node.Name, visit(node.Children, false)));
            }
            return result;
        }
        List<MapOrderNode> tree = visit(stored, true);
        tree.AddRange(lifted);
        tree.AddRange(names.Where(remaining.Contains).Order(StringComparer.Ordinal)
            .Select(name => new MapOrderNode(name, [])));
        return tree;
    }

    private static MapOrderNode? findNode(IReadOnlyList<MapOrderNode> nodes, string name)
    {
        foreach (MapOrderNode node in nodes)
        {
            if (string.Equals(node.Name, name, StringComparison.Ordinal))
                return node;
            if (findNode(node.Children, name) is MapOrderNode child)
                return child;
        }
        return null;
    }

    private static IEnumerable<string> enumerateNames(MapOrderNode node) =>
        node.Children.SelectMany(enumerateNames).Prepend(node.Name);

    private static IReadOnlyList<MapOrderNode> removeNode(IReadOnlyList<MapOrderNode> nodes, string name) => nodes
        .Where(node => !string.Equals(node.Name, name, StringComparison.Ordinal))
        .Select(node => node with { Children = removeNode(node.Children, name) }).ToArray();

    private static IReadOnlyList<MapOrderNode> removeNames(IReadOnlyList<MapOrderNode> nodes, IReadOnlySet<string> names)
    {
        List<MapOrderNode> result = [];
        foreach (MapOrderNode node in nodes)
        {
            IReadOnlyList<MapOrderNode> children = removeNames(node.Children, names);
            if (names.Contains(node.Name))
                result.AddRange(children);
            else
                result.Add(node with { Children = children });
        }
        return result;
    }

    private static IReadOnlyList<MapOrderNode>? insert(IReadOnlyList<MapOrderNode> nodes, MapOrderNode inserted,
        string? target, MapOrderDropPosition position)
    {
        if (target is null)
            return [.. nodes, inserted];
        for (int index = 0; index < nodes.Count; index++)
        {
            MapOrderNode node = nodes[index];
            List<MapOrderNode> result = [.. nodes];
            if (string.Equals(node.Name, target, StringComparison.Ordinal))
            {
                if (position == MapOrderDropPosition.Inside)
                    result[index] = node with { Children = [.. node.Children, inserted] };
                else
                    result.Insert(position == MapOrderDropPosition.Before ? index : index + 1, inserted);
                return result;
            }
            if (insert(node.Children, inserted, target, position) is IReadOnlyList<MapOrderNode> children)
            {
                result[index] = node with { Children = children };
                return result;
            }
        }
        return null;
    }

    private static IReadOnlyList<MapOrderNode> rename(IReadOnlyList<MapOrderNode> nodes, string oldName, string newName)
    {
        MapOrderNode[] result = new MapOrderNode[nodes.Count];
        bool changed = false;
        for (int index = 0; index < nodes.Count; index++)
        {
            MapOrderNode node = nodes[index];
            IReadOnlyList<MapOrderNode> children = rename(node.Children, oldName, newName);
            string name = string.Equals(node.Name, oldName, StringComparison.Ordinal) ? newName : node.Name;
            changed |= !ReferenceEquals(children, node.Children) || !ReferenceEquals(name, node.Name);
            result[index] = new MapOrderNode(name, children);
        }
        return changed ? result : nodes;
    }
}
