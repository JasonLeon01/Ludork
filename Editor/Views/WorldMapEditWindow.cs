using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class WorldMapEditWindow : Window
{
    private readonly ProjectDataStore gameData;
    private readonly bool isNew;
    private readonly MapFogEditor fogEditor;
    private readonly TextBox directoryNameBox = EditorInputs.CreateEditableTextBox();
    private readonly TextBox worldNameBox = EditorInputs.CreateEditableTextBox();
    private readonly NumericUpDown widthBox = EditorInputs.CreateNumericUpDown(256, 1, 32768, 1);
    private readonly NumericUpDown heightBox = EditorInputs.CreateNumericUpDown(192, 1, 32768, 1);
    private readonly TextBlock errorText = new()
    {
        Foreground = Brushes.IndianRed,
        TextWrapping = TextWrapping.Wrap,
    };

    private WorldMapEditWindow(
        ProjectDataStore gameData,
        WorldMapInfo initial,
        bool isNew)
    {
        this.gameData = gameData;
        this.isNew = isNew;
        Title = LocaleService.Get(isNew ? "NEW_WORLD_MAP" : "WORLD_MAP_PROPERTIES");
        Width = 600;
        Height = 440;
        MinWidth = 520;
        MinHeight = 300;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);
        EditorLayoutService.AttachWindow(this, nameof(WorldMapEditWindow));

        directoryNameBox.Text = initial.DirectoryName;
        worldNameBox.Text = initial.WorldName;
        widthBox.Value = initial.Width;
        heightBox.Value = initial.Height;
        fogEditor = new MapFogEditor(MapVisualSettings.From(initial));

        Grid form = new() { RowSpacing = 8 };
        if (isNew)
            EditorFormRows.Add(form, LocaleService.Get("WORLD_FOLDER_NAME"), directoryNameBox);
        EditorFormRows.Add(form, LocaleService.Get("WORLD_NAME"), worldNameBox);
        EditorFormRows.Add(form, LocaleService.Get("MAP_WIDTH"), widthBox);
        EditorFormRows.Add(form, LocaleService.Get("MAP_HEIGHT"), heightBox);
        EditorFormRows.Add(form, LocaleService.Get("MAP_PANORAMA"), createFileRow(fogEditor.PanoramaBox, "Panoramas"));
        EditorFormRows.Add(form, LocaleService.Get("MAP_FOG"), createFileRow(fogEditor.PathBox, "Fogs"));
        EditorFormRows.Add(form, string.Empty, fogEditor.Options);

        Button confirm = new() { Content = LocaleService.Get("CONFIRM"), MinWidth = 80 };
        confirm.Click += onConfirm;
        Button cancel = new() { Content = LocaleService.Get("CANCEL"), MinWidth = 80 };
        cancel.Click += (_, _) => Close(null);
        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { confirm, cancel },
        };
        StackPanel content = new() { Margin = new Thickness(20), Spacing = 12 };
        content.Children.Add(form);
        content.Children.Add(errorText);
        content.Children.Add(buttons);
        Content = new ScrollViewer
        {
            Content = content,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
        };
        Opened += (_, _) =>
        {
            if (isNew)
                directoryNameBox.Focus();
            else
                widthBox.Focus();
        };
    }

    public static Task<WorldMapInfo?> ShowAsync(
        Window owner,
        ProjectDataStore gameData,
        WorldMapInfo initial,
        bool isNew)
    {
        return new WorldMapEditWindow(gameData, initial, isNew).ShowDialog<WorldMapInfo?>(owner);
    }

    private Control createFileRow(TextBox textBox, string rootName)
    {
        Button browse = new() { Content = "...", MinWidth = 36 };
        browse.Click += async (_, _) => await selectFileAsync(textBox, rootName);
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
            ColumnSpacing = 6,
        };
        row.Children.Add(textBox);
        Grid.SetColumn(browse, 1);
        row.Children.Add(browse);
        return row;
    }

    private async Task selectFileAsync(TextBox target, string rootName)
    {
        string root = Path.Combine(gameData.ProjectPath, "Assets", rootName);
        Directory.CreateDirectory(root);
        string current = target.Text ?? string.Empty;
        string? initialFilePath = GameAssetPath.TryResolveExistingFile(
            gameData.ProjectPath,
            current,
            out string resolvedCurrent)
            ? resolvedCurrent
            : null;
        string? path = await FileSelectorDialog.ShowAsync(
            this,
            root,
            FileSelectorDialog.ImageFilesFilter(),
            initialFilePath: initialFilePath,
            allowEmpty: true);
        if (path is null)
            return;
        string assetPath = string.Empty;
        if (path.Length == 0
            || GameAssetPath.TryFromProjectFile(gameData.ProjectPath, path, out assetPath))
        {
            target.Text = assetPath;
        }
    }

    private void onConfirm(object? sender, RoutedEventArgs args)
    {
        string directoryName = isNew
            ? directoryNameBox.Text?.Trim() ?? string.Empty
            : directoryNameBox.Text ?? string.Empty;
        string worldName = worldNameBox.Text?.Trim() ?? string.Empty;
        if (isNew && !isValidDirectoryName(directoryName))
        {
            errorText.Text = LocaleService.Get("WORLD_FOLDER_NAME_INVALID");
            return;
        }
        if (isNew
            && (gameData.Worlds.WorldMapData.ContainsKey(directoryName)
                || gameData.Maps.MapCatalog.Any(item => string.Equals(item.Key, directoryName, StringComparison.Ordinal))))
        {
            errorText.Text = LocaleService.Get("WORLD_FOLDER_EXISTS");
            return;
        }
        if (worldName.Length == 0)
        {
            errorText.Text = LocaleService.Get("WORLD_NAME_EMPTY");
            return;
        }
        string fog = fogEditor.PathBox.Text?.Trim() ?? string.Empty;
        string panorama = fogEditor.PanoramaBox.Text?.Trim() ?? string.Empty;
        if (fog.Length != 0 && !GameAssetPath.IsCanonical(fog))
        {
            errorText.Text = $"Invalid game asset path: {fog}";
            return;
        }
        if (panorama.Length != 0 && !GameAssetPath.IsCanonical(panorama))
        {
            errorText.Text = $"Invalid game asset path: {panorama}";
            return;
        }
        Close(new WorldMapInfo
        {
            DirectoryName = directoryName,
            WorldName = worldName,
            Width = decimal.ToInt32(widthBox.Value ?? 0),
            Height = decimal.ToInt32(heightBox.Value ?? 0),
            Fog = fog,
            FogPower = fogEditor.Power,
            FogOx = fogEditor.Ox,
            FogOy = fogEditor.Oy,
            FogDistort = fogEditor.Distort,
            Panorama = panorama,
        });
    }

    private static bool isValidDirectoryName(string value)
    {
        return !string.IsNullOrWhiteSpace(value)
            && value is not "." and not ".."
            && value.IndexOfAny(Path.GetInvalidFileNameChars()) < 0
            && !value.Contains('/')
            && !value.Contains('\\')
            && !string.Equals(value, "_world", StringComparison.OrdinalIgnoreCase);
    }

}
