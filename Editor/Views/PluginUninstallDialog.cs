using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Services.Plugins;
using Ludork.Views.Utils;
using System;
using System.Threading.Tasks;

namespace Ludork.Views;

internal sealed class PluginUninstallDialog : Window
{
    private PluginUninstallDialog(PluginManagementItem item)
    {
        Title = LocaleService.Get("PLUGIN_UNINSTALL_TITLE");
        Width = 560;
        Height = 230;
        CanResize = false;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Ludork.Services.EditorTheme.Brush("Background");
        FontFamily = Ludork.Services.EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);

        string message = LocaleService.Get("PLUGIN_UNINSTALL_PROMPT")
            .Replace("{name}", item.Name, StringComparison.Ordinal)
            .Replace("{path}", item.SourcePath, StringComparison.Ordinal);
        TextBlock messageText = new()
        {
            Text = message,
            TextWrapping = TextWrapping.Wrap,
            VerticalAlignment = VerticalAlignment.Center,
        };
        Button unregister = new()
        {
            Content = LocaleService.Get("PLUGIN_UNREGISTER_ONLY"),
        };
        unregister.Classes.Add("dialog");
        unregister.Click += (_, _) => Close(
            (PluginUninstallChoice?)PluginUninstallChoice.UnregisterOnly);
        Button delete = new()
        {
            Content = LocaleService.Get("PLUGIN_UNREGISTER_DELETE"),
        };
        delete.Classes.Add("dialog");
        delete.Classes.Add("danger");
        delete.Click += (_, _) => Close(
            (PluginUninstallChoice?)PluginUninstallChoice.UnregisterAndDelete);
        Button cancel = new()
        {
            Content = LocaleService.Get("CANCEL"),
        };
        cancel.Classes.Add("dialog");
        cancel.Click += (_, _) => Close((PluginUninstallChoice?)null);

        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
        };
        buttons.Children.Add(cancel);
        buttons.Children.Add(unregister);
        buttons.Children.Add(delete);
        Border footer = new()
        {
            Padding = new Thickness(20, 12),
            Child = buttons,
        };
        footer.Classes.Add("editor-panel-footer");
        Border body = new()
        {
            Padding = new Thickness(24, 20),
            Child = messageText,
        };
        Grid layout = new()
        {
            RowDefinitions = new RowDefinitions("*,Auto"),
        };
        layout.Children.Add(body);
        Grid.SetRow(footer, 1);
        layout.Children.Add(footer);
        Content = layout;
    }

    public static Task<PluginUninstallChoice?> ShowAsync(
        Window owner,
        PluginManagementItem item)
    {
        return new PluginUninstallDialog(item)
            .ShowDialog<PluginUninstallChoice?>(owner);
    }
}
