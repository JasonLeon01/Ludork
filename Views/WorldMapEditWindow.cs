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
    private readonly GameDataService gameData;
    private readonly bool isNew;
    private readonly TextBox directoryNameBox = EditorInputs.CreateEditableTextBox();
    private readonly TextBox worldNameBox = EditorInputs.CreateEditableTextBox();
    private readonly NumericUpDown widthBox = EditorInputs.CreateNumericUpDown(256, 1, 32768, 1);
    private readonly NumericUpDown heightBox = EditorInputs.CreateNumericUpDown(192, 1, 32768, 1);
    private readonly TextBox fogBox = EditorInputs.CreateReadOnlyTextBox();
    private readonly NumericUpDown fogPowerBox = EditorInputs.CreateNumericUpDown(0, 0, 100, 1);
    private readonly NumericUpDown fogOxBox = EditorInputs.CreateNumericUpDown(0, -9999, 9999, 1);
    private readonly NumericUpDown fogOyBox = EditorInputs.CreateNumericUpDown(0, -9999, 9999, 1);
    private readonly NumericUpDown fogDistortBox = EditorInputs.CreateNumericUpDown(0, 0, 100, 1);
    private readonly TextBlock errorText = new()
    {
        Foreground = Brushes.IndianRed,
        TextWrapping = TextWrapping.Wrap,
    };
    private readonly StackPanel fogOptions = new() { Spacing = 8 };

    private WorldMapEditWindow(
        GameDataService gameData,
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

        directoryNameBox.Text = initial.DirectoryName;
        worldNameBox.Text = initial.WorldName;
        widthBox.Value = initial.Width;
        heightBox.Value = initial.Height;
        fogBox.Text = initial.Fog;
        fogPowerBox.Value = initial.FogPower;
        fogOxBox.Value = (decimal)initial.FogOx;
        fogOyBox.Value = (decimal)initial.FogOy;
        fogDistortBox.Value = initial.FogDistort;

        Grid form = new() { RowSpacing = 8 };
        if (isNew)
            addRow(form, LocaleService.Get("WORLD_FOLDER_NAME"), directoryNameBox);
        addRow(form, LocaleService.Get("WORLD_NAME"), worldNameBox);
        addRow(form, LocaleService.Get("MAP_WIDTH"), widthBox);
        addRow(form, LocaleService.Get("MAP_HEIGHT"), heightBox);
        addRow(form, LocaleService.Get("MAP_FOG"), createFileRow());
        fogOptions.Children.Add(createRow(LocaleService.Get("MAP_FOG_POWER"), fogPowerBox));
        fogOptions.Children.Add(createRow(LocaleService.Get("MAP_FOG_OX"), fogOxBox));
        fogOptions.Children.Add(createRow(LocaleService.Get("MAP_FOG_OY"), fogOyBox));
        fogOptions.Children.Add(createRow(LocaleService.Get("MAP_FOG_DISTORT"), fogDistortBox));
        addRow(form, string.Empty, fogOptions);
        fogBox.TextChanged += (_, _) => updateFogVisibility();
        updateFogVisibility();

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
        GameDataService gameData,
        WorldMapInfo initial,
        bool isNew)
    {
        return new WorldMapEditWindow(gameData, initial, isNew).ShowDialog<WorldMapInfo?>(owner);
    }

    private Control createFileRow()
    {
        Button browse = new() { Content = "...", MinWidth = 36 };
        browse.Click += async (_, _) => await selectFogAsync();
        Button clear = new() { Content = LocaleService.Get("CLEAR") };
        clear.Click += (_, _) => fogBox.Text = string.Empty;
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto,Auto"),
            ColumnSpacing = 6,
        };
        row.Children.Add(fogBox);
        Grid.SetColumn(browse, 1);
        row.Children.Add(browse);
        Grid.SetColumn(clear, 2);
        row.Children.Add(clear);
        return row;
    }

    private async Task selectFogAsync()
    {
        string root = Path.Combine(gameData.ProjectPath, "Assets", "Fogs");
        Directory.CreateDirectory(root);
        string current = fogBox.Text ?? string.Empty;
        string? initialFilePath = string.IsNullOrWhiteSpace(current)
            ? null
            : Path.Combine(root, current);
        string? path = await FileSelectorDialog.ShowAsync(
            this,
            root,
            FileSelectorDialog.ImageFilesFilter(),
            initialFilePath: initialFilePath);
        if (path is not null)
            fogBox.Text = Path.GetFileName(path);
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
            && (gameData.WorldMapData.ContainsKey(directoryName)
                || gameData.MapCatalog.Any(item => string.Equals(item.Key, directoryName, StringComparison.Ordinal))))
        {
            errorText.Text = LocaleService.Get("WORLD_FOLDER_EXISTS");
            return;
        }
        if (worldName.Length == 0)
        {
            errorText.Text = LocaleService.Get("WORLD_NAME_EMPTY");
            return;
        }
        Close(new WorldMapInfo
        {
            DirectoryName = directoryName,
            WorldName = worldName,
            Width = decimal.ToInt32(widthBox.Value ?? 0),
            Height = decimal.ToInt32(heightBox.Value ?? 0),
            Fog = fogBox.Text?.Trim() ?? string.Empty,
            FogPower = decimal.ToInt32(fogPowerBox.Value ?? 0),
            FogOx = (double)(fogOxBox.Value ?? 0),
            FogOy = (double)(fogOyBox.Value ?? 0),
            FogDistort = decimal.ToInt32(fogDistortBox.Value ?? 0),
        });
    }

    private void updateFogVisibility()
    {
        fogOptions.IsVisible = !string.IsNullOrWhiteSpace(fogBox.Text);
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

    private static void addRow(Grid form, string label, Control editor)
    {
        int rowIndex = form.RowDefinitions.Count;
        form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        Grid row = createRow(label, editor);
        Grid.SetRow(row, rowIndex);
        form.Children.Add(row);
    }

    private static Grid createRow(string label, Control editor)
    {
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions("160,*"),
            ColumnSpacing = 12,
        };
        row.Children.Add(new TextBlock
        {
            Text = label,
            VerticalAlignment = VerticalAlignment.Center,
            TextWrapping = TextWrapping.Wrap,
        });
        Grid.SetColumn(editor, 1);
        row.Children.Add(editor);
        return row;
    }
}
