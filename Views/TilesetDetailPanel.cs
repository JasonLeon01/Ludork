using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Controls.Templates;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Views;

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
        nameBox.PropertyChanged += (_, args) =>
        {
            if (args.Property == TextBox.TextProperty)
                updateName();
        };
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
        bool resolved = GameAssetPath.TryResolveExistingFile(
            gameData.ProjectPath,
            fileName,
            out _);
        bool missingFile = fileName.Length != 0 && !resolved;
        fileBox.Text = fileName;
        fileBox.BorderBrush = new SolidColorBrush(
            missingFile ? Color.Parse("#b94a48") : EditorInputs.ReadOnlyBorderColor);
        ToolTip.SetTip(fileBox, missingFile ? fileName : null);
        imageEditor.setData(
            data,
            gameData.ProjectPath,
            fileName,
            isAutoTile);
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
        if ((data["name"]?.GetValue<string>() ?? string.Empty) == value)
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
        string? initialFilePath = GameAssetPath.TryResolveExistingFile(
            gameData.ProjectPath,
            current,
            out string resolvedCurrent)
            ? resolvedCurrent
            : null;
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
        data["fileName"] = GameAssetPath.FromProjectFile(gameData.ProjectPath, path);
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
