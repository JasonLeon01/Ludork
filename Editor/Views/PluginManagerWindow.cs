using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Ludork.Services;
using Ludork.Services.Plugins;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class PluginManagerWindow : Window
{
    private readonly PluginWorkspace workspace;
    private readonly string? projectPath;
    private readonly StackPanel pluginList = new() { Spacing = 10 };
    private readonly TextBlock registryDiagnostic = new()
    {
        Foreground = new SolidColorBrush(Color.Parse("#ffb74d")),
        TextWrapping = TextWrapping.Wrap,
        IsVisible = false,
    };
    private readonly Button importButton = new();
    private readonly Button createButton = new();

    public PluginManagerWindow(PluginWorkspace workspace, string? projectPath)
    {
        this.workspace = workspace;
        this.projectPath = projectPath;
        Title = LocaleService.Get("PLUGIN_MANAGER_TITLE");
        Width = 820;
        Height = 620;
        MinWidth = 620;
        MinHeight = 420;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = Ludork.Services.EditorTheme.Brush("Background");
        FontFamily = Ludork.Services.EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);
        EditorLayoutService.AttachWindow(this, nameof(PluginManagerWindow));

        createButton.Content = LocaleService.Get("CREATE_PLUGIN");
        createButton.Click += onCreate;
        importButton.Content = LocaleService.Get("IMPORT_PLUGIN");
        importButton.Click += onImport;
        Button closeButton = new()
        {
            Content = LocaleService.Get("CLOSE"),
            MinWidth = 88,
        };
        closeButton.Click += (_, _) => Close();

        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
        };
        buttons.Children.Add(createButton);
        buttons.Children.Add(importButton);
        buttons.Children.Add(closeButton);

        Grid listGrid = new();
        listGrid.Children.Add(new ScrollViewer
        {
            Content = pluginList,
            VerticalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
        });

        Grid content = new()
        {
            Margin = new Thickness(20),
            RowDefinitions = new RowDefinitions("Auto,*,Auto"),
            RowSpacing = 12,
        };
        content.Children.Add(registryDiagnostic);
        Grid.SetRow(listGrid, 1);
        content.Children.Add(listGrid);
        Grid.SetRow(buttons, 2);
        content.Children.Add(buttons);
        Content = content;
        Opened += async (_, _) => await refreshAsync();
        KeyDown += onKeyDown;
    }

    public static Task ShowAsync(Window owner, PluginWorkspace workspace, string? projectPath)
    {
        return new PluginManagerWindow(workspace, projectPath).ShowDialog(owner);
    }

    public static async Task<bool> ImportPluginAsync(
        Window owner,
        PluginWorkspace workspace,
        string? projectPath)
    {
        bool? isProject = string.IsNullOrWhiteSpace(projectPath)
            ? false
            : await PluginScopeDialog.ShowAsync(owner);
        if (isProject is null)
            return false;
        PluginManagementService management = workspace.GetManagement(isProject.Value ? projectPath : null);
        string scope = LocaleService.Get(isProject.Value ? "PLUGIN_SCOPE_PROJECT" : "PLUGIN_SCOPE_GLOBAL");
        IStorageFolder? suggested = Directory.Exists(management.Environment.PluginsDirectory)
            ? await owner.StorageProvider.TryGetFolderFromPathAsync(
                new Uri(management.Environment.PluginsDirectory))
            : null;
        IReadOnlyList<IStorageFolder> folders =
            await owner.StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
            {
                Title = LocaleService.Get("IMPORT_PLUGIN"),
                AllowMultiple = false,
                SuggestedStartLocation = suggested,
            });
        if (folders.Count == 0)
            return false;

        string sourcePath = folders[0].Path.LocalPath;
        PluginImportPreview preview =
            await management.PreviewImportAsync(sourcePath);
        if (!preview.Success)
        {
            await AlertDialog.ShowAsync(
                owner,
                LocaleService.Get("PLUGIN_IMPORT_FAILED"),
                $"{scope}: {preview.Error}");
            return false;
        }

        string warning = LocaleService.Get("PLUGIN_FULL_TRUST_WARNING")
            .Replace("{name}", preview.Name, StringComparison.Ordinal)
            .Replace("{id}", preview.Id, StringComparison.Ordinal)
            .Replace("{path}", preview.SourcePath, StringComparison.Ordinal)
            + Environment.NewLine + Environment.NewLine
            + LocaleService.Get("PLUGIN_SCOPE") + ": " + scope;
        bool confirmed = await ConfirmationDialog.ShowAsync(
            owner,
            LocaleService.Get("PLUGIN_FULL_TRUST_TITLE"),
            warning);
        if (!confirmed)
            return false;

        PluginManagementResult result =
            await management.ImportAsync(preview.SourcePath);
        if (!result.Success)
        {
            await AlertDialog.ShowAsync(
                owner,
                LocaleService.Get("PLUGIN_IMPORT_FAILED"),
                $"{scope}: {result.Error}");
            return false;
        }

        await AlertDialog.ShowAsync(
            owner,
            LocaleService.Get("PLUGIN_IMPORT_SUCCESS"),
            LocaleService.Get("PLUGIN_RESTART_REQUIRED"));
        return true;
    }

    private async void onImport(object? sender, Avalonia.Interactivity.RoutedEventArgs args)
    {
        setActionsEnabled(false);
        try
        {
            if (await ImportPluginAsync(this, workspace, projectPath))
                await refreshAsync();
        }
        finally
        {
            setActionsEnabled(true);
        }
    }

    private async void onCreate(object? sender, Avalonia.Interactivity.RoutedEventArgs args)
    {
        setActionsEnabled(false);
        try
        {
            string? sourcePath = await PluginCreationDialog.ShowAsync(this, workspace, projectPath);
            if (sourcePath is null)
                return;
            await refreshAsync();
            string message = LocaleService.Get("PLUGIN_RESTART_REQUIRED");
            try
            {
                Process.Start(new ProcessStartInfo(sourcePath) { UseShellExecute = true });
            }
            catch (Exception exception) when (exception is System.ComponentModel.Win32Exception
                or InvalidOperationException)
            {
                message += Environment.NewLine + Environment.NewLine
                    + LocaleService.Get("PLUGIN_OPEN_DIRECTORY_FAILED")
                    + Environment.NewLine + sourcePath
                    + Environment.NewLine + exception.Message;
            }
            await AlertDialog.ShowAsync(this, LocaleService.Get("PLUGIN_CREATE_SUCCESS"), message);
        }
        finally
        {
            setActionsEnabled(true);
        }
    }

    private void setActionsEnabled(bool enabled)
    {
        createButton.IsEnabled = enabled;
        importButton.IsEnabled = enabled;
    }

    private async Task refreshAsync()
    {
        pluginList.Children.Clear();
        List<string> diagnostics = [];
        await addScopeAsync(workspace.GlobalHost, "PLUGIN_SCOPE_GLOBAL", diagnostics);
        PluginHost? projectHost = workspace.GetProjectHost(projectPath);
        if (projectHost is not null)
            await addScopeAsync(projectHost, "PLUGIN_SCOPE_PROJECT", diagnostics);
        registryDiagnostic.Text = string.Join(Environment.NewLine, diagnostics);
        registryDiagnostic.IsVisible = registryDiagnostic.Text.Length != 0;
    }

    private async Task addScopeAsync(PluginHost host, string scopeKey, List<string> diagnostics)
    {
        string scope = LocaleService.Get(scopeKey);
        List<string> scopeDiagnostics = [];
        if (host.Management.RegistryDiagnostic.Length != 0)
            scopeDiagnostics.Add(host.Management.RegistryDiagnostic);
        scopeDiagnostics.AddRange(host.Management.StartupDiagnostics);
        diagnostics.AddRange(scopeDiagnostics.Distinct(StringComparer.Ordinal)
            .Select(diagnostic => $"{scope}: {diagnostic}"));
        pluginList.Children.Add(new TextBlock
        {
            Text = scope,
            FontSize = 18,
            FontWeight = FontWeight.SemiBold,
            Margin = new Thickness(0, 6, 0, 2),
        });
        IReadOnlyList<PluginManagementItem> items = await host.GetManagementItemsAsync();
        if (items.Count == 0)
        {
            pluginList.Children.Add(new TextBlock
            {
                Text = LocaleService.Get("PLUGIN_EMPTY"),
                Foreground = EditorTheme.Brush("TextMuted"),
            });
        }
        foreach (PluginManagementItem item in items)
            pluginList.Children.Add(createPluginCard(item, host.Management, scope));
    }

    private Control createPluginCard(PluginManagementItem item, PluginManagementService management, string scope)
    {
        TextBlock title = new()
        {
            Text = item.Name,
            FontSize = 17,
            FontWeight = FontWeight.SemiBold,
            VerticalAlignment = VerticalAlignment.Center,
        };
        TextBlock status = new()
        {
            Text = getStatusText(item.Status),
            Foreground = getStatusBrush(item.Status),
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid heading = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
            ColumnSpacing = 12,
        };
        heading.Children.Add(title);
        Grid.SetColumn(status, 1);
        heading.Children.Add(status);

        StackPanel details = new() { Spacing = 5 };
        details.Children.Add(createDetail("PLUGIN_ID", item.Id));
        details.Children.Add(createDetail("PLUGIN_VERSION", item.Version));
        details.Children.Add(createDetail("PLUGIN_SOURCE", item.SourcePath));
        if (item.Diagnostic.Length != 0)
        {
            TextBlock diagnosticLabel = new()
            {
                Text = LocaleService.Get("PLUGIN_DIAGNOSTICS"),
                Foreground = Ludork.Services.EditorTheme.Brush("TextMuted"),
                Margin = new Thickness(0, 5, 0, 0),
            };
            TextBlock diagnostic = new()
            {
                Text = item.Diagnostic,
                TextWrapping = TextWrapping.Wrap,
                Foreground = EditorTheme.Brush("Text"),
                FontFamily = FontFamily.Parse("Consolas"),
                FontSize = 12,
            };
            details.Children.Add(diagnosticLabel);
            details.Children.Add(diagnostic);
        }

        StackPanel cardContent = new()
        {
            Margin = new Thickness(14),
            Spacing = 9,
        };
        cardContent.Children.Add(heading);
        cardContent.Children.Add(details);
        if (item.CanUninstall)
        {
            Button uninstall = new()
            {
                Content = LocaleService.Get("PLUGIN_UNINSTALL"),
                HorizontalAlignment = HorizontalAlignment.Right,
            };
            uninstall.Click += async (_, _) => await uninstallAsync(uninstall, item, management, scope);
            cardContent.Children.Add(uninstall);
        }
        return new Border
        {
            Background = new SolidColorBrush(Color.Parse("#292a2d")),
            BorderBrush = Ludork.Services.EditorTheme.Brush("Border"),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(5),
            Child = cardContent,
        };
    }

    private static Control createDetail(string labelKey, string value)
    {
        TextBlock label = new()
        {
            Text = LocaleService.Get(labelKey),
            Foreground = new SolidColorBrush(Color.Parse("#9e9e9e")),
            MinWidth = 85,
        };
        TextBlock text = new()
        {
            Text = value.Length == 0 ? "—" : value,
            TextWrapping = TextWrapping.Wrap,
        };
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*"),
            ColumnSpacing = 10,
        };
        row.Children.Add(label);
        Grid.SetColumn(text, 1);
        row.Children.Add(text);
        return row;
    }

    private async Task uninstallAsync(
        Button button,
        PluginManagementItem item,
        PluginManagementService management,
        string scope)
    {
        PluginUninstallChoice? choice = await PluginUninstallDialog.ShowAsync(this, item);
        if (choice is null)
            return;
        button.IsEnabled = false;
        PluginManagementResult result = await management.UnregisterAsync(
            item.Id,
            choice == PluginUninstallChoice.UnregisterAndDelete);
        if (!result.Success)
        {
            button.IsEnabled = true;
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("PLUGIN_OPERATION_FAILED"),
                $"{scope}: {result.Error}");
            return;
        }
        await AlertDialog.ShowAsync(
            this,
            LocaleService.Get("PLUGIN_UNINSTALL_SUCCESS"),
            LocaleService.Get("PLUGIN_RESTART_REQUIRED"));
        await refreshAsync();
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key != Key.Escape)
            return;
        Close();
        args.Handled = true;
    }

    private static string getStatusText(PluginRuntimeStatus status)
    {
        string key = status switch
        {
            PluginRuntimeStatus.Loaded => "PLUGIN_STATUS_LOADED",
            PluginRuntimeStatus.ManifestInvalid => "PLUGIN_STATUS_MANIFEST_INVALID",
            PluginRuntimeStatus.CompileFailed => "PLUGIN_STATUS_COMPILE_FAILED",
            PluginRuntimeStatus.InitializationFailed => "PLUGIN_STATUS_INITIALIZATION_FAILED",
            PluginRuntimeStatus.PendingRestart => "PLUGIN_STATUS_PENDING_RESTART",
            PluginRuntimeStatus.UnregisteredPendingRestart =>
                "PLUGIN_STATUS_UNREGISTERED_PENDING_RESTART",
            PluginRuntimeStatus.PendingDeletion => "PLUGIN_STATUS_PENDING_DELETION",
            _ => "PLUGIN_STATUS_UNKNOWN",
        };
        return LocaleService.Get(key);
    }

    private static IBrush getStatusBrush(PluginRuntimeStatus status)
    {
        Color color = status switch
        {
            PluginRuntimeStatus.Loaded => Color.Parse("#81c784"),
            PluginRuntimeStatus.PendingRestart => Color.Parse("#ffb74d"),
            PluginRuntimeStatus.UnregisteredPendingRestart => Color.Parse("#ffb74d"),
            PluginRuntimeStatus.PendingDeletion => Color.Parse("#ffb74d"),
            _ => Color.Parse("#ef9a9a"),
        };
        return new SolidColorBrush(color);
    }
}

internal enum PluginUninstallChoice
{
    UnregisterOnly,
    UnregisterAndDelete,
}
