using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using Ludork.Plugins.OfficialResourceCleanup.Localization;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialResourceCleanup.UI;

internal sealed class ResourceCleanupConfirmation : Window
{
    private ResourceCleanupConfirmation(ResourceCleanupReport report, PluginLocalizer localizer)
    {
        Title = localizer.Text("confirmTitle");
        Width = 510;
        SizeToContent = SizeToContent.Height;
        CanResize = false;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = PluginTheme.Brush("Background");
        FontFamily = PluginTheme.FontFamily;
        Button cancel = new() { Content = localizer.Text("cancel"), MinWidth = 90 };
        Button confirm = new() { Content = localizer.Text("trash"), MinWidth = 130 };
        cancel.Click += (_, _) => Close(false);
        confirm.Click += (_, _) => Close(true);
        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
        };
        buttons.Children.Add(cancel);
        buttons.Children.Add(confirm);
        StackPanel panel = new() { Margin = new Thickness(22), Spacing = 18 };
        panel.Children.Add(new TextBlock
        {
            Text = localizer.Format("confirmMessage", report.Candidates.Count, localizer.Size(report.TotalBytes)),
            TextWrapping = TextWrapping.Wrap,
        });
        panel.Children.Add(buttons);
        Content = panel;
    }

    public static Task<bool> ShowAsync(Window owner, ResourceCleanupReport report, PluginLocalizer localizer)
    {
        return new ResourceCleanupConfirmation(report, localizer).ShowDialog<bool>(owner);
    }
}
