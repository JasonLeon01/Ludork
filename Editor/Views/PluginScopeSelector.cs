using Avalonia.Controls;
using Avalonia.Layout;
using Ludork.Services;

namespace Ludork.Views;

internal sealed class PluginScopeSelector : StackPanel
{
    private readonly RadioButton project = new()
    {
        Content = LocaleService.Get("PLUGIN_SCOPE_PROJECT"),
        GroupName = "PluginScope",
    };

    public PluginScopeSelector(bool hasProject)
    {
        Spacing = 8;
        Children.Add(new TextBlock { Text = LocaleService.Get("PLUGIN_SCOPE") });
        StackPanel choices = new()
        {
            Orientation = Orientation.Horizontal,
            Spacing = 20,
        };
        choices.Children.Add(new RadioButton
        {
            Content = LocaleService.Get("PLUGIN_SCOPE_GLOBAL"),
            GroupName = "PluginScope",
            IsChecked = !hasProject,
        });
        project.IsChecked = hasProject;
        project.IsEnabled = hasProject;
        choices.Children.Add(project);
        Children.Add(choices);
    }

    public bool IsProject => project.IsChecked == true;
}
