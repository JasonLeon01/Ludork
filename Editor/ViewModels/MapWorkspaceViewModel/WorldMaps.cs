using Ludork.Services;
using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels;

public sealed partial class MapWorkspaceViewModel
{
    private string? selectedDocumentKey;
    private MapDocumentSnapshot? selectedMapDocument;
    private readonly Dictionary<(string Section, string Key), MapListItemViewModel> mapItems = [];

    public MapDocumentSnapshot? SelectedMapDocument
    {
        get
        {
            if (SelectedMap is not { IsMap: true } map)
                return null;
            if (selectedMapDocument is null || !string.Equals(selectedDocumentKey, map.Key, StringComparison.Ordinal))
            {
                selectedDocumentKey = map.Key;
                selectedMapDocument = ReadSelectedMapDocument();
            }
            return selectedMapDocument;
        }
    }

    internal MapDocumentSnapshot? ReadSelectedMapDocument()
    {
        if (SelectedMap is not { IsMap: true } map)
            return null;
        return liveDebugSession is null ? GameData.Maps.ReadMapDocument(map.Key)
            : liveDebugSession.ReadMapDocument(map.Key);
    }

    private void onMapPreviewChanged(object? sender, MapPreviewChangedEventArgs args)
    {
        if (!args.ReloadData || args.MapKey is not null && !string.Equals(args.MapKey, SelectedMap?.Key, StringComparison.Ordinal))
            return;
        selectedMapDocument = null;
        selectedDocumentKey = null;
        if (args.Edit is null)
            refreshLayerTabs();
    }

    private void onDocumentContentChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (liveDebugSession is not null)
            return;
        if (args.Reset)
        {
            refreshMaps();
            return;
        }
        EditorDocumentChange[] changes = args.Changes
            .Where(change => change.Section is "Maps" or "WorldMaps").ToArray();
        if (changes.Length == 0)
            return;
        if (changes.Any(change => change.IdentityChanged))
        {
            string? selectedKey = SelectedMap?.Key;
            EditorDocumentChange? selectedChange = changes.FirstOrDefault(change => change.PreviousKey == selectedKey
                && (change.Section == "WorldMaps") == (SelectedMap?.IsWorld == true));
            refreshMaps(selectedChange?.Key ?? selectedKey);
            return;
        }
        foreach (EditorDocumentChange change in changes)
        {
            if (change.Key is null || !mapItems.TryGetValue((change.Section, change.Key), out MapListItemViewModel? item))
                continue;
            if (item.IsWorld)
                item.DisplayName = GameData.Worlds.ReadWorldMapSnapshot(item.Key)?["worldName"]?.GetValue<string>() ?? item.Key;
            else if (GameData.Maps.tryGetMapCatalogEntry(item.Key, out MapCatalogEntry entry))
                item.DisplayName = entry.DisplayName;
        }
        if (SelectedMap is { IsWorld: true } world
            && changes.Any(change => change.Section == "WorldMaps" && change.Key == world.Key))
            SelectedMapChanged?.Invoke(this, EventArgs.Empty);
    }

    public JsonObject? SelectedWorldMapData => SelectedMap is { IsWorld: true } world ? GameData.Worlds.ReadWorldMapSnapshot(world.Key) : null;
    public bool IsWorldMapSelected => SelectedMap?.IsWorld == true;

    public void refreshMaps(string? selectedMapKey = null)
    {
        selectedMapDocument = null;
        selectedDocumentKey = null;
        selectedMapKey ??= SelectedMap?.Key;
        string? selectedLayerName = SelectedLayerTab is { IsOverview: false } ? SelectedLayerTab.Name : null;
        HashSet<string> expandedWorlds = Maps
            .Where(item => item.IsWorld && item.IsExpanded)
            .Select(item => item.Key)
            .ToHashSet(StringComparer.Ordinal);
        rebuildMapTree(expandedWorlds);
        SelectedMap = findMapItem(selectedMapKey) ?? Maps.FirstOrDefault();
        refreshLayerTabs(selectedLayerName);
    }

    public MapListItemViewModel? findMapItem(string? key)
    {
        if (string.IsNullOrWhiteSpace(key))
            return null;
        foreach (MapListItemViewModel root in Maps)
        {
            if (string.Equals(root.Key, key, StringComparison.Ordinal))
                return root;
            MapListItemViewModel? child = root.Children.FirstOrDefault(
                item => string.Equals(item.Key, key, StringComparison.Ordinal));
            if (child is not null)
            {
                root.IsExpanded = true;
                return child;
            }
        }
        return null;
    }

    private void rebuildMapTree(IReadOnlySet<string>? expandedWorlds = null)
    {
        Maps.Clear();
        mapItems.Clear();
        IReadOnlyList<MapCatalogEntry> catalog = GameData.Maps.MapCatalog;
        ILookup<string?, MapCatalogEntry> children = catalog
            .Where(item => item.Kind == MapCatalogEntryKind.WorldChildMap)
            .ToLookup(item => item.WorldKey, StringComparer.Ordinal);
        foreach (MapCatalogEntry entry in catalog
            .Where(item => item.Kind is MapCatalogEntryKind.StandaloneMap or MapCatalogEntryKind.WorldMap)
            .OrderBy(item => item.Key, StringComparer.Ordinal))
        {
            MapListItemViewModel root = new(
                entry.Key,
                entry.DisplayName,
                entry.Kind,
                entry.WorldKey);
            root.IsExpanded = expandedWorlds?.Contains(entry.Key) == true;
            if (entry.Kind == MapCatalogEntryKind.WorldMap)
            {
                foreach (MapCatalogEntry child in children[entry.Key]
                    .OrderBy(item => item.Key, StringComparer.Ordinal))
                {
                    root.Children.Add(new MapListItemViewModel(
                        child.Key,
                        child.DisplayName,
                        child.Kind,
                        child.WorldKey));
                }
            }
            Maps.Add(root);
            foreach (MapListItemViewModel item in root.Children.Prepend(root))
            {
                string section = item.IsWorld ? "WorldMaps" : "Maps";
                mapItems[(section, item.Key)] = item;
                item.IsModified = GameData.Documents.Find(section, item.Key)?.IsModified == true;
            }
        }
    }
}
