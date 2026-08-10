using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Ludork.Plugins.OfficialBlueprintAI.Localization;

namespace Ludork.Plugins.OfficialBlueprintAI.UI;

internal static class PluginUiText
{
    private static PluginLocalizer? localizer;

    public static void Initialize(PluginLocalizer localizer)
    {
        PluginUiText.localizer = localizer;
    }

    public static string Get(string key)
    {
        return localizer?.Text(key) ?? key;
    }
}

internal static class EditorInputs
{
    public static TextBox CreateEditableTextBox()
    {
        TextBox textBox = new TextBox();
        ApplyEditable(textBox);
        return textBox;
    }

    public static void ApplyEditable(TextBox textBox)
    {
        textBox.MinHeight = 34;
        textBox.Padding = new Thickness(16, 0);
        textBox.CornerRadius = new CornerRadius(4);
        textBox.Focusable = true;
        textBox.IsTabStop = true;
        textBox.Background = new SolidColorBrush(Color.Parse("#333333"));
        textBox.BorderBrush = new SolidColorBrush(Color.Parse("#464646"));
        textBox.BorderThickness = new Thickness(1);
        textBox.Classes.Add("ludork-editable");
    }
}

internal static class ItemSelectorDialog
{
    public static async Task<string?> ShowAsync(
        Window owner,
        string title,
        string message,
        IEnumerable<string> items,
        string? selected)
    {
        ComboBox selector = new ComboBox
        {
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        foreach (string item in items)
        {
            selector.Items.Add(item);
        }
        selector.SelectedItem = selector.Items
            .Cast<string>()
            .FirstOrDefault(item =>
                string.Equals(item, selected, StringComparison.Ordinal))
            ?? selector.Items.Cast<object>().FirstOrDefault();
        return await DialogHost.ShowValueAsync(
            owner,
            title,
            message,
            selector,
            () => selector.SelectedItem as string);
    }
}

internal static class SingleRowDialog
{
    public static Task<string?> ShowAsync(
        Window owner,
        string title,
        string message,
        IEnumerable<string> existingValues,
        string currentValue)
    {
        HashSet<string> existing = existingValues.ToHashSet(
            StringComparer.Ordinal);
        TextBox input = EditorInputs.CreateEditableTextBox();
        input.Text = currentValue;
        return DialogHost.ShowValueAsync(
            owner,
            title,
            message,
            input,
            () =>
            {
                string value = input.Text?.Trim() ?? string.Empty;
                return value.Length == 0 || existing.Contains(value)
                    ? null
                    : value;
            });
    }
}

internal static class ConfirmationDialog
{
    public static async Task<bool> ShowAsync(
        Window owner,
        string title,
        string message)
    {
        bool? result = await DialogHost.ShowValueAsync(
            owner,
            title,
            message,
            new Border(),
            () => true);
        return result == true;
    }
}

internal static class DialogHost
{
    public static async Task<T?> ShowValueAsync<T>(
        Window owner,
        string title,
        string message,
        Control content,
        Func<T?> getValue)
    {
        TaskCompletionSource<T?> completion =
            new TaskCompletionSource<T?>(
                TaskCreationOptions.RunContinuationsAsynchronously);
        Button cancel = new Button
        {
            Content = PluginUiText.Get("CANCEL"),
            MinWidth = 88,
        };
        Button confirm = new Button
        {
            Content = PluginUiText.Get("CONFIRM"),
            MinWidth = 88,
        };
        StackPanel buttons = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
        };
        buttons.Children.Add(cancel);
        buttons.Children.Add(confirm);
        StackPanel panel = new StackPanel
        {
            Margin = new Thickness(18),
            Spacing = 12,
        };
        panel.Children.Add(new TextBlock
        {
            Text = message,
            TextWrapping = TextWrapping.Wrap,
        });
        panel.Children.Add(content);
        panel.Children.Add(buttons);
        Window window = new Window
        {
            Title = title,
            Width = 480,
            Height = 230,
            MinWidth = 380,
            MinHeight = 190,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            Background = new SolidColorBrush(Color.Parse("#202124")),
            Content = panel,
        };
        cancel.Click += (_, _) =>
        {
            completion.TrySetResult(default);
            window.Close();
        };
        confirm.Click += (_, _) =>
        {
            T? value = getValue();
            if (value is null)
            {
                return;
            }
            completion.TrySetResult(value);
            window.Close();
        };
        window.Closed += (_, _) => completion.TrySetResult(default);
        window.Show(owner);
        return await completion.Task;
    }
}
