using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;

namespace Ludork.Views;

internal sealed class MaterialCombinationWindow : Window
{
    private readonly MaterialCombinationService service;
    private readonly HashSet<string> materialKeys;
    private readonly ListBox materialList;
    private readonly ListBox layerList;
    private readonly ListBox combinationList;
    private readonly Button addButton;
    private readonly Button deleteButton;
    private readonly TextBlock error = new()
    {
        IsVisible = false,
        Foreground = EditorTheme.Brush("Error"),
        TextWrapping = TextWrapping.Wrap,
        Margin = new Thickness(0, 12, 0, 0),
    };
    private List<MaterialCombination> combinations;
    private bool saving;

    private MaterialCombinationWindow(
        MaterialCombinationService service,
        IEnumerable<string> materialKeys,
        List<MaterialCombination> combinations)
    {
        this.service = service;
        this.materialKeys = new HashSet<string>(materialKeys, StringComparer.Ordinal);
        this.combinations = combinations;
        Title = LocaleService.Get("MATERIAL_COMBINATIONS");
        Width = 980;
        Height = 540;
        MinWidth = 720;
        MinHeight = 320;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = EditorTheme.Brush("Background");
        FontFamily = EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);

        MaterialOption[] options = this.materialKeys.OrderBy(key => key, StringComparer.Ordinal)
            .Select(key => new MaterialOption(GameDataPath.Materials + "/" + key + ".json", key))
            .ToArray();
        materialList = createList<MaterialOption>(value => value.Name);
        materialList.ItemsSource = new[] { new MaterialOption(string.Empty, LocaleService.Get("MATERIAL_DEFAULT")) }
            .Concat(options).ToArray();
        layerList = createList<MaterialOption>(value => value.Name);
        layerList.ItemsSource = options;
        combinationList = createList<MaterialCombination>(getCombinationName);
        combinationList.ItemsSource = combinations;
        materialList.SelectionChanged += (_, _) => updateButtons();
        layerList.SelectionChanged += (_, _) => updateButtons();
        combinationList.SelectionChanged += (_, _) => updateButtons();

