using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public sealed class ItemSelectorDialog : Window
{
    private readonly ComboBox comboBox;

    private ItemSelectorDialog(string title, string message, IEnumerable<string> options, string? initial)
    {
        Title = title;
        Width = 400;
        Height = 200;
        MinWidth = 320;
        MinHeight = 160;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Ludork.Services.EditorTheme.Brush("Surface");
        FontFamily = Ludork.Services.EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);

        comboBox = new ComboBox { HorizontalAlignment = HorizontalAlignment.Stretch };
        string? selectedItem = null;
        foreach (string opt in options)
        {
            comboBox.Items.Add(opt);
            if (opt == initial)
                selectedItem = opt;
        }
        if (selectedItem is not null)
            comboBox.SelectedItem = selectedItem;
        else if (comboBox.Items.Count > 0)
            comboBox.SelectedIndex = 0;

        TextBlock desc = new()
        {
            Text = message,
            TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(0, 0, 0, 8),
            Foreground = EditorTheme.Brush("TextMuted"),
        };
        Button confirm = new() { Content = LocaleService.Get("CONFIRM"), MinWidth = 80 };
        confirm.Click += onConfirm;
        Button cancel = new() { Content = LocaleService.Get("CANCEL"), MinWidth = 80 };
        cancel.Click += (_, _) => Close(null);

        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
        };
        buttons.Children.Add(confirm);
        buttons.Children.Add(cancel);

        StackPanel content = new() { Margin = new Thickness(20), Spacing = 10 };
        content.Children.Add(desc);
        content.Children.Add(comboBox);
        content.Children.Add(buttons);
        Content = content;
    }

    public static Task<string?> ShowAsync(
        Window owner,
        string title,
        string message,
        IEnumerable<string> options,
        string? initial = null)
    {
        return new ItemSelectorDialog(title, message, options, initial).ShowDialog<string?>(owner);
    }

    private void onConfirm(object? sender, RoutedEventArgs args)
    {
        Close(comboBox.SelectedItem as string);
    }
}
