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
        WorldMapMutationResult result = viewModel.MapWorkspace.UpdateWorldPlacement(
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
        IReadOnlyList<ReferenceRecord> references = viewModel.MapWorkspace.PlacementRemoveReferences(args.ChildMapKey);
        if (references.Count != 0)
        {
            await showMapReferenceBlockAsync(references);
            refreshWorldMapPanel();
            return;
        }
        WorldMapMutationResult result = viewModel.MapWorkspace.RemoveWorldPlacement(
            args.WorldKey,
            args.ChildMapKey);
        if (!result.Success)
            toast.ShowMessage(result.Details, 4000);
        refreshWorldMapPanel();
    }

    private void onWorldChildMapOpenRequested(object? sender, string mapKey)
    {
        if (viewModel?.CanEdit != true)
            return;
        if (viewModel is not null)
            viewModel.MapWorkspace.SelectedMap = viewModel.MapWorkspace.findMapItem(mapKey);
    }

    private void refreshMapPanel()
    {
        bool worldMode = viewModel?.MapWorkspace.SelectedMap is { IsWorld: true };
        LayerTabsScroll.IsVisible = !worldMode;
        EditModeToggles.IsVisible = !worldMode;
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
            EditorPanel.refreshMap(viewModel?.MapWorkspace.SelectedMap?.Key, viewModel?.MapWorkspace.SelectedMapData);
            selectPreviewMode(EditorPanel.EditMode);
        }
        updateLightModeControls();
        refreshMapPanelState();
    }

    private void refreshWorldMapPanel()
    {
        if (viewModel?.MapWorkspace.SelectedMap is not { IsWorld: true } world)
            return;
        IReadOnlyList<WorldMapChildSource> children = viewModel.GameData.Maps.MapCatalog
            .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap
                && string.Equals(entry.WorldKey, world.Key, StringComparison.Ordinal))
            .OrderBy(entry => entry.Key, StringComparer.Ordinal)
            .Select(entry => new WorldMapChildSource(viewModel.GameData, entry))
            .ToArray();
        WorldEditorPanel.SetWorld(
            world.Key,
            viewModel.GameData.Worlds.ReadWorldMapSnapshot(world.Key),
            children);
    }

    private void onMapListPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (viewModel?.CanEdit != true)
            return;
        if (viewModel is null || !args.GetCurrentPoint(MapList).Properties.IsRightButtonPressed)
            return;
        TreeViewItem? item = getMapListItem(args.Source);
        MapListItemViewModel? map = item?.DataContext as MapListItemViewModel;
        if (map is not null)
            viewModel.MapWorkspace.SelectedMap = map;
        showMapContextMenu(map);
        args.Handled = true;
    }

    private async void onMapListDoubleTapped(object? sender, TappedEventArgs args)
    {
        if (viewModel?.CanEdit != true)
            return;
        TreeViewItem? item = getMapListItem(args.Source);
        if (item?.DataContext is not MapListItemViewModel map)
            return;
        if (map.IsMap)
            await editMapAsync(map.Key);
        args.Handled = true;
    }

    private async void onMapListKeyDown(object? sender, KeyEventArgs args)
    {
        if (viewModel?.CanEdit != true)
            return;
        if (viewModel is null)
            return;
        bool primary = EditorShortcuts.HasPrimaryModifier(args.KeyModifiers);
        if (primary
            && args.Key == Key.C
            && viewModel.MapWorkspace.SelectedMap is { IsMap: true } copyMapItem)
        {
            viewModel.MapWorkspace.CopyMap(copyMapItem.Key);
        }
        else if (primary && args.Key == Key.V)
            viewModel.MapWorkspace.PasteMap();
        else if (args.Key == Key.Delete && viewModel.MapWorkspace.SelectedMap is MapListItemViewModel deleteMapItem)
        {
            if (deleteMapItem.IsWorld)
                await deleteWorldMapAsync(deleteMapItem.Key);
            else
                await deleteMapAsync(deleteMapItem.Key);
        }
        else if (args.Key is Key.Enter or Key.Return
            && viewModel.MapWorkspace.SelectedMap is MapListItemViewModel editMapItem)
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
            MenuItem pasteItem = new() { Header = LocaleService.Get("PASTE"), IsEnabled = viewModel.MapWorkspace.CanPasteMap };
            pasteItem.Click += (_, _) => viewModel.MapWorkspace.PasteMap();
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
            copyItem.Click += (_, _) => viewModel.MapWorkspace.CopyMap(map.Key);
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
        MapInfo initial = viewModel.MapWorkspace.NewMapDefaults();
        MapInfo? result = await MapEditWindow.ShowAsync(this, viewModel.GameData, initial, string.Empty, true);
        if (result is not null)
            viewModel.MapWorkspace.CreateMap(result);
    }

    private async Task createWorldMapAsync()
    {
        if (viewModel is null)
            return;
        WorldMapInfo initial = viewModel.MapWorkspace.NewWorldDefaults();
        WorldMapInfo? result = await WorldMapEditWindow.ShowAsync(
            this,
            viewModel.GameData,
            initial,
            true);
        if (result is null)
            return;
        if (!viewModel.MapWorkspace.CreateWorld(result))
        {
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("WORLD_CREATE_FAILED"));
            return;
        }
    }

    private async Task createWorldChildMapAsync(string worldKey)
    {
        if (viewModel is null)
            return;
        MapInfo initial = viewModel.MapWorkspace.NewWorldChildDefaults(worldKey);
        MapInfo? result = await MapEditWindow.ShowAsync(
            this,
            viewModel.GameData,
            initial,
            string.Empty,
            true,
            worldKey);
        if (result is null)
            return;
        if (!viewModel.MapWorkspace.CreateWorldChild(worldKey, result))
        {
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("WORLD_CHILD_CREATE_FAILED"));
            return;
        }
    }

    private async Task editWorldMapAsync(string worldKey)
    {
        if (viewModel?.GameData.Worlds.getWorldMapInfo(worldKey) is not WorldMapInfo initial)
            return;
        WorldMapInfo? result = await WorldMapEditWindow.ShowAsync(
            this,
            viewModel.GameData,
            initial,
            false);
        if (result is null)
            return;
        WorldMapMutationResult update = viewModel.MapWorkspace.UpdateWorld(worldKey, result);
        if (!update.Success)
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), update.Details);
            return;
        }
    }

    private async Task renameWorldMapAsync(string worldKey)
    {
        if (viewModel is null)
            return;
        IReadOnlyList<string> existing = viewModel.MapWorkspace.WorldRenameCandidates(worldKey);
        string? result = await SingleRowDialog.ShowAsync(
            this,
            LocaleService.Get("RENAME_WORLD_MAP"),
            LocaleService.Get("WORLD_FOLDER_NAME_PROMPT"),
            existing,
            worldKey);
        if (result is null || string.Equals(result, worldKey, StringComparison.Ordinal))
            return;
        bool renamed = viewModel.MapWorkspace.RenameWorld(worldKey, result);
        if (!renamed)
        {
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("WORLD_RENAME_FAILED"));
            return;
        }
    }

    private async Task deleteWorldMapAsync(string worldKey)
    {
        if (viewModel is null)
            return;
        IReadOnlyList<ReferenceRecord> references = viewModel.MapWorkspace.WorldDeleteReferences(worldKey);
        if (references.Count != 0)
        {
            await showMapReferenceBlockAsync(references);
            return;
        }
        int childCount = viewModel.MapWorkspace.WorldChildCount(worldKey);
        string message = string.Format(
            LocaleService.Get("WORLD_DELETE_CONFIRMATION"),
            worldKey,
            childCount) + Environment.NewLine + LocaleService.Get("DELETE_DOCUMENT_CONFIRMATION");
        if (!await ConfirmationDialog.ShowAsync(
                this,
                LocaleService.Get("CONFIRM_DELETE"),
                message))
        {
            return;
        }
        if (!await EditorResourceOperations.DeleteAsync(this, () => viewModel.MapWorkspace.DeleteWorld(worldKey)))
        {
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("DELETE_FAILED"));
            return;
        }
    }

    private async Task editMapAsync(string key)
    {
        if (viewModel is null)
            return;
        (MapInfo? initial, string? worldKey) = viewModel.MapWorkspace.MapProperties(key);
        if (initial is null)
            return;
        MapInfo? result = await MapEditWindow.ShowAsync(
            this,
            viewModel.GameData,
            initial,
            key,
            false,
            worldKey);
        if (result is null)
            return;
        bool updated = viewModel.MapWorkspace.UpdateMap(key, result, worldKey);
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
    }

    private async Task deleteMapAsync(string key)
    {
        if (viewModel is null)
            return;
        IReadOnlyList<ReferenceRecord> references = viewModel.MapWorkspace.MapDeleteReferences(key);
        if (references.Count != 0)
        {
            await showMapReferenceBlockAsync(references);
            return;
        }
        if (!await ConfirmationDialog.ShowAsync(
                this,
                LocaleService.Get("CONFIRM_DELETE"),
                LocaleService.Get("DELETE_DOCUMENT_CONFIRMATION")))
        {
            return;
        }
        await EditorResourceOperations.DeleteAsync(this, () => viewModel.MapWorkspace.DeleteMap(key));
    }

    private async Task showMapReferenceBlockAsync(IReadOnlyList<ReferenceRecord> references)
    {
        if (viewModel is null)
            return;
        string details = viewModel.MapWorkspace.DescribeReferences(references);
        await AlertDialog.ShowAsync(
            this,
            LocaleService.Get("ERROR"),
            LocaleService.Get("MAP_TARGET_REFERENCED") + Environment.NewLine + details);
    }
}
