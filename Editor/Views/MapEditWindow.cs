using Avalonia;
using Avalonia.Automation;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.IO;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class MapEditWindow : Window
{
    private readonly ProjectDataStore gameData;
    private readonly string currentKey;
    private readonly string keyPrefix;
    private readonly bool isNew;
    private readonly MapFogEditor fogEditor;
    private readonly TextBox fileNameBox = EditorInputs.CreateEditableTextBox();
    private readonly TextBox mapNameBox = EditorInputs.CreateEditableTextBox();
    private readonly NumericUpDown widthBox = EditorInputs.CreateNumericUpDown(13, 1, 32768, 1);
    private readonly NumericUpDown heightBox = EditorInputs.CreateNumericUpDown(13, 1, 32768, 1);
    private readonly TextBox bgmBox = EditorInputs.CreateReadOnlyTextBox();
    private readonly TextBox bgsBox = EditorInputs.CreateReadOnlyTextBox();
    private readonly TextBlock errorText = new() { Foreground = Brushes.IndianRed, TextWrapping = TextWrapping.Wrap };
    private readonly Button ambientButton = new();
    private readonly Border ambientSwatch = new();
    private readonly TextBlock ambientValue = new() { VerticalAlignment = VerticalAlignment.Center };
    private JsonObject bgmFilter;
    private JsonObject bgsFilter;
    private Color ambientColor;

    private MapEditWindow(
        ProjectDataStore gameData,
        MapInfo initial,
        string currentKey,
        bool isNew,
        string? keyPrefix)
    {
        this.gameData = gameData;
        this.currentKey = currentKey;
        this.keyPrefix = keyPrefix?.Trim().Trim('/') ?? string.Empty;
        this.isNew = isNew;
        bgmFilter = cloneObject(initial.BgmFilter);
        bgsFilter = cloneObject(initial.BgsFilter);
        ambientColor = getAmbientColor(initial.AmbientLight);

        Title = LocaleService.Get(isNew ? "NEW_MAP" : "MAPLIST_EDIT");
        Width = 640;
        Height = 560;
        MinWidth = 540;
        MinHeight = 300;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);
        EditorLayoutService.AttachWindow(this, nameof(MapEditWindow));

        fileNameBox.Text = initial.FileName;
        mapNameBox.Text = initial.MapName;
        widthBox.Value = initial.Width;
        heightBox.Value = initial.Height;
        bgmBox.Text = initial.Bgm;
        bgsBox.Text = initial.Bgs;
        fogEditor = new MapFogEditor(MapVisualSettings.From(initial));
        ambientButton.Content = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 8,
            Children =
            {
                new Border
                {
                    Width = 28,
                    Height = 18,
                    BorderBrush = EditorTheme.Brush("Border"),
                    BorderThickness = new Thickness(1),
                    ClipToBounds = true,
                    Child = new Grid
                    {
                        Children = { new BlueprintCheckerboard { CellSize = 4 }, ambientSwatch },
                    },
                },
                ambientValue,
            },
        };
        updateAmbientButton();

        Grid form = new() { RowSpacing = 8 };
        EditorFormRows.Add(form, LocaleService.Get("FILE_NAME"), fileNameBox);
        EditorFormRows.Add(form, LocaleService.Get("EDIT_MAP"), mapNameBox);
        EditorFormRows.Add(form, LocaleService.Get("MAP_WIDTH"), widthBox);
        EditorFormRows.Add(form, LocaleService.Get("MAP_HEIGHT"), heightBox);
        ambientButton.Click += onPickAmbient;
        EditorFormRows.Add(form, LocaleService.Get("AMBIENT_LIGHT"), ambientButton);
        EditorFormRows.Add(form, LocaleService.Get("MAP_BGM"), createFileRow(bgmBox, "Musics", true));
        EditorFormRows.Add(form, LocaleService.Get("MAP_BGS"), createFileRow(bgsBox, "Musics", false));
        EditorFormRows.Add(form, LocaleService.Get("MAP_PANORAMA"), createFileRow(fogEditor.PanoramaBox, "Panoramas", null));
        EditorFormRows.Add(form, LocaleService.Get("MAP_FOG"), createFileRow(fogEditor.PathBox, "Fogs", null));
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
        Content = new ScrollViewer { Content = content, VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
        Opened += (_, _) => fileNameBox.Focus();
    }

    public static Task<MapInfo?> ShowAsync(
        Window owner,
        ProjectDataStore gameData,
        MapInfo initial,
        string currentKey,
        bool isNew,
        string? keyPrefix = null)
    {
        return new MapEditWindow(gameData, initial, currentKey, isNew, keyPrefix).ShowDialog<MapInfo?>(owner);
    }

    private Control createFileRow(TextBox textBox, string rootName, bool? isBgm)
    {
        Button browse = new() { Content = "...", MinWidth = 36 };
        browse.Click += async (_, _) => await selectFileAsync(
            textBox,
            rootName,
            rootName is "Fogs" or "Panoramas"
                ? FileSelectorDialog.ImageFilesFilter()
                : FileSelectorDialog.AudioFilesFilter());
        Grid row = new() { ColumnDefinitions = new ColumnDefinitions("*,Auto"), ColumnSpacing = 6 };
        row.Children.Add(textBox);
        Grid.SetColumn(browse, 1);
        row.Children.Add(browse);
        if (isBgm is null)
            return row;
        Button filter = new() { Content = LocaleService.Get("FILTER") };
        filter.Click += async (_, _) => await editFilterAsync(isBgm.Value);
        row.ColumnDefinitions.Add(new ColumnDefinition(GridLength.Auto));
        Grid.SetColumn(filter, 2);
        row.Children.Add(filter);
        return row;
    }

    private async Task selectFileAsync(TextBox target, string rootName, string filterStr)
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
            filterStr,
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

    private async Task editFilterAsync(bool isBgm)
    {
        JsonObject? result = await MapAudioFilterWindow.ShowAsync(this, isBgm ? bgmFilter : bgsFilter, isBgm);
        if (result is null)
            return;
        if (isBgm)
            bgmFilter = result;
        else
            bgsFilter = result;
    }

    private async void onPickAmbient(object? sender, RoutedEventArgs args)
    {
        Color? result = await MapColourPickerWindow.ShowAsync(this, ambientColor);
        if (result is not Color color)
            return;
        ambientColor = color;
        updateAmbientButton();
    }

    private void onConfirm(object? sender, RoutedEventArgs args)
    {
        string fileName = fileNameBox.Text?.Trim() ?? string.Empty;
        string key = normaliseMapKey(fileName);
        if (string.IsNullOrWhiteSpace(key))
        {
            errorText.Text = LocaleService.Get("MAP_FILE_NAME_EMPTY");
            return;
        }
        if (keyPrefix.Length != 0 && key.Contains('/'))
        {
            errorText.Text = LocaleService.Get("WORLD_CHILD_FILE_NAME_INVALID");
            return;
        }
        string lookupKey = keyPrefix.Length == 0 ? key : keyPrefix + "/" + key;
        if (gameData.Maps.MapData.ContainsKey(lookupKey)
            && (isNew || !string.Equals(lookupKey, currentKey, StringComparison.Ordinal)))
        {
            errorText.Text = LocaleService.Get("MAP_FILE_NAME_EXISTS");
            return;
        }
        foreach (string assetPath in new[]
                 {
                     bgmBox.Text ?? string.Empty,
                     bgsBox.Text ?? string.Empty,
                     fogEditor.PathBox.Text ?? string.Empty,
                     fogEditor.PanoramaBox.Text ?? string.Empty,
                 })
        {
            if (assetPath.Length != 0 && !GameAssetPath.IsCanonical(assetPath))
            {
                errorText.Text = $"Invalid game asset path: {assetPath}";
                return;
            }
        }
        Close(new MapInfo
        {
            FileName = fileName,
            MapName = mapNameBox.Text?.Trim() ?? string.Empty,
            Width = getIntValue(widthBox),
            Height = getIntValue(heightBox),
            AmbientLight = new JsonArray(
                (int)ambientColor.R,
                (int)ambientColor.G,
                (int)ambientColor.B,
                (int)ambientColor.A),
            Bgm = bgmBox.Text?.Trim() ?? string.Empty,
            BgmFilter = cloneObject(bgmFilter),
            Bgs = bgsBox.Text?.Trim() ?? string.Empty,
            BgsFilter = cloneObject(bgsFilter),
            Fog = fogEditor.PathBox.Text?.Trim() ?? string.Empty,
            FogPower = fogEditor.Power,
            FogOx = fogEditor.Ox,
            FogOy = fogEditor.Oy,
            FogDistort = fogEditor.Distort,
            Panorama = fogEditor.PanoramaBox.Text?.Trim() ?? string.Empty,
        });
    }

    private void updateAmbientButton()
    {
        ambientSwatch.Background = new SolidColorBrush(ambientColor);
        ambientValue.Text = $"#{ambientColor.A:X2}{ambientColor.R:X2}{ambientColor.G:X2}{ambientColor.B:X2}";
        AutomationProperties.SetName(ambientButton, $"{LocaleService.Get("AMBIENT_LIGHT")} {ambientValue.Text}");
    }

    private static int getIntValue(NumericUpDown number)
    {
        return decimal.ToInt32(number.Value ?? 0);
    }

    private static JsonObject cloneObject(JsonObject value) => (JsonObject)value.DeepClone();

    private static Color getAmbientColor(JsonArray values)
    {
        return Color.FromArgb(getColorComponent(values, 3), getColorComponent(values, 0), getColorComponent(values, 1), getColorComponent(values, 2));
    }

    private static byte getColorComponent(JsonArray values, int index)
    {
        return (byte)Math.Clamp(values[index]?.GetValue<int?>() ?? 255, 0, 255);
    }

    private static string normaliseMapKey(string fileName)
    {
        string key = fileName.Replace('\\', '/').Trim().Trim('/');
        return key.EndsWith(".json", StringComparison.OrdinalIgnoreCase) ? key[..^5] : key;
    }

}
