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
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class PluginManagerWindow : Window
{
    private readonly PluginHost pluginHost;
    private readonly StackPanel pluginList = new() { Spacing = 10 };
    private readonly TextBlock registryDiagnostic = new()
    {
        Foreground = new SolidColorBrush(Color.Parse("#ffb74d")),
        TextWrapping = TextWrapping.Wrap,
        IsVisible = false,
    };
    private readonly TextBlock emptyMessage = new()
    {
        HorizontalAlignment = HorizontalAlignment.Center,
        VerticalAlignment = VerticalAlignment.Center,
        Foreground = new SolidColorBrush(Color.Parse("#9e9e9e")),
        IsVisible = false,
    };
    private readonly Button importButton = new();

    public PluginManagerWindow(PluginHost pluginHost)
    {
        this.pluginHost = pluginHost;
        Title = LocaleService.Get("PLUGIN_MANAGER_TITLE");
        Width = 820;
        Height = 620;
        MinWidth = 620;
        MinHeight = 420;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.Parse("#202124"));
        FontFamily = FontFamily.Parse(
            "avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);

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
        buttons.Children.Add(importButton);
        buttons.Children.Add(closeButton);

        emptyMessage.Text = LocaleService.Get("PLUGIN_EMPTY");
        Grid listGrid = new();
        listGrid.Children.Add(new ScrollViewer
        {
            Content = pluginList,
            VerticalScrollBarVisibility = Avalonia.Controls.Primitives.ScrollBarVisibility.Auto,
        });
        listGrid.Children.Add(emptyMessage);

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

    public static Task ShowAsync(Window owner, PluginHost pluginHost)
    {
        return new PluginManagerWindow(pluginHost).ShowDialog(owner);
    }

    public static async Task<bool> ImportPluginAsync(
        Window owner,
        PluginHost pluginHost)
    {
        IStorageFolder? suggested = Directory.Exists(pluginHost.Environment.PluginsDirectory)
            ? await owner.StorageProvider.TryGetFolderFromPathAsync(
                new Uri(pluginHost.Environment.PluginsDirectory))
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
            await pluginHost.Management.PreviewImportAsync(sourcePath);
        if (!preview.Success)
        {
            await AlertDialog.ShowAsync(
                owner,
                LocaleService.Get("PLUGIN_IMPORT_FAILED"),
                preview.Error);
            return false;
        }

        string warning = LocaleService.Get("PLUGIN_FULL_TRUST_WARNING")
            .Replace("{name}", preview.Name, StringComparison.Ordinal)
            .Replace("{id}", preview.Id, StringComparison.Ordinal)
            .Replace("{path}", preview.SourcePath, StringComparison.Ordinal);
        bool confirmed = await ConfirmationDialog.ShowAsync(
            owner,
            LocaleService.Get("PLUGIN_FULL_TRUST_TITLE"),
            warning);
        if (!confirmed)
            return false;

        PluginManagementResult result =
            await pluginHost.Management.ImportAsync(preview.SourcePath);
        if (!result.Success)
        {
            await AlertDialog.ShowAsync(
                owner,
                LocaleService.Get("PLUGIN_IMPORT_FAILED"),
                result.Error);
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
        importButton.IsEnabled = false;
        try
        {
            if (await ImportPluginAsync(this, pluginHost))
                await refreshAsync();
        }
        finally
        {
            importButton.IsEnabled = true;
        }
    }

    private async Task refreshAsync()
    {
        IReadOnlyList<PluginManagementItem> items =
            await pluginHost.GetManagementItemsAsync();
        List<string> diagnostics = [];
        if (pluginHost.Management.RegistryDiagnostic.Length != 0)
            diagnostics.Add(pluginHost.Management.RegistryDiagnostic);
        diagnostics.AddRange(pluginHost.Management.StartupDiagnostics);
        registryDiagnostic.Text = string.Join(
            Environment.NewLine,
            diagnostics.Distinct(StringComparer.Ordinal));
        registryDiagnostic.IsVisible = registryDiagnostic.Text.Length != 0;
        pluginList.Children.Clear();
        foreach (PluginManagementItem item in items)
            pluginList.Children.Add(createPluginCard(item));
        emptyMessage.IsVisible = items.Count == 0;
    }

    private Control createPluginCard(PluginManagementItem item)
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
                Foreground = new SolidColorBrush(Color.Parse("#aaaaaa")),
                Margin = new Thickness(0, 5, 0, 0),
            };
            TextBlock diagnostic = new()
            {
                Text = item.Diagnostic,
                TextWrapping = TextWrapping.Wrap,
                Foreground = new SolidColorBrush(Color.Parse("#cccccc")),
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
                Tag = item,
            };
            uninstall.Click += onUninstall;
            cardContent.Children.Add(uninstall);
        }
        return new Border
        {
            Background = new SolidColorBrush(Color.Parse("#292a2d")),
            BorderBrush = new SolidColorBrush(Color.Parse("#464646")),
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

    private async void onUninstall(object? sender, Avalonia.Interactivity.RoutedEventArgs args)
    {
        if (sender is not Button { Tag: PluginManagementItem item } button)
            return;
        PluginUninstallChoice? choice = await PluginUninstallDialog.ShowAsync(this, item);
        if (choice is null)
            return;
        button.IsEnabled = false;
        PluginManagementResult result = await pluginHost.Management.UnregisterAsync(
            item.Id,
            choice == PluginUninstallChoice.UnregisterAndDelete);
        if (!result.Success)
        {
            button.IsEnabled = true;
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("PLUGIN_OPERATION_FAILED"),
                result.Error);
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
