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
    private readonly IReadOnlyList<string> options;
    private readonly TextBox searchBox;
    private readonly ListBox optionList;
    private readonly Button confirmButton;

    private SearchSelectorDialog(
        string title,
        IEnumerable<string> options,
        string current)
    {
        Title = title;
        Width = 360;
        Height = 480;
        MinWidth = 300;
        MinHeight = 280;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);

        this.options = options.ToArray();
        searchBox = EditorInputs.CreateEditableTextBox();
        searchBox.PlaceholderText = LocaleService.Get("SEARCH");
        searchBox.TextChanged += (_, _) => rebuildOptions();

        optionList = new ListBox
        {
            HorizontalAlignment = HorizontalAlignment.Stretch,
            VerticalAlignment = VerticalAlignment.Stretch,
            ItemTemplate = HintedTextPresenter.StringItemTemplate,
        };
        optionList.SelectionChanged += (_, _) => updateConfirmState();
        optionList.DoubleTapped += (_, _) => confirm();

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
        Grid.SetRow(optionList, 2);
        content.Children.Add(optionList);
        Grid.SetRow(actions, 4);
        content.Children.Add(actions);
        Content = content;

        KeyDown += onKeyDown;
        Opened += (_, _) => searchBox.Focus();
        rebuildOptions();
        if (this.options.Contains(current, StringComparer.Ordinal))
            optionList.SelectedItem = current;
    }

    public static Task<string?> ShowAsync(
        Window owner,
        string title,
        IEnumerable<string> options,
        string current = "")
    {
        SearchSelectorDialog dialog = new(title, options, current);
        return dialog.ShowDialog<string?>(owner);
    }

    private void rebuildOptions()
    {
        string search = searchBox.Text?.Trim() ?? string.Empty;
        string[] filtered = options
            .Where(option => option.Contains(search, StringComparison.OrdinalIgnoreCase))
            .ToArray();
        string? selected = optionList.SelectedItem as string;
        optionList.ItemsSource = filtered;
        optionList.SelectedItem = selected is not null
            && filtered.Contains(selected, StringComparer.Ordinal)
            ? selected
            : null;
        updateConfirmState();
    }

    private void updateConfirmState()
    {
        confirmButton.IsEnabled = optionList.SelectedItem is string;
    }

    private void confirm()
    {
        if (optionList.SelectedItem is string selected && selected.Length != 0)
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
