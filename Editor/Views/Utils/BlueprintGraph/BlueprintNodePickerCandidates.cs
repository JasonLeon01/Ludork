using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed class BlueprintNodePickerCandidates
{
    private readonly IReadOnlyList<BlueprintNodePickerEntry> entries;
    private readonly IReadOnlyList<TreeItem> roots;
    private readonly int[]? targetPins;
    private IReadOnlyList<BlueprintNodePickerSelection>? selections;
    private BlueprintNodePickerSelection?[]? selectionsByEntry;
    private BlueprintNodePickerRow?[]? searchRows;
    private HashSet<TreeItem>? visibleGroups;
    private HashSet<TreeItem>? contextVisibleGroups;

    internal BlueprintNodePickerCandidates(IReadOnlyList<BlueprintNodePickerEntry> entries)
    {
        this.entries = entries;
        roots = buildTree(entries);
        Count = entries.Count;
    }

    private BlueprintNodePickerCandidates(BlueprintNodePickerCandidates source, int[] targetPins, int count)
    {
        entries = source.entries;
        roots = source.roots;
        this.targetPins = targetPins;
        Count = count;
    }

    public int Count { get; }

    public IReadOnlyList<BlueprintNodePickerSelection> Selections
    {
        get
        {
            if (selections is not null)
                return selections;
            List<BlueprintNodePickerSelection> result = new(Count);
            for (int index = 0; index < entries.Count; index++)
            {
                if (isVisible(index, false))
                    result.Add(getSelection(index));
            }
            selections = result;
            return selections;
        }
    }

    internal BlueprintNodePickerCandidates Filter(int[] pins, int count)
    {
        return new BlueprintNodePickerCandidates(this, pins, count);
    }

    internal IReadOnlyList<BlueprintNodePickerRow> GetRows(
        string search,
        bool contextSensitive,
        IReadOnlySet<string> expandedGroups)
    {
        List<BlueprintNodePickerRow> rows = [];
        if (search.Length != 0)
        {
            searchRows ??= new BlueprintNodePickerRow?[entries.Count];
            for (int index = 0; index < entries.Count; index++)
            {
                BlueprintNodePickerEntry entry = entries[index];
                if (isVisible(index, contextSensitive)
                    && (entry.Definition.MemberName.Contains(search, StringComparison.OrdinalIgnoreCase)
                        || entry.Title.Contains(search, StringComparison.OrdinalIgnoreCase)))
                {
                    searchRows[index] ??= new BlueprintNodePickerRow(entry.SearchTitle, 0, getSelection(index));
                    rows.Add(searchRows[index]!);
                }
            }
            return rows;
        }

        HashSet<TreeItem> groups = contextSensitive
            ? contextVisibleGroups ??= getVisibleGroups(true)
            : visibleGroups ??= getVisibleGroups(false);
        foreach (TreeItem root in roots)
            appendRows(root, 0, contextSensitive, groups, expandedGroups, rows);
        return rows;
    }

    private bool isVisible(int index, bool contextSensitive)
    {
        return (targetPins is null || targetPins[index] >= 0)
            && (!contextSensitive || entries[index].Definition.IsContextRelevant);
    }

    private BlueprintNodePickerSelection getSelection(int index)
    {
        selectionsByEntry ??= new BlueprintNodePickerSelection?[entries.Count];
        return selectionsByEntry[index] ??= new BlueprintNodePickerSelection(
            entries[index].Definition, targetPins?[index]);
    }

    private HashSet<TreeItem> getVisibleGroups(bool contextSensitive)
    {
        HashSet<TreeItem> result = [];
        foreach (TreeItem root in roots)
            addVisibleGroups(root, contextSensitive, result);
        return result;
    }

    private bool addVisibleGroups(TreeItem item, bool contextSensitive, ISet<TreeItem> groups)
    {
        if (item.EntryIndex >= 0)
            return isVisible(item.EntryIndex, contextSensitive);
        bool visible = false;
        foreach (TreeItem child in item.Children)
            visible |= addVisibleGroups(child, contextSensitive, groups);
        if (visible)
            groups.Add(item);
        return visible;
    }

    private static IReadOnlyList<TreeItem> buildTree(IReadOnlyList<BlueprintNodePickerEntry> entries)
    {
        TreeBuilder root = new(string.Empty, string.Empty);
        for (int index = 0; index < entries.Count; index++)
        {
            BlueprintNodePickerEntry entry = entries[index];
            TreeBuilder parent = root;
            string path = string.Empty;
            foreach (string segment in entry.GroupPath)
            {
                path = path.Length == 0 ? segment : path + "." + segment;
                if (!parent.Groups.TryGetValue(segment, out TreeBuilder? child))
                {
                    child = new TreeBuilder(EditorDisplayName.Format(segment), path);
                    parent.Groups.Add(segment, child);
                }
                parent = child;
            }
            parent.Leaves.Add(new TreeItem(entry.Title, string.Empty, index, []));
        }
        return freezeTree(root);
    }

    private static IReadOnlyList<TreeItem> freezeTree(TreeBuilder builder)
    {
        IEnumerable<TreeItem> groups = builder.Groups.Values
            .OrderBy(group => group.Name, StringComparer.Ordinal)
            .Select(group => new TreeItem(group.Name, group.Path, -1, freezeTree(group)));
        return groups.Concat(builder.Leaves).ToArray();
    }

    private void appendRows(
        TreeItem item,
        int depth,
        bool contextSensitive,
        IReadOnlySet<TreeItem> visible,
        IReadOnlySet<string> expandedGroups,
        ICollection<BlueprintNodePickerRow> rows)
    {
        if (item.EntryIndex >= 0)
        {
            if (isVisible(item.EntryIndex, contextSensitive))
                rows.Add(new BlueprintNodePickerRow(item.Name, depth, getSelection(item.EntryIndex)));
            return;
        }
        if (!visible.Contains(item))
            return;
        bool expanded = expandedGroups.Contains(item.Path);
        rows.Add(new BlueprintNodePickerRow(item.Name, depth, GroupPath: item.Path, IsExpanded: expanded));
        if (!expanded)
            return;
        foreach (TreeItem child in item.Children)
            appendRows(child, depth + 1, contextSensitive, visible, expandedGroups, rows);
    }

    private sealed record TreeItem(
        string Name,
        string Path,
        int EntryIndex,
        IReadOnlyList<TreeItem> Children);

    private sealed class TreeBuilder
    {
        public TreeBuilder(string name, string path)
        {
            Name = name;
            Path = path;
        }

        public string Name { get; }
        public string Path { get; }
        public Dictionary<string, TreeBuilder> Groups { get; } = new(StringComparer.Ordinal);
        public List<TreeItem> Leaves { get; } = [];
    }
}
