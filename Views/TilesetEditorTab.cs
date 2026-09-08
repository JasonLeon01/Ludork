using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.VisualTree;
using Ludork.Services;
using Ludork.ViewModels;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views;

internal sealed class TilesetEditorTab : Grid
{
    private readonly Window owner;
    private readonly GameDataService gameData;
    private readonly TileSelectViewModel tileSelect;
    private readonly bool isAutoTile;
    private readonly ListBox dataList = new()
    {
        ItemTemplate = HintedTextPresenter.StringItemTemplate,
    };
    private readonly TilesetDetailPanel detail;
    private JsonObject? clipboard;
    private string? clipboardName;
    private bool selectionChanging;

    public TilesetEditorTab(Window owner, GameDataService gameData, TileSelectViewModel tileSelect, bool isAutoTile)
    {
        this.owner = owner;
        this.gameData = gameData;
        this.tileSelect = tileSelect;
        this.isAutoTile = isAutoTile;
        ColumnDefinitions = new ColumnDefinitions("120,*");
        detail = new TilesetDetailPanel(owner, gameData, isAutoTile, onDataChanged);
        dataList.Width = 120;
        dataList.SelectionChanged += (_, _) => selectCurrent();
        dataList.AddHandler(KeyDownEvent, onListKeyDown, RoutingStrategies.Tunnel);
        dataList.AddHandler(PointerPressedEvent, onListPointerPressed, RoutingStrategies.Tunnel);

        Children.Add(dataList);
        Grid.SetColumn(detail, 1);
        Children.Add(detail);
        refreshList();
    }

    private IReadOnlyDictionary<string, JsonObject> data => isAutoTile ? gameData.AutoTileData : gameData.TilesetData;
    private string title => LocaleService.Get(isAutoTile ? "AUTOTILES_DATA" : "TILESETS_DATA");
    private string addTitle => LocaleService.Get(isAutoTile ? "ADD_AUTOTILE" : "ADD_TILESET");
    private string renameTitle => LocaleService.Get(isAutoTile ? "RENAME_AUTOTILE" : "RENAME_TILESET");
    private string prompt => LocaleService.Get(isAutoTile ? "ENTER_AUTOTILE_NAME" : "ENTER_TILESET_FILE");

    private void refreshList(string? selectedKey = null)
    {
        selectedKey ??= dataList.SelectedItem as string;
        selectionChanging = true;
        string[] keys = data.Keys.ToArray();
        dataList.ItemsSource = keys;
        dataList.SelectedItem = keys.Contains(selectedKey, StringComparer.Ordinal) ? selectedKey : keys.FirstOrDefault();
        selectionChanging = false;
        selectCurrent();
    }

    private void selectCurrent()
    {
        if (selectionChanging)
            return;
        string? key = dataList.SelectedItem as string;
        detail.setData(key, key is not null && data.TryGetValue(key, out JsonObject? value) ? value : null);
    }

    private async void addAsync()
    {
        string? key = await SingleRowDialog.ShowAsync(owner, addTitle, prompt, data.Keys);
        if (string.IsNullOrWhiteSpace(key))
            return;
        bool added = isAutoTile ? gameData.CreateAutoTile(key) : gameData.CreateTileset(key);
        if (!added)
            return;
        refreshAll(key);
    }

    private async void renameAsync()
    {
        if (dataList.SelectedItem is not string oldKey || !data.ContainsKey(oldKey))
            return;
        string? nextKey = await SingleRowDialog.ShowAsync(owner, renameTitle, prompt, data.Keys.Where(key => key != oldKey), oldKey);
        if (string.IsNullOrWhiteSpace(nextKey) || nextKey == oldKey)
            return;
        if (!isAutoTile)
        {
            IReadOnlyList<string> referencingMaps = gameData.GetMapsReferencingTileset(oldKey);
            bool updateReferences = referencingMaps.Count != 0;
            if (updateReferences)
            {
                string mapFiles = string.Join(
                    Environment.NewLine,
                    referencingMaps.Select(gameData.GetMapRuntimePath));
                bool confirmed = await ConfirmationDialog.ShowAsync(
                    owner,
                    renameTitle,
                    string.Format(
                        LocaleService.Get("TILESET_REFERENCED_WARNING"),
                        mapFiles,
                        oldKey,
                        nextKey));
                if (!confirmed)
                    return;
            }
            if (!gameData.RenameTileset(oldKey, nextKey, updateReferences))
            {
                await AlertDialog.ShowAsync(
                    owner,
                    LocaleService.Get("ERROR"),
                    LocaleService.Get("TILESET_RENAME_FAILED"));
                return;
            }
            tileSelect.setCurrentTilesetKey(nextKey);
            refreshAll(nextKey);
            return;
        }
        if (gameData.RenameAutoTile(oldKey, nextKey))
            refreshAll(nextKey);
    }

