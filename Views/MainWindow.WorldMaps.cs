using Avalonia.Controls;
using Avalonia;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using AvaloniaEdit.Document;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Plugin.Abstractions;
using Ludork.Services;
using Ludork.Services.BlueprintAssistant;
using Ludork.Services.Plugins;
using Ludork.ViewModels;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class MainWindow
{
    private void onWorldPlacementChanged(
        object? sender,
        WorldMapPlacementChangedEventArgs args)
    {
        if (viewModel is null)
            return;
        WorldMapMutationResult result = viewModel.GameData.UpdateWorldPlacement(
            args.WorldKey,
            args.ChildMapKey,
            args.X,
            args.Y);
        if (!result.Success)
        {
            toast.ShowMessage(result.Details, 4000);
            refreshWorldMapPanel();
            return;
        }
        refreshWorldMapPanel();
    }

    private async void onWorldPlacementRemoved(
        object? sender,
        WorldMapPlacementRemovedEventArgs args)
    {
        if (viewModel is null)
            return;
        IReadOnlyList<ReferenceRecord> references = viewModel.ReferenceIndex.GetExternalMapReferences(
            [viewModel.GameData.GetMapRuntimePath(args.ChildMapKey)]);
        if (references.Count != 0)
        {
            await showMapReferenceBlockAsync(references);
            refreshWorldMapPanel();
            return;
        }
        WorldMapMutationResult result = viewModel.GameData.RemoveWorldPlacement(
            args.WorldKey,
            args.ChildMapKey);
        if (!result.Success)
            toast.ShowMessage(result.Details, 4000);
        refreshWorldMapPanel();
    }

    private void onWorldChildMapOpenRequested(object? sender, string mapKey)
    {
        if (viewModel is not null)
            viewModel.SelectedMap = viewModel.findMapItem(mapKey);
    }

    private void refreshMapPanel()
    {
        bool worldMode = viewModel?.SelectedMap is { IsWorld: true };
        MapEditToolbar.IsVisible = !worldMode;
        EditorScroll.IsVisible = !worldMode;
        WorldEditorPanel.IsVisible = worldMode;
        RightModePanel.IsVisible = !worldMode;
        UpperRightSplitter.IsVisible = !worldMode;
        if (worldMode)
        {
            EditorPanel.refreshMap(null, null);
            refreshWorldMapPanel();
        }
        else
        {
            WorldEditorPanel.SetWorld(null, null, Array.Empty<WorldMapChildSource>());
            EditorPanel.refreshMap(viewModel?.SelectedMap?.Key, viewModel?.SelectedMapData);
            selectPreviewMode(EditorPanel.EditMode);
        }
        refreshMapPanelState();
    }

    private void refreshWorldMapPanel()
    {
        if (viewModel?.SelectedMap is not { IsWorld: true } world)
            return;
        IReadOnlyList<WorldMapChildSource> children = viewModel.GameData.MapCatalog
            .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap
                && string.Equals(entry.WorldKey, world.Key, StringComparison.Ordinal))
            .OrderBy(entry => entry.Key, StringComparer.Ordinal)
            .Select(entry => new WorldMapChildSource(
                entry.Key,
                entry.DisplayName,
                entry.Width,
                entry.Height,
                () => viewModel.GameData.getMap(entry.Key),
                entry.LayerOrder,
                () => viewModel.GameData.LoadedMapData.ContainsKey(entry.Key),
                () => viewModel.GameData.ReadWorldChildMapSnapshotAsync(entry.Key),
                snapshot => viewModel.GameData.InstallWorldChildMapSnapshot(entry.Key, snapshot)))
            .ToArray();
        WorldEditorPanel.SetWorld(
            world.Key,
            viewModel.GameData.getWorldMap(world.Key),
            children);
    }

    private void onMapListPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (viewModel is null || !args.GetCurrentPoint(MapList).Properties.IsRightButtonPressed)
            return;
        TreeViewItem? item = getMapListItem(args.Source);
        MapListItemViewModel? map = item?.DataContext as MapListItemViewModel;
        if (map is not null)
            viewModel.SelectedMap = map;
        showMapContextMenu(map);
        args.Handled = true;
    }

    private async void onMapListDoubleTapped(object? sender, TappedEventArgs args)
    {
        TreeViewItem? item = getMapListItem(args.Source);
        if (item?.DataContext is not MapListItemViewModel map)
            return;
        if (map.IsMap)
            await editMapAsync(map.Key);
        args.Handled = true;
    }

    private async void onMapListKeyDown(object? sender, KeyEventArgs args)
    {
        if (viewModel is null)
            return;
        bool primary = EditorShortcuts.HasPrimaryModifier(args.KeyModifiers);
        if (primary
            && args.Key == Key.C
            && viewModel.SelectedMap is { IsMap: true } copyMapItem)
        {
            copyMap(copyMapItem.Key);
        }
        else if (primary && args.Key == Key.V)
            pasteMap();
        else if (args.Key == Key.Delete && viewModel.SelectedMap is MapListItemViewModel deleteMapItem)
        {
            if (deleteMapItem.IsWorld)
                await deleteWorldMapAsync(deleteMapItem.Key);
            else
                await deleteMapAsync(deleteMapItem.Key);
        }
        else if (args.Key is Key.Enter or Key.Return
            && viewModel.SelectedMap is MapListItemViewModel editMapItem)
        {
            if (editMapItem.IsWorld)
                await editWorldMapAsync(editMapItem.Key);
            else
                await editMapAsync(editMapItem.Key);
        }
        else
            return;
        args.Handled = true;
    }

    private void showMapContextMenu(MapListItemViewModel? map)
    {
        if (viewModel is null)
            return;
        ContextMenu menu = new();
        if (map is null)
        {
            MenuItem newMap = new() { Header = LocaleService.Get("NEW_MAP") };
            newMap.Click += async (_, _) => await createMapAsync();
            MenuItem newWorld = new() { Header = LocaleService.Get("NEW_WORLD_MAP") };
            newWorld.Click += async (_, _) => await createWorldMapAsync();
            MenuItem pasteItem = new() { Header = LocaleService.Get("PASTE"), IsEnabled = mapClipboard is not null };
            pasteItem.Click += (_, _) => pasteMap();
            menu.Items.Add(newMap);
            menu.Items.Add(newWorld);
            menu.Items.Add(pasteItem);
        }
        else if (map.IsWorld)
        {
            MenuItem newChild = new() { Header = LocaleService.Get("NEW_WORLD_CHILD_MAP") };
            newChild.Click += async (_, _) => await createWorldChildMapAsync(map.Key);
            MenuItem properties = new() { Header = LocaleService.Get("WORLD_MAP_PROPERTIES") };
            properties.Click += async (_, _) => await editWorldMapAsync(map.Key);
            MenuItem rename = new() { Header = LocaleService.Get("RENAME_FILE") };
            rename.Click += async (_, _) => await renameWorldMapAsync(map.Key);
            MenuItem delete = new() { Header = LocaleService.Get("DELETE") };
            delete.Click += async (_, _) => await deleteWorldMapAsync(map.Key);
            menu.Items.Add(newChild);
            menu.Items.Add(properties);
            menu.Items.Add(rename);
            menu.Items.Add(delete);
        }
        else
        {
            MenuItem editMap = new() { Header = LocaleService.Get("MAPLIST_EDIT") };
            editMap.Click += async (_, _) => await editMapAsync(map.Key);
            MenuItem copyItem = new() { Header = LocaleService.Get("COPY") };
            copyItem.Click += (_, _) => copyMap(map.Key);
            MenuItem deleteItem = new() { Header = LocaleService.Get("DELETE") };
            deleteItem.Click += async (_, _) => await deleteMapAsync(map.Key);
            menu.Items.Add(editMap);
            menu.Items.Add(copyItem);
            menu.Items.Add(deleteItem);
            if (Application.Current is App app)
            {
                app.appendPluginMapContextMenuCommands(
                    this,
                    menu,
                    map.Key,
                    createMapEditorHost(map.Key));
            }
        }
        menu.Open(MapList);
    }

    private async Task createMapAsync()
    {
        if (viewModel is null)
            return;
        MapInfo initial = new()
        {
            FileName = viewModel.GameData.getNewMapFileName(),
            MapName = LocaleService.Get("NEW_MAP_DEFAULT_NAME"),
            Width = 13,
            Height = 13,
        };
        MapInfo? result = await MapEditWindow.ShowAsync(this, viewModel.GameData, initial, string.Empty, true);
        if (result is not null && viewModel.GameData.CreateMap(result))
            viewModel.refreshMaps(normaliseMapKey(result.FileName));
    }

    private async Task createWorldMapAsync()
    {
        if (viewModel is null)
            return;
        WorldMapInfo initial = new()
        {
            DirectoryName = getNewWorldMapName(),
            Width = 256,
            Height = 192,
        };
        WorldMapInfo? result = await WorldMapEditWindow.ShowAsync(
            this,
            viewModel.GameData,
            initial,
            true);
        if (result is null)
            return;
        if (!viewModel.GameData.CreateWorldMap(result.DirectoryName, result))
        {
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("WORLD_CREATE_FAILED"));
            return;
        }
        viewModel.refreshMaps(result.DirectoryName);
    }

    private async Task createWorldChildMapAsync(string worldKey)
    {
        if (viewModel is null)
            return;
        string childName = getNewWorldChildMapName(worldKey);
        MapInfo initial = new()
        {
            FileName = childName + ".json",
            MapName = LocaleService.Get("NEW_MAP_DEFAULT_NAME"),
            Width = 13,
            Height = 13,
        };
        MapInfo? result = await MapEditWindow.ShowAsync(
            this,
            viewModel.GameData,
            initial,
            string.Empty,
            true,
            worldKey);
        if (result is null)
            return;
        result.FileName = Path.GetFileName(result.FileName);
        if (!viewModel.GameData.CreateWorldChildMap(worldKey, result))
        {
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("WORLD_CHILD_CREATE_FAILED"));
            return;
        }
        string key = worldKey + "/" + normaliseMapKey(result.FileName);
        viewModel.refreshMaps(key);
    }

    private async Task editWorldMapAsync(string worldKey)
    {
        if (viewModel?.GameData.getWorldMapInfo(worldKey) is not WorldMapInfo initial)
            return;
        WorldMapInfo? result = await WorldMapEditWindow.ShowAsync(
            this,
            viewModel.GameData,
            initial,
            false);
        if (result is null)
            return;
        WorldMapMutationResult update = viewModel.GameData.UpdateWorldMap(worldKey, result);
        if (!update.Success)
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), update.Details);
            return;
        }
        viewModel.refreshMaps(worldKey);
    }

    private async Task renameWorldMapAsync(string worldKey)
    {
        if (viewModel is null)
            return;
        IReadOnlyList<string> existing = viewModel.GameData.WorldMapData.Keys
            .Where(key => !string.Equals(key, worldKey, StringComparison.Ordinal))
            .Concat(viewModel.GameData.MapCatalog
                .Where(entry => entry.Kind == MapCatalogEntryKind.StandaloneMap)
                .Select(entry => entry.Key))
            .ToArray();
        string? result = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("RENAME_WORLD_MAP"),
            LocaleService.Get("WORLD_FOLDER_NAME_PROMPT"),
            existing,
            worldKey);
        if (result is null || string.Equals(result, worldKey, StringComparison.Ordinal))
            return;
        Dictionary<string, string> replacements = new(StringComparer.Ordinal)
        {
            [viewModel.GameData.GetWorldManifestRuntimePath(worldKey)] =
                viewModel.GameData.GetWorldManifestRuntimePath(result),
        };
        foreach (string childKey in viewModel.GameData.getWorldChildren(worldKey))
        {
            string childName = childKey[(worldKey.Length + 1)..];
            replacements[viewModel.GameData.GetMapRuntimePath(childKey)] =
                viewModel.GameData.GetMapRuntimePath(result + "/" + childName);
        }
        long historyGesture = viewModel.GameData.BeginHistoryGesture();
        bool renamed;
        try
        {
            renamed = viewModel.GameData.RenameWorldMap(worldKey, result);
            if (renamed)
                viewModel.ReferenceIndex.RewriteMapReferences(replacements);
        }
        finally
        {
            viewModel.GameData.EndHistoryGesture(historyGesture);
        }
        if (!renamed)
        {
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("WORLD_RENAME_FAILED"));
            return;
        }
        viewModel.refreshMaps(result);
    }

    private async Task deleteWorldMapAsync(string worldKey)
    {
        if (viewModel is null)
            return;
        IReadOnlyList<string> childKeys = viewModel.GameData.getWorldChildren(worldKey);
        IReadOnlyList<string> targetPaths = childKeys
            .Select(viewModel.GameData.GetMapRuntimePath)
            .Append(viewModel.GameData.GetWorldManifestRuntimePath(worldKey))
            .ToArray();
        IReadOnlyList<ReferenceRecord> references = viewModel.ReferenceIndex.GetExternalMapReferences(
            targetPaths,
            childKeys);
        if (references.Count != 0)
        {
            await showMapReferenceBlockAsync(references);
            return;
        }
        int childCount = childKeys.Count;
        string message = string.Format(
            LocaleService.Get("WORLD_DELETE_CONFIRMATION"),
            worldKey,
            childCount);
        if (!await ConfirmationDialog.ShowAsync(
                this,
                LocaleService.Get("CONFIRM_DELETE"),
                message))
        {
            return;
        }
        if (!viewModel.GameData.DeleteWorldMap(worldKey))
        {
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("DELETE_FAILED"));
            return;
        }
        viewModel.refreshMaps();
    }

    private async Task editMapAsync(string key)
    {
        if (viewModel?.GameData.getMapInfo(key) is not MapInfo initial)
            return;
        string? worldKey = viewModel.GameData.TryGetWorldForMap(key, out string parentWorld)
            ? parentWorld
            : null;
        if (worldKey is not null)
            initial.FileName = Path.GetFileName(initial.FileName);
        MapInfo? result = await MapEditWindow.ShowAsync(
            this,
            viewModel.GameData,
            initial,
            key,
            false,
            worldKey);
        if (result is null)
            return;
        if (worldKey is not null)
            result.FileName = worldKey + "/" + Path.GetFileName(result.FileName);
        string nextKey = normaliseMapKey(result.FileName);
        Dictionary<string, string> replacements = new(StringComparer.Ordinal);
        if (!string.Equals(key, nextKey, StringComparison.Ordinal))
        {
            replacements[viewModel.GameData.GetMapRuntimePath(key)] =
                viewModel.GameData.GetMapRuntimePath(nextKey);
        }
        long historyGesture = viewModel.GameData.BeginHistoryGesture();
        bool updated;
        try
        {
            updated = viewModel.GameData.UpdateMap(key, result);
            if (updated)
                viewModel.ReferenceIndex.RewriteMapReferences(replacements);
        }
        finally
        {
            viewModel.GameData.EndHistoryGesture(historyGesture);
        }
        if (!updated)
        {
            if (worldKey is not null)
            {
                await AlertDialog.ShowAsync(
                    this,
                    LocaleService.Get("ERROR"),
                    LocaleService.Get("WORLD_CHILD_UPDATE_FAILED"));
            }
            return;
        }
        viewModel.refreshMaps(nextKey);
    }

    private void copyMap(string key)
    {
        if (viewModel?.GameData.getMap(key) is JsonObject map)
            mapClipboard = new MapClipboard(key, (JsonObject)map.DeepClone());
    }

    private void pasteMap()
    {
        if (viewModel is null || mapClipboard is null)
            return;
        string? copyKey = viewModel.GameData.TryGetWorldForMap(mapClipboard.SourceKey, out _)
            ? viewModel.GameData.CopyWorldChildMap(mapClipboard.SourceKey)
            : viewModel.GameData.PasteMap(mapClipboard.Data, mapClipboard.SourceKey);
        if (copyKey is not null)
            viewModel.refreshMaps(copyKey);
    }

    private string getNewWorldMapName()
    {
        if (viewModel is null)
            return "World_01";
        for (int index = 1; ; index += 1)
        {
            string name = $"World_{index:D2}";
            if (!viewModel.GameData.WorldMapData.ContainsKey(name)
                && !viewModel.GameData.MapCatalog.Any(entry => string.Equals(entry.Key, name, StringComparison.Ordinal)))
            {
                return name;
            }
        }
    }

    private string getNewWorldChildMapName(string worldKey)
    {
        if (viewModel is null)
            return "Map_01";
        HashSet<string> childNames = viewModel.GameData.getWorldChildren(worldKey)
            .Select(Path.GetFileName)
            .Where(name => name is not null)
            .Select(name => name!)
            .ToHashSet(StringComparer.Ordinal);
        for (int index = 1; ; index += 1)
        {
            string name = $"Map_{index:D2}";
            if (!childNames.Contains(name))
                return name;
        }
    }

    private async Task deleteMapAsync(string key)
    {
        if (viewModel is null)
            return;
        IReadOnlyList<ReferenceRecord> references = viewModel.ReferenceIndex.GetExternalMapReferences(
            [viewModel.GameData.GetMapRuntimePath(key)],
            [key]);
        if (references.Count != 0)
        {
            await showMapReferenceBlockAsync(references);
            return;
        }
        if (!await ConfirmationDialog.ShowAsync(
                this,
                LocaleService.Get("CONFIRM_DELETE"),
                LocaleService.Get("DELETE_CONFIRMATION")))
        {
            return;
        }
        if (viewModel.GameData.DeleteMap(key))
            viewModel.refreshMaps();
    }

    private async Task showMapReferenceBlockAsync(IReadOnlyList<ReferenceRecord> references)
    {
        if (viewModel is null)
            return;
        string details = string.Join(
            Environment.NewLine,
            references.Select(reference =>
            {
                ReferenceNode? source = viewModel.ReferenceIndex.GetNode(reference.Source);
                string sourceName = source is null ? reference.Source : source.Type + ":" + source.Key;
                return sourceName + " · " + reference.Path;
            }));
        await AlertDialog.ShowAsync(
            this,
            LocaleService.Get("ERROR"),
            LocaleService.Get("MAP_TARGET_REFERENCED") + Environment.NewLine + details);
    }
}

