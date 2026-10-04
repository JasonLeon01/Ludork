using Avalonia.Controls;
using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using Ludork.Services.BlueprintAssistant;
using Ludork.Services.Plugins;
using Ludork.Views;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork;

public partial class App
{
    private PluginWorkspace? pluginWorkspace;
    private readonly HashSet<PluginHost> notifiedPluginHosts = [];

    public async Task importPluginAsync(Window owner)
    {
        if (pluginWorkspace is not null)
            await PluginManagerWindow.ImportPluginAsync(owner, pluginWorkspace, (owner as MainWindow)?.ProjectPath);
    }

    public async Task showPluginManagerAsync(Window owner)
    {
        if (pluginWorkspace is not null)
            await PluginManagerWindow.ShowAsync(owner, pluginWorkspace, (owner as MainWindow)?.ProjectPath);
    }

    internal void installPluginMenus(
        Window owner,
        string? projectPath,
        params (PluginMenuLocation Location, NativeMenu Menu)[] targets)
    {
        if (pluginWorkspace is null)
            return;
        IReadOnlyList<RegisteredPluginMenuCommand> registrations = pluginWorkspace.GetMenuCommands(projectPath);
        foreach ((PluginMenuLocation location, NativeMenu menu) in targets)
        {
            RegisteredPluginMenuCommand[] commands = registrations
                .Where(registration => registration.Command.Location == location)
                .ToArray();
            if (commands.Length == 0)
                continue;
            if (menu.Items.Count != 0)
                menu.Items.Add(new NativeMenuItemSeparator());
            foreach (RegisteredPluginMenuCommand registration in commands)
            {
                NativeMenuItem item = new()
                {
                    Header = registration.Command.Label,
                };
                item.Click += async (_, _) =>
                    await executePluginMenuCommandAsync(owner, projectPath, registration);
                menu.Items.Add(item);
            }
        }
    }

    internal void appendPluginMapContextMenuCommands(
        MainWindow owner,
        ContextMenu menu,
        string mapKey,
        IMapEditorHost mapEditorHost)
    {
        if (pluginWorkspace is null)
            return;
        IReadOnlyList<RegisteredPluginMapContextMenuCommand> registrations =
            pluginWorkspace.GetMapContextMenuCommands(mapEditorHost.ProjectPath);
        if (registrations.Count == 0)
            return;
        if (menu.Items.Count != 0)
            menu.Items.Add(new Separator());
        foreach (RegisteredPluginMapContextMenuCommand registration in registrations)
        {
            MenuItem item = new MenuItem
            {
                Header = registration.Command.Label,
            };
            item.Click += async (_, _) =>
                await executePluginMapContextMenuCommandAsync(
                    owner,
                    mapKey,
                    mapEditorHost,
                    registration);
            menu.Items.Add(item);
        }
    }

    private async Task executePluginMenuCommandAsync(
        Window owner,
        string? projectPath,
        RegisteredPluginMenuCommand registration)
    {
        PluginWindowUserInterface userInterface = new(owner);
        PluginMenuContext context = new(
            projectPath,
            LocaleService.CurrentLanguage,
            registration.PluginDirectory,
            registration.PluginDataDirectory,
            userInterface,
            TextHintService.Refresh,
            new PluginSecretStore(registration.SecretStoreId),
            owner is MainWindow mainWindow
                ? mainWindow.CreateBlueprintAssistantHost()
                : null,
            owner is MainWindow cleanupWindow
                ? cleanupWindow.CreateResourceCleanupHost()
                : null,
            CancellationToken.None);
        await executePluginOperationAsync(
            owner,
            userInterface,
            () => registration.Command.Handler(context));
    }

    private async Task executePluginMapContextMenuCommandAsync(
        MainWindow owner,
        string mapKey,
        IMapEditorHost mapEditorHost,
        RegisteredPluginMapContextMenuCommand registration)
    {
        PluginWindowUserInterface userInterface = new(owner);
        PluginMapContextMenuContext context = new(
            mapEditorHost.ProjectPath,
            mapKey,
            LocaleService.CurrentLanguage,
            registration.PluginDirectory,
            registration.PluginDataDirectory,
            userInterface,
            TextHintService.Refresh,
            new PluginSecretStore(registration.SecretStoreId),
            mapEditorHost,
            CancellationToken.None);
        await executePluginOperationAsync(
            owner,
            userInterface,
            () => registration.Command.Handler(context));
    }

    private static async Task executePluginOperationAsync(
        Window owner,
        PluginWindowUserInterface userInterface,
        Func<Task<PluginResult>> operation)
    {
        try
        {
            PluginResult result = await operation();
            if (!result.Success
                && !userInterface.MessageShown
                && !string.IsNullOrWhiteSpace(result.Error))
            {
                await AlertDialog.ShowAsync(
                    owner,
                    LocaleService.Get("PLUGIN_OPERATION_FAILED"),
                    result.Error);
            }
        }
        catch (OperationCanceledException)
        {
        }
        catch (Exception exception)
        {
            await AlertDialog.ShowAsync(
                owner,
                LocaleService.Get("PLUGIN_OPERATION_FAILED"),
                exception.ToString());
        }
    }

    private void registerPluginFailureNotification(Window window)
    {
        if (pluginWorkspace is null)
            return;
        List<PluginHost> hosts = [pluginWorkspace.GlobalHost];
        PluginHost? project = pluginWorkspace.GetProjectHost((window as MainWindow)?.ProjectPath);
        if (project is not null)
            hosts.Add(project);
        window.Opened += async (_, _) =>
        {
            PluginHost[] failures = hosts.Where(host => host.HasStartupFailures
                && !notifiedPluginHosts.Contains(host)).ToArray();
            if (failures.Length == 0)
                return;
            foreach (PluginHost host in failures)
                notifiedPluginHosts.Add(host);
            string message = string.Join(Environment.NewLine + Environment.NewLine,
                failures.Select(host => host.Environment.RegistryPath + Environment.NewLine
                    + host.StartupFailureSummary));
            await AlertDialog.ShowAsync(
                window,
                LocaleService.Get("PLUGIN_STARTUP_FAILURES_TITLE"),
                message);
        };
    }

    private sealed class PluginWindowUserInterface : IAvaloniaPluginUserInterface
    {
        private readonly Window owner;

        public PluginWindowUserInterface(Window owner)
        {
            this.owner = owner;
        }

        public bool MessageShown { get; private set; }

        public Window Owner => owner;

        public async Task ShowMessageAsync(
            string title,
            string message,
            PluginMessageKind kind,
            CancellationToken cancellationToken)
        {
            cancellationToken.ThrowIfCancellationRequested();
            MessageShown = true;
            await AlertDialog.ShowAsync(owner, title, message);
        }
    }
}
