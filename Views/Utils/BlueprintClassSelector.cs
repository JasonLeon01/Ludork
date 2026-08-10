using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public enum BlueprintClassSelectorMode
{
    Parent,
    NodeParameter,
}

public static class BlueprintClassSelector
{
    private const string BlueprintPrefix = "Data.Blueprints.";
    private const string ActorRoot = "Engine.Actor";
    private const string InfoRoot = "Engine.InfoBase";

    public static Task<string?> ShowAsync(
        Window owner,
        GameDataService gameData,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver,
        string current,
        string? blueprintKey,
        BlueprintClassSelectorMode mode)
    {
        BlueprintClassOptions options = buildOptions(
            gameData,
            metadataService,
            classResolver,
            blueprintKey,
            mode);
        string initial = current;
        if (!current.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
        {
            ResolvedBlueprintClass resolved = classResolver.Resolve(current);
            if (resolved.RootType is not null)
                initial = resolved.RootType.QualifiedName;
        }
        return BlueprintClassSelectorWindow.ShowAsync(owner, options, initial);
    }

    private static BlueprintClassOptions buildOptions(
        GameDataService gameData,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver,
        string? blueprintKey,
        BlueprintClassSelectorMode mode)
    {
        HashSet<string> classes = new(StringComparer.Ordinal);
        foreach (string typeName in BlueprintCompatibilityCatalog.GetTypeNames())
            addClass(classes, typeName, classResolver);
        foreach (LuaTypeMetadata metadata in metadataService.EnumerateTypes())
            addClass(classes, metadata.Type.QualifiedName, classResolver);

        List<string> blueprints = [];
        foreach (string key in gameData.BlueprintsData.Keys.OrderBy(value => value, StringComparer.Ordinal))
        {
            if (mode == BlueprintClassSelectorMode.Parent
                && (string.Equals(key, blueprintKey, StringComparison.Ordinal)
                    || createsCycle(gameData, key, blueprintKey)))
            {
                continue;
            }
            string reference = BlueprintPrefix + key.Replace('/', '.').Replace('\\', '.');
            if (isBlueprintClass(reference, classResolver))
                blueprints.Add(reference);
        }
        return new BlueprintClassOptions(
            classes.OrderBy(value => value, StringComparer.Ordinal).ToArray(),
            blueprints);
    }

    private static void addClass(
        ISet<string> classes,
        string typeName,
        BlueprintClassResolver classResolver)
    {
        ResolvedBlueprintClass resolved = classResolver.Resolve(typeName);
        string qualifiedName = resolved.RootType?.QualifiedName ?? typeName;
        LuaTypeReference type = LuaTypeReference.Parse(qualifiedName);
        if (!type.TypeName.StartsWith('_') && isBlueprintClass(qualifiedName, classResolver))
            classes.Add(qualifiedName);
    }

    private static bool isBlueprintClass(
        string reference,
        BlueprintClassResolver classResolver)
    {
        return classResolver.IsDerivedFrom(reference, ActorRoot)
            || classResolver.IsDerivedFrom(reference, InfoRoot);
    }

    private static bool createsCycle(
        GameDataService gameData,
        string candidateKey,
        string? currentBlueprintKey)
    {
        if (string.IsNullOrWhiteSpace(currentBlueprintKey))
            return false;
        HashSet<string> visited = new(StringComparer.Ordinal);
        string? key = candidateKey;
        while (!string.IsNullOrWhiteSpace(key) && visited.Add(key))
        {
            if (string.Equals(key, currentBlueprintKey, StringComparison.Ordinal))
                return true;
            if (!gameData.BlueprintsData.TryGetValue(key, out JsonObject? blueprint)
                || blueprint["parent"] is not JsonValue parentValue
                || !parentValue.TryGetValue(out string? parent)
                || string.IsNullOrWhiteSpace(parent)
                || !parent.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
            {
                return false;
            }
            key = BlueprintEditorDocument.NormalizeBlueprintKey(parent);
        }
        return false;
    }
}

internal sealed record BlueprintClassOptions(
    IReadOnlyList<string> Classes,
    IReadOnlyList<string> Blueprints);

internal sealed class BlueprintClassSelectorWindow : Window
{
    private readonly BlueprintClassOptions options;
    private readonly TextBox searchBox;
    private readonly ListBox classList;
    private readonly ListBox blueprintList;
    private readonly Button confirmButton;
    private bool changingSelection;

    private BlueprintClassSelectorWindow(BlueprintClassOptions options, string initial)
    {
        this.options = options;
        Title = LocaleService.Get("CLASS_SELECTOR");
        Width = 800;
        Height = 560;
        MinWidth = 500;
        MinHeight = 300;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);

        searchBox = EditorInputs.CreateEditableTextBox();
        searchBox.PlaceholderText = LocaleService.Get("SEARCH");
        searchBox.TextChanged += (_, _) => rebuildLists();
        classList = createList();
        blueprintList = createList();
        classList.SelectionChanged += (_, _) => selectFrom(classList, blueprintList);
        blueprintList.SelectionChanged += (_, _) => selectFrom(blueprintList, classList);
        classList.DoubleTapped += (_, _) => confirm();
        blueprintList.DoubleTapped += (_, _) => confirm();

        confirmButton = new Button
        {
            Content = LocaleService.Get("CONFIRM"),
            MinWidth = 80,
            IsEnabled = false,
        };
        confirmButton.Click += (_, _) => confirm();
        Button cancelButton = new()
        {
            Content = LocaleService.Get("CANCEL"),
            MinWidth = 80,
        };
        cancelButton.Click += (_, _) => Close(null);

        Grid lists = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,8,*"),
        };
        lists.Children.Add(createListColumn(LocaleService.Get("PROJECT_CLASSES"), classList));
        Control blueprintColumn = createListColumn(LocaleService.Get("PROJECT_BLUEPRINT"), blueprintList);
        Grid.SetColumn(blueprintColumn, 2);
        lists.Children.Add(blueprintColumn);
        StackPanel actions = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { confirmButton, cancelButton },
        };
        Grid content = new()
        {
            Margin = new Thickness(12),
            RowDefinitions = new RowDefinitions("Auto,8,*,8,Auto"),
        };
        content.Children.Add(searchBox);
        Grid.SetRow(lists, 2);
        content.Children.Add(lists);
        Grid.SetRow(actions, 4);
        content.Children.Add(actions);
        Content = content;

