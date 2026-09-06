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
        Background = new SolidColorBrush(Color.Parse("#202124"));
        FontFamily = FontFamily.Parse(
            "avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
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
        unregister.Click += (_, _) => Close(
            (PluginUninstallChoice?)PluginUninstallChoice.UnregisterOnly);
        Button delete = new()
        {
            Content = LocaleService.Get("PLUGIN_UNREGISTER_DELETE"),
        };
        delete.Click += (_, _) => Close(
            (PluginUninstallChoice?)PluginUninstallChoice.UnregisterAndDelete);
        Button cancel = new()
        {
            Content = LocaleService.Get("CANCEL"),
        };
        cancel.Click += (_, _) => Close((PluginUninstallChoice?)null);

        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
        };
        buttons.Children.Add(unregister);
        buttons.Children.Add(delete);
        buttons.Children.Add(cancel);
        Grid layout = new()
        {
            Margin = new Thickness(22),
            RowDefinitions = new RowDefinitions("*,Auto"),
            RowSpacing = 16,
        };
        layout.Children.Add(messageText);
        Grid.SetRow(buttons, 1);
        layout.Children.Add(buttons);
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
