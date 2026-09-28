using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public sealed class SearchSelectorDialog : Window
{
    private readonly IReadOnlyList<SearchSelectorGroup> groups;
    private readonly TextBox searchBox;
    private readonly List<ListBox> optionLists = [];
    private bool changingSelection;
    private readonly Button confirmButton;

    private SearchSelectorDialog(
        string title,
        IReadOnlyList<SearchSelectorGroup> groups,
        string current,
        bool grouped)
    {
        Title = title;
        Width = grouped ? 800 : 360;
        Height = grouped ? 560 : 480;
        MinWidth = grouped ? 500 : 300;
        MinHeight = grouped ? 300 : 280;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);

        this.groups = groups;
        searchBox = EditorInputs.CreateEditableTextBox();
        searchBox.PlaceholderText = LocaleService.Get("SEARCH");
        searchBox.TextChanged += (_, _) => rebuildOptions();

        Grid lists = new() { ColumnSpacing = 8 };
        foreach (SearchSelectorGroup group in groups)
        {
            ListBox list = new()
            {
                HorizontalAlignment = HorizontalAlignment.Stretch,
                VerticalAlignment = VerticalAlignment.Stretch,
                ItemTemplate = HintedTextPresenter.StringItemTemplate,
            };
            list.SelectionChanged += (_, _) => selectFrom(list);
            list.DoubleTapped += (_, _) => confirm();
            Control column = list;
            if (grouped)
            {
                Grid titledColumn = new() { RowDefinitions = new RowDefinitions("Auto,4,*") };
                titledColumn.Children.Add(new TextBlock { Text = group.Title });
                Grid.SetRow(list, 2);
                titledColumn.Children.Add(list);
                column = titledColumn;
            }
            Grid.SetColumn(column, optionLists.Count);
            lists.ColumnDefinitions.Add(new ColumnDefinition(new GridLength(1, GridUnitType.Star)));
            lists.Children.Add(column);
            optionLists.Add(list);
        }

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
        rebuildOptions();
        for (int index = groups.Count - 1; index >= 0; index--)
        {
            if (!groups[index].Options.Contains(current, StringComparer.Ordinal))
                continue;
            optionLists[index].SelectedItem = current;
            break;
        }
    }

    public static Task<string?> ShowAsync(
        Window owner,
        string title,
        IEnumerable<string> options,
        string current = "")
    {
        SearchSelectorDialog dialog = new(title, [new SearchSelectorGroup(string.Empty, options.ToArray())], current, false);
        return dialog.ShowDialog<string?>(owner);
    }

    internal static Task<string?> ShowGroupedAsync(
        Window owner,
        string title,
        IReadOnlyList<SearchSelectorGroup> groups,
        string current)
    {
        SearchSelectorDialog dialog = new(title, groups, current, true);
        return dialog.ShowDialog<string?>(owner);
    }

    private void rebuildOptions()
    {
        string search = searchBox.Text?.Trim() ?? string.Empty;
        changingSelection = true;
        for (int index = 0; index < groups.Count; index++)
        {
            ListBox list = optionLists[index];
            string? selected = list.SelectedItem as string;
            string[] filtered = groups[index].Options
                .Where(option => option.Contains(search, StringComparison.OrdinalIgnoreCase))
                .ToArray();
            list.ItemsSource = filtered;
            list.SelectedItem = selected is not null && filtered.Contains(selected, StringComparer.Ordinal)
                ? selected
                : null;
        }
        changingSelection = false;
        updateConfirmState();
    }

    private void selectFrom(ListBox source)
    {
        if (changingSelection)
            return;
        changingSelection = true;
        if (source.SelectedItem is string)
        {
            foreach (ListBox other in optionLists)
            {
                if (!ReferenceEquals(source, other))
                    other.SelectedItem = null;
            }
        }
        changingSelection = false;
        updateConfirmState();
    }

    private string? getSelection()
    {
        return optionLists.Select(list => list.SelectedItem).OfType<string>().FirstOrDefault();
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
