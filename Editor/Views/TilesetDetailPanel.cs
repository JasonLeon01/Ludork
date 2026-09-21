using Ludork.Models;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Threading;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Controls.Templates;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Views;

internal sealed class TilesetDetailPanel : Grid
{
    private readonly Window owner;
    private readonly ProjectDataStore gameData;
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
    private EditorDocument? resourceDocument;
    private bool attached;
    private bool refreshPending;
    private long brushGestureId;

    public TilesetDetailPanel(Window owner, ProjectDataStore gameData, bool isAutoTile, Action dataChanged)
    {
        this.owner = owner;
        this.gameData = gameData;
        this.isAutoTile = isAutoTile;
        this.dataChanged = dataChanged;
        RowDefinitions = new RowDefinitions("Auto,64,*");
        RowSpacing = 5;
        imageEditor = new TilesetImageEditor(gameData, gameData.Configs.getCellSize())
        {
            EditRequested = updateMetadata,
            DirectionEditRequested = updateDirection,
            MaterialCommitRequested = updateMaterial,
            GestureStarted = () => brushGestureId = gameData.BeginHistoryGesture(),
            GestureCompleted = () => gameData.EndHistoryGesture(brushGestureId),
            MaterialEditRequested = editMaterial,
            HorizontalAlignment = HorizontalAlignment.Left,
            VerticalAlignment = VerticalAlignment.Top,
        };
        imageEditor.ImageChanged += (_, _) => updateImageStatus();
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
            Background = Ludork.Services.EditorTheme.Brush("Background"),
        };
        Grid.SetRow(scroll, 2);
        Children.Add(scroll);
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        attached = true;
        gameData.DataReloaded += onAssetsReloaded;
        imageEditor.ReloadImage();
        if (resourceDocument is not null)
            resourceDocument.Changed += onDocumentChanged;
        onDocumentChanged(this, EventArgs.Empty);
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        attached = false;
        gameData.DataReloaded -= onAssetsReloaded;
        imageEditor.Dispose();
        if (resourceDocument is not null)
            resourceDocument.Changed -= onDocumentChanged;
        base.OnDetachedFromVisualTree(args);
    }

    private void onDocumentChanged(object? sender, EventArgs args)
    {
        if (refreshPending)
            return;
        refreshPending = true;
        Dispatcher.UIThread.Post(() =>
        {
            refreshPending = false;
            if (!attached || resourceDocument is null)
                return;
            JsonObject? current = resourceDocument.Data;
            if (!JsonNode.DeepEquals(current, data) || key != resourceDocument.Key)
                setData(resourceDocument.Key, current);
        });
    }

    public EditorDocument? Document => resourceDocument;

    public void setData(string? nextKey, JsonObject? nextData)
    {
        if (resourceDocument is not null)
            resourceDocument.Changed -= onDocumentChanged;
        key = nextKey;
        resourceDocument = nextKey is null ? null : gameData.GetDocument(isAutoTile ? "AutoTiles" : "Tilesets", nextKey);
        if (attached && resourceDocument is not null)
            resourceDocument.Changed += onDocumentChanged;
        data = nextData;
        populating = true;
        nameBox.Text = data?["name"]?.GetValue<string>() ?? string.Empty;
        string fileName = data?["fileName"]?.GetValue<string>() ?? string.Empty;
        fileBox.Text = fileName;
        imageEditor.setData(
            data,
            gameData.ProjectPath,
            fileName,
            isAutoTile);
        populating = false;
    }

    private void onAssetsReloaded(object? sender, EventArgs args) => imageEditor.ReloadImage();

    private void updateImageStatus()
    {
        string fileName = fileBox.Text ?? string.Empty;
        bool missing = fileName.Length != 0 && !imageEditor.HasImage;
        fileBox.BorderBrush = new SolidColorBrush(missing ? Color.Parse("#b94a48") : EditorInputs.ReadOnlyBorderColor);
        ToolTip.SetTip(fileBox, missing ? fileName : null);
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
        if (populating || data is null || key is null)
            return;
        string value = nameBox.Text ?? string.Empty;
        if ((data["name"]?.GetValue<string>() ?? string.Empty) == value)
            return;
        if (gameData.Assets.UpdateTilesetName(key, isAutoTile, value))
        {
            data["name"] = value;
            dataChanged();
        }
        else
            refreshCurrent();
    }

    private void updateMode()
    {
        imageEditor.Mode = (TilesetEditMode)Math.Max(0, modeList.SelectedIndex);
        imageEditor.InvalidateVisual();
    }

    private async void browseFileAsync()
    {
        if (data is null || key is null)
            return;
        string selectedKey = key;
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
        using EditorThumbnailLease? lease = await gameData.Thumbnails.AcquireAsync(path, 0);
        if (lease is null)
            return;
        Avalonia.Media.Imaging.Bitmap bitmap = lease.Bitmap;
        if (isAutoTile && (bitmap.PixelSize.Width < 96 || bitmap.PixelSize.Height < 128 || bitmap.PixelSize.Width % 96 != 0))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), string.Format(LocaleService.Get("AUTOTILE_FILE_SIZE_INVALID"), bitmap.PixelSize.Width, bitmap.PixelSize.Height));
            return;
        }
        if (gameData.Assets.UpdateTilesetImage(selectedKey, isAutoTile, GameAssetPath.FromProjectFile(gameData.ProjectPath, path), bitmap.PixelSize.Width, bitmap.PixelSize.Height))
            dataChanged();
        if (key == selectedKey)
            refreshCurrent();
    }

    private bool updateMetadata(string property, JsonNode value, IReadOnlyList<int> indices, int count)
    {
        if (key is null || data is null)
            return false;
        string assetPath = data["fileName"]?.GetValue<string>() ?? string.Empty;
        return completeMetadataEdit(gameData.Assets.UpdateTilesetMetadata(key, isAutoTile, assetPath, property, value, indices, count));
    }

    private bool updateDirection(int index, int count, int direction, bool value)
    {
        if (key is null || data is null)
            return false;
        string assetPath = data["fileName"]?.GetValue<string>() ?? string.Empty;
        return completeMetadataEdit(gameData.Assets.UpdateTilesetDirection(key, assetPath, index, count, direction, value));
    }

    private bool updateMaterial(int index, int count, JsonObject initial, JsonObject edited)
    {
        if (key is null || data is null)
            return false;
        string assetPath = data["fileName"]?.GetValue<string>() ?? string.Empty;
        return completeMetadataEdit(gameData.Assets.UpdateTilesetMaterial(key, isAutoTile, assetPath, index, count, initial, edited));
    }

    private bool completeMetadataEdit(bool changed)
    {
        if (changed)
        {
            dataChanged();
            return true;
        }
        refreshCurrent();
        return false;
    }

    private void refreshCurrent()
    {
        IReadOnlyDictionary<string, TilesetSnapshot> entries = isAutoTile ? gameData.Assets.AutoTileData : gameData.Assets.TilesetData;
        setData(key, key is not null && entries.TryGetValue(key, out TilesetSnapshot? value) ? value.ToJson() : null);
    }

    private void editMaterial(JsonObject material, Action<JsonObject> apply)
    {
        MaterialEditorWindow window = new(material, apply);
        window.ShowDialog(owner);
    }
}
