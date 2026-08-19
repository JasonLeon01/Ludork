using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Controls.Templates;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Controls;
using Ludork.Services;
using Ludork.ViewModels;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class TilesetEditorWindow : Window
{
    private readonly GameDataService gameData;
    private readonly ProjectSaveService projectSave;
    private readonly TileSelectViewModel tileSelect;
    private readonly List<TilesetEditorTab> editorTabs = [];
    private readonly HashSet<TabItem> initializedTabs = [];
    private readonly HashSet<TabItem> pendingTabs = [];
    private readonly DeferredWindowInitializer initializer;
    private readonly Toast toast;
    private TabControl? tabControl;
    private TabItem? tilesetItem;
    private TabItem? autoTileItem;
    private TilesetEditorTab? tilesetTab;
    private TilesetEditorTab? autoTileTab;
    private string? pendingTilesetKey;
    private string? pendingAutoTileKey;
    private bool pendingAutoTilePage;
    private bool closed;

    public TilesetEditorWindow(
        GameDataService gameData,
        ProjectSaveService projectSave,
        TileSelectViewModel tileSelect)
    {
        this.gameData = gameData;
        this.projectSave = projectSave;
        this.tileSelect = tileSelect;
        Title = LocaleService.Get("TILESETS_DATA");
        Width = 560;
        Height = 480;
        MinWidth = 560;
        MinHeight = 480;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.FromRgb(43, 43, 43));
        FontFamily = FontFamily.Parse("avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);
        HistoryMergeBehavior.AttachBoundary(this, gameData);

        Content = DeferredWindowInitializer.CreateLoadingContent();
        initializer = new DeferredWindowInitializer(this, initializeContent);
        toast = new Toast(this);
        gameData.DataRestored += onDataRestored;
        Closed += (_, _) =>
        {
            closed = true;
            gameData.DataRestored -= onDataRestored;
        };
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
    }

    public void NavigateTo(bool isAutoTile, string? key = null)
    {
        pendingAutoTilePage = isAutoTile;
        if (key is not null)
        {
            if (isAutoTile)
                pendingAutoTileKey = key;
            else
                pendingTilesetKey = key;
        }
        if (initializer.IsInitialized)
            navigateToPendingPage();
    }

    private void initializeContent()
    {
        tabControl = new TabControl
        {
            ItemsPanel = new FuncTemplate<Panel?>(() => new StackPanel { Orientation = Orientation.Horizontal }),
        };
        tilesetItem = new TabItem
        {
            Header = LocaleService.Get("TILESETS_DATA"),
            Content = DeferredWindowInitializer.CreateLoadingContent(),
        };
        autoTileItem = new TabItem
        {
            Header = LocaleService.Get("AUTOTILES_DATA"),
            Content = DeferredWindowInitializer.CreateLoadingContent(),
        };
        tabControl.Items.Add(tilesetItem);
        tabControl.Items.Add(autoTileItem);
        tabControl.SelectionChanged += (_, _) => initializeTab(tabControl.SelectedItem as TabItem);
        Content = new Border { Padding = new Thickness(5), Child = tabControl };
        navigateToPendingPage();
    }

    private void navigateToPendingPage()
    {
        TabItem? item = pendingAutoTilePage ? autoTileItem : tilesetItem;
        if (tabControl is null || item is null)
            return;
        tabControl.SelectedItem = item;
        initializeTab(item);
        applyPendingSelection(pendingAutoTilePage);
    }

    private void initializeTab(TabItem? item)
    {
        if (item is null || initializedTabs.Contains(item) || !pendingTabs.Add(item))
            return;
        Dispatcher.UIThread.Post(() =>
        {
            pendingTabs.Remove(item);
            if (closed || initializedTabs.Contains(item))
                return;
            bool isAutoTile = ReferenceEquals(item, autoTileItem);
            TilesetEditorTab tab = new(this, gameData, tileSelect, isAutoTile);
            if (isAutoTile)
                autoTileTab = tab;
            else
                tilesetTab = tab;
            editorTabs.Add(tab);
            initializedTabs.Add(item);
            item.Content = tab;
            applyPendingSelection(isAutoTile);
        }, DispatcherPriority.Background);
    }

    private void applyPendingSelection(bool isAutoTile)
    {
        TilesetEditorTab? tab = isAutoTile ? autoTileTab : tilesetTab;
        string? key = isAutoTile ? pendingAutoTileKey : pendingTilesetKey;
        if (tab is null || key is null)
            return;
        if (isAutoTile)
            pendingAutoTileKey = null;
        else
            pendingTilesetKey = null;
        tab.SelectData(key);
    }

    private void onDataRestored(object? sender, EventArgs args)
    {
        tileSelect.RefreshData();
        foreach (TilesetEditorTab tab in editorTabs)
            tab.RefreshAfterDataRestore();
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (!EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return;
        if (args.Key == Key.Z)
            EditorFeedback.ShowHistory(toast, "Undo", gameData.Undo());
        else if (args.Key == Key.Y)
            EditorFeedback.ShowHistory(toast, "Redo", gameData.Redo());
        else if (args.Key == Key.S)
            await EditorSaveWorkflow.TrySaveAsync(this, projectSave);
        else
            return;
        args.Handled = true;
    }
}

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
        if (dataList.SelectedItem is not string oldKey || !data.TryGetValue(oldKey, out JsonObject? value))
            return;
        string? nextKey = await SingleRowDialog.ShowAsync(owner, renameTitle, prompt, data.Keys.Where(key => key != oldKey), oldKey);
        if (string.IsNullOrWhiteSpace(nextKey) || nextKey == oldKey)
            return;
        if (!isAutoTile && mapsReferenceTileset(oldKey))
        {
            bool proceed = await ConfirmationDialog.ShowAsync(owner, renameTitle, string.Format(LocaleService.Get("TILESET_REFERENCED_WARNING"), string.Join(Environment.NewLine, getReferencingMaps(oldKey))));
            if (!proceed)
                return;
        }
        gameData.RecordSnapshot();
        Dictionary<string, JsonObject> target = isAutoTile
            ? (Dictionary<string, JsonObject>)gameData.AutoTileData
            : (Dictionary<string, JsonObject>)gameData.TilesetData;
        target.Remove(oldKey);
        target[nextKey] = value;
        value["name"] = nextKey;
        gameData.refreshModifiedState();
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
        Dictionary<string, JsonObject> target = isAutoTile
            ? (Dictionary<string, JsonObject>)gameData.AutoTileData
            : (Dictionary<string, JsonObject>)gameData.TilesetData;
        JsonObject copy = (JsonObject)clipboard.DeepClone();
        copy["name"] = key;
        gameData.RecordSnapshot();
        target[key] = copy;
        gameData.refreshModifiedState();
        refreshAll(key);
    }

    private async void deleteAsync()
    {
        if (dataList.SelectedItem is not string key)
            return;
        bool confirmed = await ConfirmationDialog.ShowAsync(owner, LocaleService.Get("CONFIRM_DELETE"), LocaleService.Get("DELETE_CONFIRMATION"));
        if (!confirmed)
            return;
        if (!data.ContainsKey(key))
            return;
        gameData.RecordSnapshot();
        Dictionary<string, JsonObject> target = isAutoTile
            ? (Dictionary<string, JsonObject>)gameData.AutoTileData
            : (Dictionary<string, JsonObject>)gameData.TilesetData;
        target.Remove(key);
        gameData.refreshModifiedState();
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

    private bool mapsReferenceTileset(string key) => getReferencingMaps(key).Count != 0;

    private List<string> getReferencingMaps(string key)
    {
        return gameData.MapData
            .Where(map => map.Value["layers"] is JsonObject layers && layers.Any(layer => layer.Value?["layerTileset"]?.GetValue<string>() == key))
            .Select(map => map.Key)
            .ToList();
    }

    private void onDataChanged()
    {
        gameData.refreshModifiedState();
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

internal sealed class TilesetDetailPanel : Grid
{
    private readonly Window owner;
    private readonly GameDataService gameData;
    private readonly bool isAutoTile;
    private readonly Action dataChanged;
    private readonly TextBox nameBox = EditorInputs.CreateEditableTextBox();
    private readonly TextBox fileBox = EditorInputs.CreateReadOnlyTextBox();
    private readonly ListBox modeList = new()
    {
        Height = 64,
        ItemTemplate = HintedTextPresenter.StringItemTemplate,
        ItemsPanel = new FuncTemplate<Panel?>(() => new StackPanel { Orientation = Orientation.Horizontal }),
    };
    private readonly TilesetImageEditor imageEditor;
    private JsonObject? data;
    private string? key;
    private bool populating;

    public TilesetDetailPanel(Window owner, GameDataService gameData, bool isAutoTile, Action dataChanged)
    {
        this.owner = owner;
        this.gameData = gameData;
        this.isAutoTile = isAutoTile;
        this.dataChanged = dataChanged;
        RowDefinitions = new RowDefinitions("Auto,64,*");
        RowSpacing = 5;
        imageEditor = new TilesetImageEditor(gameData.getCellSize())
        {
            BeforeDataChanged = gameData.RecordSnapshot,
            DataChanged = onImageDataChanged,
            MaterialEditRequested = editMaterial,
            HorizontalAlignment = HorizontalAlignment.Left,
            VerticalAlignment = VerticalAlignment.Top,
        };
        HistoryMergeBehavior.Attach(nameBox, gameData);
        nameBox.TextChanged += (_, _) => updateName();
        modeList.SelectionChanged += (_, _) => updateMode();
        modeList.ItemsSource = isAutoTile
            ? new[] { LocaleService.Get("PASSABLE"), LocaleService.Get("MATERIAL") }
            : new[] { LocaleService.Get("PASSABLE"), LocaleService.Get("MATERIAL"), LocaleService.Get("DIR4") };
        modeList.SelectedIndex = 0;

        Grid header = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*,10,Auto,*,Auto"),
            ColumnSpacing = 8,
        };
        addHeader(header, 0, isAutoTile ? "AUTOTILE_NAME" : "TILESET_NAME", nameBox);
        addHeader(header, 3, "FILE_NAME", fileBox);
        Button browse = new() { Content = "...", MinWidth = 34 };
        browse.Click += (_, _) => browseFileAsync();
        Grid.SetColumn(browse, 5);
        header.Children.Add(browse);
        Children.Add(header);

        Grid.SetRow(modeList, 1);
        Children.Add(modeList);
        ScrollViewer scroll = new()
        {
            Content = imageEditor,
            HorizontalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
            HorizontalContentAlignment = HorizontalAlignment.Left,
            VerticalContentAlignment = VerticalAlignment.Top,
            Background = Brushes.Black,
        };
        Grid.SetRow(scroll, 2);
        Children.Add(scroll);
    }

    public void setData(string? nextKey, JsonObject? nextData)
    {
        key = nextKey;
        data = nextData;
        populating = true;
        nameBox.Text = data?["name"]?.GetValue<string>() ?? string.Empty;
        string fileName = data?["fileName"]?.GetValue<string>() ?? string.Empty;
        string path = string.IsNullOrWhiteSpace(fileName) ? string.Empty : Path.Combine(gameData.ProjectPath, "Assets", isAutoTile ? "Autotiles" : "Tilesets", fileName);
        bool missingFile = fileName.Length != 0 && !File.Exists(path);
        fileBox.Text = fileName;
        fileBox.BorderBrush = new SolidColorBrush(
            missingFile ? Color.Parse("#b94a48") : EditorInputs.ReadOnlyBorderColor);
        ToolTip.SetTip(fileBox, missingFile ? path : null);
        imageEditor.setData(data, path, isAutoTile);
        populating = false;
    }

    private static void addHeader(Grid header, int labelColumn, string label, Control editor)
    {
        TextBlock text = new() { Text = LocaleService.Get(label), VerticalAlignment = VerticalAlignment.Center };
        Grid.SetColumn(text, labelColumn);
        header.Children.Add(text);
        editor.VerticalAlignment = VerticalAlignment.Center;
        Grid.SetColumn(editor, labelColumn + 1);
        header.Children.Add(editor);
    }

    private void updateName()
    {
        if (populating || data is null)
            return;
        string value = nameBox.Text ?? string.Empty;
        if (data["name"]?.GetValue<string>() == value)
            return;
        gameData.RecordSnapshot();
        data["name"] = value;
        dataChanged();
    }

    private void updateMode()
    {
        imageEditor.Mode = (TilesetEditMode)Math.Max(0, modeList.SelectedIndex);
        imageEditor.InvalidateVisual();
    }

    private async void browseFileAsync()
    {
        if (data is null)
            return;
        string root = Path.Combine(gameData.ProjectPath, "Assets", isAutoTile ? "Autotiles" : "Tilesets");
        Directory.CreateDirectory(root);
        string current = data["fileName"]?.GetValue<string>() ?? string.Empty;
        string? initialFilePath = string.IsNullOrWhiteSpace(current)
            ? null
            : Path.Combine(root, current);
        string? path = await FileSelectorDialog.ShowAsync(
            owner,
            root,
            FileSelectorDialog.ImageFilesFilter(),
            initialFilePath: initialFilePath);
        if (path is null)
            return;
        using Avalonia.Media.Imaging.Bitmap bitmap = new(path);
        if (isAutoTile && (bitmap.PixelSize.Width < 96 || bitmap.PixelSize.Height < 128 || bitmap.PixelSize.Width % 96 != 0))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), string.Format(LocaleService.Get("AUTOTILE_FILE_SIZE_INVALID"), bitmap.PixelSize.Width, bitmap.PixelSize.Height));
            return;
        }
        gameData.RecordSnapshot();
        data["fileName"] = Path.GetFileName(path);
        if (isAutoTile)
            data["material"] ??= createDefaultMaterial();
        else
            resizeTilesetMetadata(bitmap.PixelSize.Width / gameData.getCellSize() * (bitmap.PixelSize.Height / gameData.getCellSize()));
        dataChanged();
        setData(key, data);
    }

    private void resizeTilesetMetadata(int count)
    {
        if (data is null)
            return;
        resize(data, "passable", count, () => true);
        resize(data, "materials", count, createDefaultMaterial);
        resize(data, "dir4", count, () => new JsonArray(true, true, true, true));
    }

    private static void resize(JsonObject data, string name, int count, Func<JsonNode?> createValue)
    {
        JsonArray values = data[name] as JsonArray ?? new JsonArray();
        while (values.Count < count)
            values.Add(createValue());
        while (values.Count > count)
            values.RemoveAt(values.Count - 1);
        data[name] = values;
    }

    private void onImageDataChanged() => dataChanged();

    private void editMaterial(JsonObject material, Action<JsonObject> apply)
    {
        MaterialEditorWindow window = new(material, apply);
        window.ShowDialog(owner);
    }

    private static JsonObject createDefaultMaterial() => new()
    {
        ["lightBlock"] = 0.0,
        ["mirror"] = false,
        ["reflectionStrength"] = 0.5,
        ["opacity"] = 1.0,
        ["speedRate"] = 1.0,
    };
}
