using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Services.Plugins;
using Ludork.Views.Utils;
using System.IO;
using System.Threading.Tasks;

namespace Ludork.Views;

internal sealed class PluginCreationDialog : Window
{
    private readonly PluginWorkspace workspace;
    private readonly string? projectPath;
    private readonly TextBox name = EditorInputs.CreateEditableTextBox();
    private readonly TextBox id = EditorInputs.CreateEditableTextBox();
    private readonly PluginScopeSelector scope;
    private readonly RadioButton withWindow = new()
    {
        Content = LocaleService.Get("PLUGIN_TEMPLATE_WINDOW"),
        GroupName = "PluginTemplate",
        IsChecked = true,
    };
    private readonly TextBlock error = new()
    {
        Foreground = new SolidColorBrush(Color.Parse("#ef9a9a")),
        TextWrapping = TextWrapping.Wrap,
        IsVisible = false,
    };
    private readonly StackPanel form;
    private bool creating;

    private PluginCreationDialog(PluginWorkspace workspace, string? projectPath)
    {
        this.workspace = workspace;
        this.projectPath = projectPath;
        Title = LocaleService.Get("CREATE_PLUGIN");
        Width = 480;
        SizeToContent = SizeToContent.Height;
        CanResize = false;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = EditorTheme.Brush("Background");
        FontFamily = EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);

        scope = new PluginScopeSelector(!string.IsNullOrWhiteSpace(projectPath));
        StackPanel templateChoices = new()
        {
            Orientation = Orientation.Horizontal,
            Spacing = 20,
            Children =
            {
                withWindow,
                new RadioButton
                {
                    Content = LocaleService.Get("PLUGIN_TEMPLATE_LOGIC"),
                    GroupName = "PluginTemplate",
                },
            },
        };
        Button create = new() { Content = LocaleService.Get("CREATE"), IsDefault = true };
        create.Click += onCreate;
        Button cancel = new() { Content = LocaleService.Get("CANCEL") };
        cancel.Click += (_, _) => Close((string?)null);
        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { create, cancel },
        };
        form = new StackPanel
        {
            Margin = new Thickness(22),
            Spacing = 12,
            Children =
            {
                new TextBlock { Text = LocaleService.Get("PLUGIN_NAME") },
                name,
                new TextBlock { Text = LocaleService.Get("PLUGIN_ID") },
                id,
                new TextBlock { Text = LocaleService.Get("PLUGIN_TEMPLATE") },
                templateChoices,
                scope,
                error,
                buttons,
            },
        };
        Content = form;
        Opened += (_, _) => name.Focus();
        Closing += (_, args) => args.Cancel = creating;
        KeyDown += (_, args) =>
        {
            if (args.Key != Key.Escape || creating)
                return;
            Close((string?)null);
            args.Handled = true;
        };
    }

    public static Task<string?> ShowAsync(Window owner, PluginWorkspace workspace, string? projectPath)
    {
        return new PluginCreationDialog(workspace, projectPath).ShowDialog<string?>(owner);
    }

    private async void onCreate(object? sender, RoutedEventArgs args)
    {
        if (creating)
            return;
        string pluginName = name.Text?.Trim() ?? string.Empty;
        string pluginId = id.Text?.Trim() ?? string.Empty;
        if (pluginName.Length == 0 || pluginId.Length == 0)
        {
            error.Text = LocaleService.Get("PLUGIN_CREATE_REQUIRED");
            error.IsVisible = true;
            return;
        }
        PluginManagementService management = workspace.GetManagement(scope.IsProject ? projectPath : null);
        creating = true;
        form.IsEnabled = false;
        error.IsVisible = false;
        PluginManagementResult result;
        try
        {
            result = await management.CreateAsync(new PluginCreationRequest(
                pluginId, pluginName, withWindow.IsChecked == true));
        }
        finally
        {
            creating = false;
            form.IsEnabled = true;
        }
        if (!result.Success)
        {
            error.Text = result.Error;
            error.IsVisible = true;
            return;
        }
        Close(Path.Combine(management.Environment.PluginsDirectory, pluginId));
    }
}
