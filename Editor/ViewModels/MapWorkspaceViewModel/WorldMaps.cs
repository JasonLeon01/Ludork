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
        if (liveDebugSession is null && args.Edit is not null && selectedMapDocument is not null
            && string.Equals(selectedDocumentKey, args.MapKey, StringComparison.Ordinal))
        {
            MapDocumentCodec.ApplyEdits(selectedMapDocument, args.Edit);
            return;
        }
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
            foreach (EditorDocumentChange change in changes.Where(change => change.Section == "WorldMaps"
                && change.IdentityChanged && change.PreviousKey is not null && change.Key is not null))
                GameData.MapOrder.RenameScope(change.PreviousKey!, change.Key!);
            foreach (EditorDocumentChange change in changes.Where(change => change.Section == "Maps"
                && change.IdentityChanged && change.PreviousKey is not null && change.Key is not null))
            {
                string scope = MapOrderService.GetScope(change.PreviousKey!);
                if (string.Equals(scope, MapOrderService.GetScope(change.Key!), StringComparison.Ordinal))
                    GameData.MapOrder.RenameEntry(scope, MapOrderService.GetName(change.PreviousKey!),
                        MapOrderService.GetName(change.Key!));
            }
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
        rebuildMapTree(getExpandedMapKeys().ToHashSet(StringComparer.Ordinal));
        SelectedMap = findMapItem(selectedMapKey) ?? Maps.FirstOrDefault();
        refreshLayerTabs(selectedLayerName);
    }

    public MapListItemViewModel? findMapItem(string? key)
    {
        if (string.IsNullOrWhiteSpace(key))
            return null;
        return findMapItem(Maps, key);
    }

    private static MapListItemViewModel? findMapItem(IEnumerable<MapListItemViewModel> items, string key)
    {
        foreach (MapListItemViewModel item in items)
        {
            if (string.Equals(item.Key, key, StringComparison.Ordinal))
                return item;
            if (findMapItem(item.Children, key) is MapListItemViewModel child)
            {
                item.IsExpanded = true;
                return child;
            }
        }
        return null;
    }

    private static IEnumerable<MapListItemViewModel> enumerateMapItems(IEnumerable<MapListItemViewModel> items) =>
        items.SelectMany(item => enumerateMapItems(item.Children).Prepend(item));

    private static string getMapSection(MapListItemViewModel item) => item.IsWorld ? "WorldMaps" : "Maps";

    private IEnumerable<string> getExpandedMapKeys() => enumerateMapItems(Maps)
        .Where(item => item.IsExpanded && item.Children.Count != 0)
        .Select(item => item.Key);

    public void SaveMapListExpansion() => ProjectConfig.MapListExpandedKeys = getExpandedMapKeys().ToArray();

    private void rebuildMapTree(IReadOnlySet<string>? expanded = null)
    {
        Maps.Clear();
        mapItems.Clear();
        Dictionary<string, MapCatalogEntry> catalog = GameData.Maps.MapCatalog
            .ToDictionary(entry => entry.Key, StringComparer.Ordinal);
        MapListItemViewModel createItem(string scope, MapOrderNode node)
        {
            MapCatalogEntry entry = catalog[MapOrderService.GetKey(scope, node.Name)];
            MapListItemViewModel item = new(entry.Key, entry.DisplayName, entry.Kind, entry.WorldKey);
            string section = getMapSection(item);
            item.IsExpanded = expanded?.Contains(item.Key) == true;
            item.IsModified = GameData.Documents.Find(section, item.Key)?.IsModified == true;
            mapItems[(section, item.Key)] = item;
            IReadOnlyList<MapOrderNode> children = item.IsWorld ? GameData.MapOrder.GetTree(item.Key) : node.Children;
            foreach (MapOrderNode child in children)
                item.Children.Add(createItem(item.IsWorld ? item.Key : scope, child));
            return item;
        }
        foreach (MapOrderNode node in GameData.MapOrder.GetTree(string.Empty))
            Maps.Add(createItem(string.Empty, node));
    }
}