        addButton = new Button
        {
            Content = LocaleService.Get("MATERIAL_COMBINATION_ADD"),
            HorizontalAlignment = HorizontalAlignment.Stretch,
            IsEnabled = false,
            MinWidth = 88,
        };
        deleteButton = new Button
        {
            Content = LocaleService.Get("DELETE"),
            HorizontalAlignment = HorizontalAlignment.Stretch,
            IsEnabled = false,
        };
        addButton.Click += async (_, _) => await addAsync();
        deleteButton.Click += async (_, _) => await deleteAsync();
        StackPanel actions = new() { Spacing = 8, Children = { addButton, deleteButton } };
        Grid lists = new() { ColumnDefinitions = new ColumnDefinitions("*,12,*,12,2*,12,Auto") };
        lists.Children.Add(createColumn(LocaleService.Get("MATERIAL_COMBINATION_MATERIAL"), materialList));
        Control layerColumn = createColumn(LocaleService.Get("MATERIAL_COMBINATION_LAYER"), layerList);
        Grid.SetColumn(layerColumn, 2);
        lists.Children.Add(layerColumn);
        Control combinationColumn = createColumn(LocaleService.Get("MATERIAL_COMBINATION_EXISTING"), combinationList);
        Grid.SetColumn(combinationColumn, 4);
        lists.Children.Add(combinationColumn);
        Grid.SetColumn(actions, 6);
        actions.Margin = new Thickness(0, 28, 0, 0);
        lists.Children.Add(actions);
        Grid content = new() { Margin = new Thickness(20), RowDefinitions = new RowDefinitions("*,Auto") };
        content.Children.Add(lists);
        Grid.SetRow(error, 1);
        content.Children.Add(error);
        Content = content;
        Closing += (_, args) => args.Cancel = saving;
        KeyDown += (_, args) =>
        {
            if (args.Key != Key.Escape || saving)
                return;
            Close();
            args.Handled = true;
        };
    }

    public static async Task ShowAsync(Window owner, ProjectDataStore gameData)
    {
        MaterialCombinationService service = new(gameData.ProjectPath);
        List<MaterialCombination> combinations;
        try
        {
            combinations = service.Read();
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or InvalidDataException or JsonException)
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"),
                LocaleService.Get("MATERIAL_COMBINATIONS_LOAD_FAILED") + Environment.NewLine + exception.Message);
            return;
        }
        await new MaterialCombinationWindow(service, gameData.Assets.MaterialsData.Keys, combinations).ShowDialog(owner);
    }

    private static ListBox createList<T>(Func<T, string> getName) where T : class
    {
        return new ListBox
        {
            Background = EditorTheme.Brush("Surface"),
            SelectionMode = SelectionMode.Single,
            ItemTemplate = new FuncDataTemplate<T>((value, _) => new HintedTextPresenter
            {
                Text = value is null ? string.Empty : getName(value),
            }),
        };
    }

    private static Grid createColumn(string title, ListBox list)
    {
        Grid column = new() { RowDefinitions = new RowDefinitions("Auto,*") };
        column.Children.Add(new TextBlock { Text = title, Margin = new Thickness(0, 0, 0, 8) });
        Grid.SetRow(list, 1);
        column.Children.Add(list);
        return column;
    }

    private string getCombinationName(MaterialCombination combination)
    {
        return getMaterialName(combination.Material) + " + " + getMaterialName(combination.Layer);
    }

    private string getMaterialName(string path)
    {
        if (path.Length == 0)
            return LocaleService.Get("MATERIAL_DEFAULT");
        GameDataPath.TryGetMaterialKey(path, out string key);
        return materialKeys.Contains(key) ? key : string.Format(LocaleService.Get("MATERIAL_COMBINATION_MISSING"), key);
    }

    private MaterialCombination? selectedCombination()
    {
        return materialList.SelectedItem is MaterialOption material && layerList.SelectedItem is MaterialOption layer
            ? new MaterialCombination(material.Path, layer.Path)
            : null;
    }

    private void updateButtons()
    {
        MaterialCombination? candidate = selectedCombination();
        addButton.IsEnabled = !saving && candidate is not null && !combinations.Contains(candidate);
        deleteButton.IsEnabled = !saving && combinationList.SelectedItem is MaterialCombination;
    }

    private async Task addAsync()
    {
        MaterialCombination? candidate = selectedCombination();
        if (saving || candidate is null || combinations.Contains(candidate))
            return;
        List<MaterialCombination> next = combinations.Append(candidate)
            .OrderBy(value => value.Material, StringComparer.Ordinal)
            .ThenBy(value => value.Layer, StringComparer.Ordinal)
            .ToList();
        await saveAsync(next, candidate);
    }

    private async Task deleteAsync()
    {
        if (saving || combinationList.SelectedItem is not MaterialCombination selected)
            return;
        int selectedIndex = combinationList.SelectedIndex;
        List<MaterialCombination> next = combinations.Where(value => value != selected).ToList();
        MaterialCombination? selection = next.Count == 0 ? null : next[Math.Min(selectedIndex, next.Count - 1)];
        await saveAsync(next, selection);
    }

    private async Task saveAsync(List<MaterialCombination> next, MaterialCombination? selection)
    {
        saving = true;
        error.IsVisible = false;
        materialList.IsEnabled = false;
        layerList.IsEnabled = false;
        combinationList.IsEnabled = false;
        updateButtons();
        try
        {
            await service.SaveAsync(next);
            combinations = next;
            combinationList.ItemsSource = next;
            combinationList.SelectedItem = selection;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            error.Text = LocaleService.Get("MATERIAL_COMBINATIONS_SAVE_FAILED") + Environment.NewLine + exception.Message;
            error.IsVisible = true;
        }
        finally
        {
            saving = false;
            materialList.IsEnabled = true;
            layerList.IsEnabled = true;
            combinationList.IsEnabled = true;
            updateButtons();
        }
    }

    private sealed record MaterialOption(string Path, string Name);
}
