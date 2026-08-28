using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels;

public partial class MainViewModel
{
    public JsonObject? SelectedMapData => SelectedMap is { IsMap: true } map ? GameData.getMap(map.Key) : null;
    public JsonObject? SelectedWorldMapData => SelectedMap is { IsWorld: true } world ? GameData.getWorldMap(world.Key) : null;
    public bool IsWorldMapSelected => SelectedMap?.IsWorld == true;

    public void refreshMaps(string? selectedMapKey = null)
    {
        selectedMapKey ??= SelectedMap?.Key;
        HashSet<string> expandedWorlds = Maps
            .Where(item => item.IsWorld && item.IsExpanded)
            .Select(item => item.Key)
            .ToHashSet(StringComparer.Ordinal);
        rebuildMapTree(expandedWorlds);
        SelectedMap = findMapItem(selectedMapKey) ?? Maps.FirstOrDefault();
        refreshLayerTabs();
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
        IReadOnlyList<MapCatalogEntry> catalog = GameData.MapCatalog;
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
                foreach (MapCatalogEntry child in catalog
                    .Where(item => item.Kind == MapCatalogEntryKind.WorldChildMap
                        && string.Equals(item.WorldKey, entry.Key, StringComparison.Ordinal))
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
        }
    }
}

public sealed class MapListItemViewModel : ViewModelBase
{
    private bool isExpanded;

    public MapListItemViewModel(
        string key,
        string displayName,
        MapCatalogEntryKind kind,
        string? worldKey)
    {
        Key = key;
        DisplayName = displayName;
        Kind = kind;
        WorldKey = worldKey;
    }

    public string Key { get; }
    public string DisplayName { get; }
    public MapCatalogEntryKind Kind { get; }
    public string? WorldKey { get; }
    public bool IsWorld => Kind == MapCatalogEntryKind.WorldMap;
    public bool IsMap => Kind is MapCatalogEntryKind.StandaloneMap or MapCatalogEntryKind.WorldChildMap;
    public bool IsWorldChild => Kind == MapCatalogEntryKind.WorldChildMap;
    public ObservableCollection<MapListItemViewModel> Children { get; } = [];
    public bool IsExpanded
    {
        get => isExpanded;
        set => SetProperty(ref isExpanded, value);
    }
}
