using System.Collections.Generic;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels;

public sealed partial class MapWorkspaceViewModel : ViewModelBase, IDisposable
{
    private readonly ProjectDataStore GameData;
    private readonly ProjectConfigService ProjectConfig;
    private readonly TileSelectViewModel TileSelect;
    private readonly ReferenceIndexService references;
    private bool canEdit = true;
    private bool disposed;
    private MapListItemViewModel? selectedMap;
    private LayerTabViewModel? selectedLayerTab;
    private JsonObject? copiedLayer;
    private string? copiedLayerName;
    private (string? MapKey, LiveDebugSession? Session, string? Context) actorOutlinerSource;
    public event EventHandler? SelectedMapChanged;
    public event EventHandler? ActorOutlinerChanged;
    public event EventHandler? LayerDisplayStateChanged;
    public ObservableCollection<MapListItemViewModel> Maps { get; } = [];
    public ObservableCollection<LayerTabViewModel> LayerTabs { get; } = [];
    public ObservableCollection<ActorOutlinerItemViewModel> ActorOutlinerItems { get; } = [];

    public MapWorkspaceViewModel(ProjectDataStore gameData, ProjectConfigService projectConfig, TileSelectViewModel tileSelect, ReferenceIndexService references)
    {
        GameData = gameData;
        ProjectConfig = projectConfig;
        TileSelect = tileSelect;
        this.references = references;
        TileSelect.TilesetSelected += onTilesetSelected;
        GameData.Maps.MapPreviewChanged += onMapPreviewChanged;
        GameData.Documents.Changed += onDocumentsChanged;
        rebuildMapTree();
        SelectedMap = findMapItem(ProjectConfig.LastOpenedMapKey) ?? Maps.FirstOrDefault();
    }

    public bool CanEdit
    {
        get => canEdit;
        set
        {
            if (!SetProperty(ref canEdit, value))
                return;
            RefreshConfiguration();
            OnPropertyChanged(nameof(CanUseMapTools));
        }
    }

    public void RefreshConfiguration()
    {
        OnPropertyChanged(nameof(LiveDebug));
        OnPropertyChanged(nameof(CanConfigureLiveDebug));
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        TileSelect.TilesetSelected -= onTilesetSelected;
        GameData.Maps.MapPreviewChanged -= onMapPreviewChanged;
        GameData.Documents.Changed -= onDocumentsChanged;
        if (liveDebugSession is not null)
            liveDebugSession.Changed -= onRuntimeMapChanged;
        liveDebugSession = null;
    }

    private void onDocumentsChanged(object? sender, EventArgs args)
    {
        foreach (MapListItemViewModel item in Maps.SelectMany(root => root.Children.Prepend(root)))
            item.IsModified = GameData.Documents.Find(item.IsWorld ? "WorldMaps" : "Maps", item.Key)?.IsModified == true;
    }

