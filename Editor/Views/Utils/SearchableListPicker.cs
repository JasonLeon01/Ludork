using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Views.Utils;

public sealed class SearchableListPicker : Button
{
    private readonly Flyout flyout;
    private readonly Border flyoutBorder;
    private readonly TextBox searchBox;
    private readonly ListBox optionList;
    private readonly TextBlock valueText;
    private readonly TextBlock itemCountText;
    private IReadOnlyList<string> itemsSource = [];
    private string selectedValue = string.Empty;
    private string placeholderText = string.Empty;
    private bool rebuilding;

    public SearchableListPicker()
    {
        Height = EditorInputs.FieldMinHeight;
        MinHeight = EditorInputs.FieldMinHeight;
        Padding = EditorInputs.FieldPadding;
        HorizontalAlignment = HorizontalAlignment.Stretch;
        HorizontalContentAlignment = HorizontalAlignment.Stretch;
        Background = new SolidColorBrush(EditorInputs.EditableBackgroundColor);
        BorderBrush = new SolidColorBrush(EditorInputs.FieldBorderColor);
        BorderThickness = new Thickness(1);
        CornerRadius = new CornerRadius(4);

        valueText = new TextBlock
        {
            VerticalAlignment = VerticalAlignment.Center,
            TextTrimming = TextTrimming.CharacterEllipsis,
        };
        TextBlock arrow = new()
        {
            Text = "▾",
            VerticalAlignment = VerticalAlignment.Center,
            Foreground = Brushes.Gray,
        };
        Grid buttonContent = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
            ColumnSpacing = 8,
        };
        buttonContent.Children.Add(valueText);
        Grid.SetColumn(arrow, 1);
        buttonContent.Children.Add(arrow);
        Content = buttonContent;

        searchBox = EditorInputs.CreateEditableTextBox();
        searchBox.PlaceholderText = LocaleService.Get("SEARCH");
        searchBox.TextChanged += (_, _) => rebuildOptions();
        searchBox.KeyDown += onSearchKeyDown;

        optionList = new ListBox
        {
            HorizontalAlignment = HorizontalAlignment.Stretch,
            VerticalAlignment = VerticalAlignment.Stretch,
            ItemTemplate = HintedTextPresenter.StringItemTemplate,
        };
        optionList.PointerReleased += onOptionPointerReleased;
        optionList.KeyDown += onOptionListKeyDown;

        itemCountText = new TextBlock
        {
            Foreground = Brushes.Gray,
        };

        Grid flyoutContent = new()
        {
            RowDefinitions = new RowDefinitions("Auto,8,*,8,Auto"),
            MinHeight = 220,
            MaxHeight = 360,
        };
        flyoutContent.Children.Add(searchBox);
        Grid.SetRow(optionList, 2);
        flyoutContent.Children.Add(optionList);
        Grid.SetRow(itemCountText, 4);
        flyoutContent.Children.Add(itemCountText);
        flyoutBorder = new Border
        {
            Width = 320,
            Padding = new Thickness(8),
            Background = new SolidColorBrush(Color.Parse("#202225")),
            BorderBrush = new SolidColorBrush(EditorInputs.FieldBorderColor),
            BorderThickness = new Thickness(1),
            Child = flyoutContent,
        };
        flyout = new Flyout
        {
            Placement = PlacementMode.BottomEdgeAlignedLeft,
            Content = flyoutBorder,
        };
        flyout.Opened += onFlyoutOpened;
        Flyout = flyout;
        refreshValueText();
    }

    public event EventHandler? SelectionChanged;

    public IEnumerable<string> ItemsSource
    {
        get => itemsSource;
        set
        {
            itemsSource = value?.ToArray() ?? [];
            rebuildOptions();
        }
    }

    public string SelectedValue
    {
        get => selectedValue;
        set
        {
            string next = value ?? string.Empty;
            if (string.Equals(selectedValue, next, StringComparison.Ordinal))
                return;
            selectedValue = next;
            refreshValueText();
            rebuildOptions();
        }
    }

    public string PlaceholderText
    {
        get => placeholderText;
        set
        {
            placeholderText = value ?? string.Empty;
            refreshValueText();
        }
    }

    public Func<string, string, bool>? Filter { get; set; }

    private void onFlyoutOpened(object? sender, EventArgs args)
    {
        flyoutBorder.Width = Math.Max(240, Bounds.Width);
        searchBox.Text = string.Empty;
        rebuildOptions();
        Dispatcher.UIThread.Post(() =>
        {
            searchBox.Focus();
            searchBox.SelectAll();
        });
    }

    private void rebuildOptions()
    {
        string query = searchBox.Text?.Trim() ?? string.Empty;
        IEnumerable<string> filtered = itemsSource.Where(option => matches(option, query));
        string[] options = filtered.ToArray();
        itemCountText.Text = LocaleService.Get("GAME_VARIABLE_ITEM_COUNT")
            .Replace("{count}", options.Length.ToString(), StringComparison.Ordinal);
        rebuilding = true;
        optionList.ItemsSource = options;
        optionList.SelectedItem = options.Contains(selectedValue, StringComparer.Ordinal)
            ? selectedValue
            : null;
        rebuilding = false;
    }

    private bool matches(string option, string query)
    {
        if (Filter is not null)
            return Filter(option, query);
        return option.Contains(query, StringComparison.OrdinalIgnoreCase);
    }

    private void onOptionPointerReleased(object? sender, PointerReleasedEventArgs args)
    {
        Control? source = args.Source as Control;
        bool isOption = source is ListBoxItem
            || source?.GetVisualAncestors().OfType<ListBoxItem>().Any() == true;
        if (rebuilding || !isOption || optionList.SelectedItem is not string selected)
            return;
        select(selected);
        args.Handled = true;
    }

    private void select(string value)
    {
        if (!string.Equals(selectedValue, value, StringComparison.Ordinal))
        {
            selectedValue = value;
            refreshValueText();
            SelectionChanged?.Invoke(this, EventArgs.Empty);
        }
        flyout.Hide();
    }

    private void onSearchKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.Escape)
        {
            flyout.Hide();
            args.Handled = true;
            return;
        }
        if (args.Key == Key.Enter && optionList.SelectedItem is string selected)
        {
            select(selected);
            args.Handled = true;
            return;
        }
        if (args.Key != Key.Down || optionList.ItemCount == 0)
            return;
        optionList.SelectedIndex = Math.Max(0, optionList.SelectedIndex);
        optionList.Focus();
        args.Handled = true;
    }

    private void onOptionListKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.Escape)
        {
            flyout.Hide();
            args.Handled = true;
        }
        else if (args.Key == Key.Enter && optionList.SelectedItem is string selected)
        {
            select(selected);
            args.Handled = true;
        }
    }

    private void refreshValueText()
    {
        valueText.Text = selectedValue.Length == 0 ? placeholderText : selectedValue;
        if (selectedValue.Length == 0)
            valueText.Foreground = Brushes.Gray;
        else
            valueText.ClearValue(TextBlock.ForegroundProperty);
    }
}