        KeyDown += onKeyDown;
        Opened += (_, _) => searchBox.Focus();
        rebuildLists();
        if (options.Blueprints.Contains(initial, StringComparer.Ordinal))
            blueprintList.SelectedItem = initial;
        else if (options.Classes.Contains(initial, StringComparer.Ordinal))
            classList.SelectedItem = initial;
    }

    public static Task<string?> ShowAsync(
        Window owner,
        BlueprintClassOptions options,
        string initial)
    {
        BlueprintClassSelectorWindow window = new(options, initial);
        return window.ShowDialog<string?>(owner);
    }

    private static ListBox createList()
    {
        return new ListBox
        {
            HorizontalAlignment = HorizontalAlignment.Stretch,
            VerticalAlignment = VerticalAlignment.Stretch,
            ItemTemplate = HintedTextPresenter.StringItemTemplate,
        };
    }

    private static Control createListColumn(string title, ListBox list)
    {
        Grid column = new()
        {
            RowDefinitions = new RowDefinitions("Auto,4,*"),
        };
        column.Children.Add(new TextBlock { Text = title });
        Grid.SetRow(list, 2);
        column.Children.Add(list);
        return column;
    }

    private void rebuildLists()
    {
        string search = searchBox.Text?.Trim() ?? string.Empty;
        string? selected = getSelection();
        string[] classes = options.Classes
            .Where(value => value.Contains(search, StringComparison.OrdinalIgnoreCase))
            .ToArray();
        string[] blueprints = options.Blueprints
            .Where(value => value.Contains(search, StringComparison.OrdinalIgnoreCase))
            .ToArray();
        changingSelection = true;
        classList.ItemsSource = classes;
        blueprintList.ItemsSource = blueprints;
        classList.SelectedItem = selected is not null && classes.Contains(selected, StringComparer.Ordinal)
            ? selected
            : null;
        blueprintList.SelectedItem = selected is not null && blueprints.Contains(selected, StringComparer.Ordinal)
            ? selected
            : null;
        changingSelection = false;
        updateConfirmState();
    }

    private void selectFrom(ListBox source, ListBox other)
    {
        if (changingSelection)
            return;
        changingSelection = true;
        if (source.SelectedItem is string)
            other.SelectedItem = null;
        changingSelection = false;
        updateConfirmState();
    }

    private string? getSelection()
    {
        return classList.SelectedItem as string ?? blueprintList.SelectedItem as string;
    }

    private void updateConfirmState()
    {
        confirmButton.IsEnabled = getSelection() is not null;
    }

    private void confirm()
    {
        if (getSelection() is string selected && selected.Length != 0)
            Close(selected);
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.Escape)
        {
            Close(null);
            args.Handled = true;
        }
        else if (args.Key == Key.Enter)
        {
            confirm();
            args.Handled = true;
        }
    }
}