    private void copy()
    {
        if (dataList.SelectedItem is not string key || !data.TryGetValue(key, out JsonObject? value))
            return;
        clipboard = (JsonObject)value.DeepClone();
        clipboardName = key;
    }

    private void paste()
    {
        if (clipboard is null)
            return;
        string baseName = clipboardName ?? (isAutoTile ? "AutoTile" : "Tileset");
        string key = getCopyName(baseName);
        if (gameData.PasteTileset(key, isAutoTile, clipboard))
            refreshAll(key);
    }

    private async void deleteAsync()
    {
        if (dataList.SelectedItem is not string key)
            return;
        bool confirmed = await ConfirmationDialog.ShowAsync(owner, LocaleService.Get("CONFIRM_DELETE"), LocaleService.Get("DELETE_CONFIRMATION"));
        if (!confirmed)
            return;
        if (gameData.DeleteTileset(key, isAutoTile))
            refreshAll();
    }

    private void onListKeyDown(object? sender, KeyEventArgs args)
    {
        if (EditorShortcuts.HasPrimaryModifier(args.KeyModifiers) && args.Key == Key.C)
            copy();
        else if (EditorShortcuts.HasPrimaryModifier(args.KeyModifiers) && args.Key == Key.V)
            paste();
        else if (args.Key == Key.Delete)
            deleteAsync();
        else
            return;
        args.Handled = true;
    }

    private void onListPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        PointerPoint point = args.GetCurrentPoint(dataList);
        if (!point.Properties.IsRightButtonPressed)
            return;

        string? key = getItemAt(point.Position);
        if (key is not null)
            dataList.SelectedItem = key;
        showContextMenu(key);
        args.Handled = true;
    }

    private string? getItemAt(Point position)
    {
        Visual? visual = dataList.InputHitTest(position) as Visual;
        while (visual is not null)
        {
            if (visual is ListBoxItem { DataContext: string key })
                return key;
            visual = visual.GetVisualParent();
        }
        return null;
    }

    private void showContextMenu(string? key)
    {
        ContextMenu menu = new();
        if (key is null)
        {
            MenuItem add = new() { Header = addTitle };
            add.Click += (_, _) => addAsync();
            MenuItem pasteItem = new() { Header = LocaleService.Get("PASTE"), IsEnabled = clipboard is not null };
            pasteItem.Click += (_, _) => paste();
            menu.ItemsSource = new object[] { add, pasteItem };
        }
        else
        {
            MenuItem rename = new() { Header = renameTitle };
            rename.Click += (_, _) => renameAsync();
            MenuItem copyItem = new() { Header = LocaleService.Get("COPY") };
            copyItem.Click += (_, _) => copy();
            MenuItem delete = new() { Header = LocaleService.Get("DELETE") };
            delete.Click += (_, _) => deleteAsync();
            menu.ItemsSource = new object[] { rename, copyItem, delete };
        }
        dataList.ContextMenu = menu;
        menu.Open(dataList);
    }

    private string getCopyName(string baseName)
    {
        string candidate = baseName + " (copy)";
        for (int index = 1; data.ContainsKey(candidate); index++)
            candidate = $"{baseName} (copy) ({index})";
        return candidate;
    }

    private void onDataChanged()
    {
        tileSelect.RefreshData();
    }

    private void refreshAll(string? selectedKey = null)
    {
        tileSelect.RefreshData();
        refreshList(selectedKey);
    }

    public void SelectData(string key) => refreshList(key);

    public void RefreshAfterDataRestore() => refreshAll();
}
