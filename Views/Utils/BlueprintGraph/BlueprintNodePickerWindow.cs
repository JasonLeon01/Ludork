using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed class BlueprintNodePickerWindow : Window
{
    private readonly IReadOnlyList<BlueprintGraphNodeDefinition> definitions;
    private readonly TextBox searchBox;
    private readonly ListBox itemList;
    private readonly CheckBox contextSensitiveBox;
    private readonly ObservableCollection<BlueprintNodePickerRow> rows = [];
    private readonly HashSet<string> expandedGroups = new(StringComparer.Ordinal);
    private readonly DispatcherTimer deactivateTimer;
    private TaskCompletionSource<BlueprintGraphNodeDefinition?>? completion;
    private BlueprintGraphNodeDefinition? result;
    private bool canCloseOnDeactivate;

    private BlueprintNodePickerWindow(
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions,
        PixelPoint position)
    {
        this.definitions = definitions;
        Position = position;
        Width = 320;
        Height = 420;
        MinWidth = 260;
        MinHeight = 280;
        WindowStartupLocation = WindowStartupLocation.Manual;
        WindowDecorations = Avalonia.Controls.WindowDecorations.None;
        CanResize = true;
        ShowInTaskbar = false;
        Topmost = true;
        Background = new SolidColorBrush(Color.Parse("#2b2b2b"));
        FontFamily = FontFamily.Parse("avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");

        searchBox = EditorInputs.CreateEditableTextBox();
        searchBox.PlaceholderText = LocaleService.Get("SEARCH");
        searchBox.TextChanged += (_, _) => rebuildRows();

        itemList = new ListBox
        {
            ItemsSource = rows,
            Background = new SolidColorBrush(Color.Parse("#232323")),
            BorderBrush = new SolidColorBrush(Color.Parse("#464646")),
            BorderThickness = new Thickness(1),
            ItemTemplate = new FuncDataTemplate<BlueprintNodePickerRow>(createRow),
        };
        itemList.DoubleTapped += onItemDoubleTapped;

        contextSensitiveBox = new CheckBox
        {
            Content = LocaleService.Get("CONTEXT_SENSITIVE"),
            IsChecked = true,
        };
        contextSensitiveBox.IsCheckedChanged += (_, _) => rebuildRows();

        Grid content = new()
        {
            Margin = new Thickness(12),
            RowDefinitions = new RowDefinitions("Auto,10,*,10,Auto"),
        };
        Grid.SetRow(searchBox, 0);
        content.Children.Add(searchBox);
        Grid.SetRow(itemList, 2);
        content.Children.Add(itemList);
        Grid.SetRow(contextSensitiveBox, 4);
        content.Children.Add(contextSensitiveBox);
        Content = content;

        deactivateTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(150),
        };
        deactivateTimer.Tick += onDeactivateTimer;
        Opened += onOpened;
        Deactivated += onDeactivated;
        KeyDown += onKeyDown;
        rebuildRows();
    }

    public static Task<BlueprintGraphNodeDefinition?> ShowAsync(
        Window owner,
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions,
        PixelPoint position)
    {
        BlueprintNodePickerWindow window = new(definitions, position);
        return window.showAsync(owner);
    }

    private Task<BlueprintGraphNodeDefinition?> showAsync(Window owner)
    {
        completion = new TaskCompletionSource<BlueprintGraphNodeDefinition?>(
            TaskCreationOptions.RunContinuationsAsynchronously);
        Closed += onClosed;
        Show(owner);
        return completion.Task;
    }

    private Control? createRow(BlueprintNodePickerRow? row, INameScope? scope)
    {
        if (row is null)
            return null;
        Grid content = new()
        {
            ColumnDefinitions = new ColumnDefinitions("24,*"),
            Margin = new Thickness(row.Depth * 16, 2, 4, 2),
            MinHeight = 28,
        };
        if (row.IsGroup)
        {
            Button toggle = new()
            {
                Content = row.IsExpanded ? "▼" : "▶",
                Width = 22,
                Height = 22,
                Padding = new Thickness(0),
                Background = Brushes.Transparent,
                BorderThickness = new Thickness(0),
                Focusable = false,
            };
            toggle.Click += (_, _) => toggleGroup(row);
            content.Children.Add(toggle);
        }
        TextBlock title = new()
        {
            Text = row.DisplayText,
            TextTrimming = TextTrimming.CharacterEllipsis,
            VerticalAlignment = VerticalAlignment.Center,
            Foreground = row.IsGroup ? Brushes.White : new SolidColorBrush(Color.Parse("#eeeeee")),
        };
        Grid.SetColumn(title, 1);
        content.Children.Add(title);
        return content;
    }

    private void rebuildRows()
    {
        string search = searchBox.Text?.Trim() ?? string.Empty;
        bool contextSensitive = contextSensitiveBox.IsChecked == true;
        BlueprintGraphNodeDefinition[] filtered = definitions
            .Where(definition => !contextSensitive || definition.IsContextRelevant)
            .ToArray();
        rows.Clear();
        if (!string.IsNullOrWhiteSpace(search))
        {
            foreach (BlueprintGraphNodeDefinition definition in filtered
                .Where(definition => definition.MemberName.Contains(search, StringComparison.OrdinalIgnoreCase)
                    || definition.Title.Contains(search, StringComparison.OrdinalIgnoreCase))
                .OrderBy(definition => definition.Title, StringComparer.Ordinal)
                .ThenBy(definition => definition.RuntimePath, StringComparer.Ordinal))
            {
                string hierarchy = string.Join(
                    '.',
                    definition.PickerPath.Select(EditorDisplayName.Format));
                string display = string.IsNullOrWhiteSpace(hierarchy)
                    ? definition.Title
                    : $"{definition.Title} ({hierarchy})";
                rows.Add(new BlueprintNodePickerRow(display, 0, definition));
            }
            itemList.SelectedItem = rows.FirstOrDefault();
            return;
        }

        IReadOnlyList<BlueprintNodePickerTreeItem> roots = buildTree(filtered);
        foreach (BlueprintNodePickerTreeItem root in roots)
            appendVisibleRows(root, 0);
    }

    private static IReadOnlyList<BlueprintNodePickerTreeItem> buildTree(
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions)
    {
        List<BlueprintNodePickerTreeItem> roots = [];
        foreach (BlueprintGraphNodeDefinition definition in definitions)
        {
            List<BlueprintNodePickerTreeItem> children = roots;
            string groupPath = string.Empty;
            foreach (string segment in definition.PickerPath)
            {
                groupPath = string.IsNullOrWhiteSpace(groupPath)
                    ? segment
                    : groupPath + "." + segment;
                BlueprintNodePickerTreeItem? group = children.FirstOrDefault(item =>
                    item.Definition is null
                    && string.Equals(item.Path, groupPath, StringComparison.Ordinal));
                if (group is null)
                {
                    group = new BlueprintNodePickerTreeItem(
                        EditorDisplayName.Format(segment),
                        groupPath,
                        null);
                    children.Add(group);
                }
                children = group.Children;
            }
            children.Add(new BlueprintNodePickerTreeItem(
                definition.Title,
                definition.RuntimePath,
                definition));
        }
        sortTree(roots);
        return roots;
    }

    private static void sortTree(List<BlueprintNodePickerTreeItem> items)
    {
        items.Sort((left, right) =>
        {
            if (left.Definition is null && right.Definition is not null)
                return -1;
            if (left.Definition is not null && right.Definition is null)
                return 1;
            return string.Compare(left.Name, right.Name, StringComparison.Ordinal);
        });
        foreach (BlueprintNodePickerTreeItem item in items)
            sortTree(item.Children);
    }

    private void appendVisibleRows(BlueprintNodePickerTreeItem item, int depth)
    {
        if (item.Definition is not null)
        {
            rows.Add(new BlueprintNodePickerRow(item.Name, depth, item.Definition));
            return;
        }
        bool isExpanded = expandedGroups.Contains(item.Path);
        rows.Add(new BlueprintNodePickerRow(item.Name, depth, item.Path, isExpanded));
        if (!isExpanded)
            return;
        foreach (BlueprintNodePickerTreeItem child in item.Children)
            appendVisibleRows(child, depth + 1);
    }

    private void toggleGroup(BlueprintNodePickerRow row)
    {
        if (!row.IsGroup || row.GroupPath is null)
            return;
        if (!expandedGroups.Add(row.GroupPath))
            expandedGroups.Remove(row.GroupPath);
        rebuildRows();
    }

    private void activateSelection()
    {
        if (itemList.SelectedItem is not BlueprintNodePickerRow row)
            return;
        if (row.IsGroup)
        {
            toggleGroup(row);
            return;
        }
        if (row.Definition is null)
            return;
        result = row.Definition;
        Close();
    }

    private void onOpened(object? sender, EventArgs args)
    {
        Activate();
        searchBox.Focus();
        canCloseOnDeactivate = false;
        deactivateTimer.Start();
    }

    private void onDeactivateTimer(object? sender, EventArgs args)
    {
        deactivateTimer.Stop();
        canCloseOnDeactivate = true;
    }

    private void onDeactivated(object? sender, EventArgs args)
    {
        if (canCloseOnDeactivate)
            Close();
    }

    private void onItemDoubleTapped(object? sender, TappedEventArgs args)
    {
        activateSelection();
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.Escape)
        {
            Close();
            args.Handled = true;
            return;
        }
        if (args.Key == Key.Enter)
        {
            activateSelection();
            args.Handled = true;
            return;
        }
        if (args.Key == Key.Down && searchBox.IsFocused && rows.Count != 0)
        {
            itemList.SelectedIndex = Math.Max(0, itemList.SelectedIndex);
            itemList.Focus();
            args.Handled = true;
        }
    }

    private void onClosed(object? sender, EventArgs args)
    {
        deactivateTimer.Stop();
        completion?.TrySetResult(result);
    }
}

internal sealed class BlueprintNodePickerRow
{
    public BlueprintNodePickerRow(
        string displayText,
        int depth,
        BlueprintGraphNodeDefinition definition)
    {
        DisplayText = displayText;
        Depth = depth;
        Definition = definition;
    }

    public BlueprintNodePickerRow(
        string displayText,
        int depth,
        string groupPath,
        bool isExpanded)
    {
        DisplayText = displayText;
        Depth = depth;
        GroupPath = groupPath;
        IsExpanded = isExpanded;
    }

    public string DisplayText { get; }
    public int Depth { get; }
    public string? GroupPath { get; }
    public BlueprintGraphNodeDefinition? Definition { get; }
    public bool IsGroup => GroupPath is not null;
    public bool IsExpanded { get; }
}

internal sealed class BlueprintNodePickerTreeItem
{
    public BlueprintNodePickerTreeItem(
        string name,
        string path,
        BlueprintGraphNodeDefinition? definition)
    {
        Name = name;
        Path = path;
        Definition = definition;
    }

    public string Name { get; }
    public string Path { get; }
    public BlueprintGraphNodeDefinition? Definition { get; }
    public List<BlueprintNodePickerTreeItem> Children { get; } = [];
}
