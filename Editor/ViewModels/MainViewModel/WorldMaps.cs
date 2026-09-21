using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels;

public partial class MainViewModel
{
    private string? selectedSnapshotKey;
    private JsonObject? selectedMapSnapshot;

    public JsonObject? SelectedMapData
    {
        get
        {
            if (SelectedMap is not { IsMap: true } map)
                return null;
            if (selectedMapSnapshot is null || !string.Equals(selectedSnapshotKey, map.Key, StringComparison.Ordinal))
            {
                selectedSnapshotKey = map.Key;
                selectedMapSnapshot = liveDebugSession is null
                    ? GameData.ReadMapSnapshot(map.Key) : liveDebugSession.ReadMapSnapshot(map.Key);
            }
            return selectedMapSnapshot;
        }
    }

    private void onMapPreviewChanged(object? sender, MapPreviewChangedEventArgs args)
    {
        if (!args.ReloadData || args.MapKey is not null && !string.Equals(args.MapKey, selectedSnapshotKey, StringComparison.Ordinal))
            return;
        if (args.Edit is not null && selectedMapSnapshot is not null)
            args.Edit.ApplyTo(selectedMapSnapshot);
        else
        {
            selectedSnapshotKey = null;
            selectedMapSnapshot = null;
        }
    }

    public JsonObject? SelectedWorldMapData => SelectedMap is { IsWorld: true } world ? GameData.ReadWorldMapSnapshot(world.Key) : null;
    public bool IsWorldMapSelected => SelectedMap?.IsWorld == true;

    public void refreshMaps(string? selectedMapKey = null)
    {
        selectedSnapshotKey = null;
        selectedMapSnapshot = null;
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
            foreach (MapListItemViewModel item in root.Children.Prepend(root))
                item.IsModified = GameData.Documents.Find(item.IsWorld ? "WorldMaps" : "Maps", item.Key)?.IsModified == true;
        }
    }
}