    public MapListItemViewModel? SelectedMap
    {
        get => selectedMap;
        set
        {
            if (liveDebugSession is not null && !selectingRuntimeMap)
                return;
            if (!SetProperty(ref selectedMap, value))
                return;
            if (liveDebugSession is null && !selectingRuntimeMap)
                ProjectConfig.LastOpenedMapKey = value?.Key;
            refreshLayerTabs();
            IReadOnlyCollection<string> pinnedMaps = value is { IsWorldChild: true }
                ? new[] { value.Key }
                : Array.Empty<string>();
            if (liveDebugSession is null)
                GameData.Maps.TrimWorldChildCache(pinnedMaps);
            SelectedMapChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    public LayerTabViewModel? SelectedLayerTab
    {
        get => selectedLayerTab;
        set
        {
            if (!SetProperty(ref selectedLayerTab, value))
                return;
            bool hasLayer = value is not null && !value.IsOverview && SelectedMap is not null;
            TileSelect.IsLayerSelected = hasLayer;
            if (hasLayer && liveDebugSession is null)
                TileSelect.setCurrentTilesetKey(GameData.Maps.getLayerTilesetKey(SelectedMap!.Key, value!.Name));
            else if (hasLayer)
                restoreRuntimeTileset();
            LayerDisplayStateChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    public bool IsSelectedLayerEditable => SelectedLayerTab is
        {
            IsOverview: false,
            LayerVisible: true,
        };

    public bool canEditLayer(string mapKey, string layerName)
    {
        MapDocumentSnapshot? map = SelectedMap?.Key == mapKey ? SelectedMapDocument : GameData.Maps.ReadMapDocument(mapKey);
        return map?.Layers.GetValueOrDefault(layerName)?.Visible == true;
    }

    public bool moveLayer(LayerTabViewModel moving, LayerTabViewModel target)
    {
        if (!CanEdit)
            return false;
        if (SelectedMap is null || moving.IsOverview || target.IsOverview || moving == target)
            return false;
        if (!GameData.Maps.reorderLayers(SelectedMap.Key, moving.Name, target.Name))
            return false;
        int fromIndex = LayerTabs.IndexOf(moving);
        int targetIndex = LayerTabs.IndexOf(target);
        LayerTabs.Move(fromIndex, targetIndex);
        SelectedLayerTab = moving;
        refreshActorOutliner();
        return true;
    }

    public bool addLayer(string name, string? insertAfterLayer)
    {
        if (!CanEdit)
            return false;
        if (SelectedMap is null || !GameData.Maps.addEmptyLayer(SelectedMap.Key, name, insertAfterLayer))
            return false;
        refreshLayerTabs(name);
        return true;
    }

    public bool renameLayer(string oldName, string newName)
    {
        if (!CanEdit)
            return false;
        if (SelectedMap is null || !GameData.Maps.renameLayer(SelectedMap.Key, oldName, newName))
            return false;
        refreshLayerTabs(newName);
        return true;
    }

    public bool deleteLayer(string layerName)
    {
        if (!CanEdit)
            return false;
        if (SelectedMap is null || !GameData.Maps.removeLayer(SelectedMap.Key, layerName))
            return false;
        refreshLayerTabs();
        return true;
    }

    public bool copyLayer(string layerName)
    {
        if (!CanEdit)
            return false;
        if (SelectedMap is null || GameData.Maps.copyLayer(SelectedMap.Key, layerName) is not { } copy)
            return false;
        copiedLayer = copy;
        copiedLayerName = layerName;
        OnPropertyChanged(nameof(CanPasteLayer));
        return true;
    }

    public bool pasteLayer(string insertAfterLayer)
    {
        if (!CanEdit)
            return false;
        if (SelectedMap is null || copiedLayer is null || string.IsNullOrWhiteSpace(copiedLayerName))
            return false;
        string name = getUniqueLayerName($"{copiedLayerName}_copy");
        if (!GameData.Maps.pasteLayer(SelectedMap.Key, name, copiedLayer, insertAfterLayer))
            return false;
        refreshLayerTabs(name);
        return true;
    }

    public bool CanPasteLayer => copiedLayer is not null;

    public bool setLayerVisible(LayerTabViewModel layer, bool visible)
    {
        if (!CanEdit)
            return false;
        if (SelectedMap is null || layer.IsOverview || layer.LayerVisible == visible)
            return false;
        if (!GameData.Maps.SetLayerVisible(SelectedMap.Key, layer.Name, visible))
            return false;
        layer.LayerVisible = visible;
        updateLayerEditability();
        return true;
    }

    public string getLayerShaderPath(string layerName)
    {
        return SelectedMap is null ? string.Empty : GameData.Maps.getLayerShaderPath(SelectedMap.Key, layerName);
    }

    public bool setLayerShaderPath(string layerName, string shaderPath)
    {
        if (!CanEdit)
            return false;
        return SelectedMap is not null && GameData.Maps.setLayerShaderPath(SelectedMap.Key, layerName, shaderPath);
    }

    public bool layerNameExists(string name, string? except = null)
    {
        return LayerTabs.Any(item => !item.IsOverview && item.Name == name && item.Name != except);
    }

    public void refreshActorOutliner()
    {
        IsRefreshingActorOutliner = true;
        (string? MapKey, LiveDebugSession? Session, string? Context) source =
            (SelectedMap?.Key, liveDebugSession, liveDebugSession?.Context);
        if (actorOutlinerSource != source)
        {
            ActorOutlinerItems.Clear();
            actorOutlinerSource = source;
        }
        ActorOutlinerItemViewModel[] previousItems = ActorOutlinerItems
            .SelectMany(layer => layer.EnumerateDescendants().Prepend(layer)).ToArray();
        Dictionary<string, ActorOutlinerItemViewModel> runtimeActors = previousItems
            .Where(actor => actor.RuntimeId is not null)
            .ToDictionary(actor => actor.RuntimeId!, StringComparer.Ordinal);
        Dictionary<string, ActorOutlinerItemViewModel> currentActors = new(StringComparer.Ordinal);
        List<ActorOutlinerItemViewModel> layers = [];
        Dictionary<ActorOutlinerItemViewModel, List<ActorOutlinerItemViewModel>> childrenToKeep = [];
        List<(ActorOutlinerItemViewModel Item, ActorOutlinerItemViewModel Layer, string? ParentId)> actorEntries = [];
        IReadOnlyDictionary<string, IReadOnlyList<MapActorSnapshot>>? actorGroups = SelectedMapDocument?.Actors;
        foreach (LayerTabViewModel layer in LayerTabs)
        {
            if (layer.IsOverview)
                continue;
            ActorOutlinerItemViewModel layerItem = ActorOutlinerItems.FirstOrDefault(item => item.LayerName == layer.Name)
                ?? new(layer.Name, layer.Name, layer.Name, null, []);
            layers.Add(layerItem);
            childrenToKeep[layerItem] = [];
            if (actorGroups?.GetValueOrDefault(layer.Name) is IReadOnlyList<MapActorSnapshot> layerActors)
            {
                for (int index = 0; index < layerActors.Count; index += 1)
                {
                    MapActorSnapshot actor = layerActors[index];
                    string tag = actor.Tag;
                    string reference = actor.Blueprint;
                    string? runtimeId = actor.RuntimeId;
                    string name = !string.IsNullOrWhiteSpace(tag)
                        ? tag
                        : !string.IsNullOrWhiteSpace(reference)
                            ? reference
                            : $"#{index + 1}";
                    ActorOutlinerItemViewModel? item = runtimeId is not null
                        ? runtimeActors.GetValueOrDefault(runtimeId)
                        : layerItem.Children.FirstOrDefault(child => child.ActorIndex == index);
                    item ??= new(name, reference, layer.Name, index, [], runtimeId);
                    item.Update(name, reference, layer.Name, index);
                    childrenToKeep[item] = [];
                    if (runtimeId is not null)
                        currentActors[runtimeId] = item;
                    actorEntries.Add((item, layerItem, actor.ParentRuntimeId));
                }
            }
        }
        foreach ((ActorOutlinerItemViewModel item, ActorOutlinerItemViewModel layer, string? parentId) in actorEntries)
        {
            ActorOutlinerItemViewModel parent = liveDebugSession is not null && parentId is not null
                && currentActors.TryGetValue(parentId, out ActorOutlinerItemViewModel? owner) && owner != item
                ? owner : layer;
            childrenToKeep[parent].Add(item);
        }
        if (liveDebugSession is not null)
            foreach (ActorOutlinerItemViewModel parent in childrenToKeep.Keys.ToArray())
            {
                List<ActorOutlinerItemViewModel> children = childrenToKeep[parent];
                childrenToKeep[parent] = parent.Children.Where(children.Contains).Concat(children.Except(parent.Children)).ToList();
            }
        foreach (ActorOutlinerItemViewModel parent in previousItems)
            foreach (ActorOutlinerItemViewModel child in parent.Children.ToArray())
                if (!childrenToKeep.TryGetValue(parent, out List<ActorOutlinerItemViewModel>? kept) || !kept.Contains(child))
                    parent.Children.Remove(child);
        reconcileOutlinerItems(ActorOutlinerItems, layers);
        foreach ((ActorOutlinerItemViewModel parent, List<ActorOutlinerItemViewModel> children) in childrenToKeep)
            reconcileOutlinerItems(parent.Children, children);
        IsRefreshingActorOutliner = false;
        ActorOutlinerChanged?.Invoke(this, EventArgs.Empty);
    }

    public bool IsRefreshingActorOutliner { get; private set; }

    private static void reconcileOutlinerItems(
        ObservableCollection<ActorOutlinerItemViewModel> items, IReadOnlyList<ActorOutlinerItemViewModel> expected)
    {
        for (int index = items.Count - 1; index >= 0; index--)
            if (!expected.Contains(items[index]))
                items.RemoveAt(index);
        for (int index = 0; index < expected.Count; index++)
        {
            ActorOutlinerItemViewModel item = expected[index];
            int previous = items.IndexOf(item);
            if (previous < 0)
                items.Insert(index, item);
            else if (previous != index)
                items.Move(previous, index);
        }
    }

    private void refreshLayerTabs(string? preferredLayerName = null)
    {
        string? previousName = preferredLayerName ?? (SelectedLayerTab is { IsOverview: false } ? SelectedLayerTab.Name : null);
        LayerTabs.Clear();
        if (SelectedMap is not { IsMap: true })
        {
            SelectedLayerTab = null;
            refreshActorOutliner();
            updateLayerEditability();
            return;
        }
        LayerTabs.Add(new LayerTabViewModel(LocaleService.Get("OVERVIEW"), true, true));
        if (SelectedMap is not null)
        {
            foreach (string name in displayedLayerNames())
            {
                bool visible = SelectedMapDocument?.Layers.GetValueOrDefault(name)?.Visible ?? true;
                LayerTabs.Add(new LayerTabViewModel(
                    name,
                    false,
                    visible));
            }
        }
        SelectedLayerTab = previousName is null
            ? LayerTabs[0]
            : LayerTabs.FirstOrDefault(item => !item.IsOverview && item.Name == previousName) ?? LayerTabs[0];
        refreshActorOutliner();
        updateLayerEditability();
    }

    private void updateLayerEditability()
    {
        LayerDisplayStateChanged?.Invoke(this, EventArgs.Empty);
    }

    private string getUniqueLayerName(string baseName)
    {
        string candidate = baseName;
        int suffix = 2;
        while (layerNameExists(candidate))
            candidate = $"{baseName}_{suffix++}";
        return candidate;
    }

    private void onTilesetSelected(object? sender, string tilesetKey)
    {
        if (!CanEdit)
        {
            restoreRuntimeTileset();
            return;
        }
        if (SelectedMap is null || SelectedLayerTab is not { IsOverview: false } layer)
            return;
        if (GameData.Maps.setLayerTilesetKey(SelectedMap.Key, layer.Name, tilesetKey))
            LayerDisplayStateChanged?.Invoke(this, EventArgs.Empty);
    }

}
