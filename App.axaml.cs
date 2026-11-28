using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Ludork.Services;
using Ludork.Services.Plugins;
using Ludork.Views;
using Ludork.Views.Utils;

namespace Ludork;

public partial class App : Application
{
    private EditorSettings? editorSettings;
    private AboutDialog? aboutDialog;
    private IClassicDesktopStyleApplicationLifetime? desktopLifetime;
    private readonly List<string> initialProjects = [];
    private bool ready;
    internal EditorNotificationService? Notifications { get; private set; }

    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
        EditorContextMenuBehavior.Initialize();
#if DEBUG
        this.AttachDeveloperTools();
#endif
    }

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktopLifetime = desktop;
            if (OperatingSystem.IsMacOS() || OperatingSystem.IsWindows())
                desktop.ShutdownMode = ShutdownMode.OnExplicitShutdown;
            desktop.ShutdownRequested += onShutdownRequested;
            IDisposable activation = Window.IsActiveProperty.Changed.AddClassHandler<Window>(
                (window, _) => trackActiveWindow(window));
            desktop.Exit += (_, _) => activation.Dispose();
            IActivatableLifetime? activatableLifetime = this.TryGetFeature<IActivatableLifetime>();
            if (activatableLifetime is not null)
                activatableLifetime.Activated += onActivated;
            editorSettings = EditorSettings.Load();
            EditorLayoutService.Initialize(editorSettings);
            editorSettings.SaveFailed += (_, message) => showSettingsError(message);
            desktop.Exit += (_, _) => EditorLayoutService.Save();
            LocaleService.Initialize(editorSettings.Language);
            Notifications = new EditorNotificationService(desktop);
            desktop.Exit += (_, _) => Notifications.Dispose();
            pluginWorkspace = new PluginWorkspace(LocaleService.CurrentLanguage);
            Task.Run(() => pluginWorkspace.InitializeAsync()).GetAwaiter().GetResult();
            TextHintService.Configure(pluginWorkspace);
            desktop.Exit += (_, _) => pluginWorkspace?.Dispose();
            NativeMenu? applicationMenu = NativeMenu.GetMenu(this);
            if (applicationMenu?.Items.FirstOrDefault() is NativeMenuItem aboutMenuItem)
                aboutMenuItem.Header = LocaleService.Get("ABOUT_TITLE");
            initializeDesktopMenus();
            desktop.Exit += (_, _) => disposeDesktopMenus();
            string? projectArgument = desktop.Args?.FirstOrDefault(argument =>
                !string.IsNullOrWhiteSpace(argument) && !argument.StartsWith("--", StringComparison.Ordinal));
            if (projectArgument is not null)
                initialProjects.Add(projectArgument);
            StartWindow startWindow = createStartWindow();
            startWindow.Opened += (_, _) =>
            {
                ready = true;
                Dispatcher.UIThread.Post(() =>
                {
                    foreach (string path in initialProjects)
                        openExternalProject(path);
                    initialProjects.Clear();
                });
            };
            desktop.MainWindow = startWindow;
            Program.ListenForLaunches(path =>
            {
                if (quitDecision is not null)
                    return false;
                ActivateFromLaunch(path);
                return true;
            });
        }

        base.OnFrameworkInitializationCompleted();
        if (editorSettings?.LastError is string error)
            Avalonia.Threading.Dispatcher.UIThread.Post(() => showSettingsError(error),
                Avalonia.Threading.DispatcherPriority.ContextIdle);
    }

    private void showSettingsError(string message)
    {
        string text = string.Format(LocaleService.Get("EDITOR_SETTINGS_SAVE_FAILED"), message);
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop
            && (desktop.Windows.FirstOrDefault(window => window.IsActive && window.IsVisible)
                ?? desktop.Windows.FirstOrDefault(window => window.IsVisible)) is Window owner)
            new Ludork.Controls.Toast(owner).ShowMessage(text, 6000);
        else
            Console.Error.WriteLine(text);
    }

    public void showAbout(Window owner)
    {
        if (aboutDialog is not null)
        {
            aboutDialog.Activate();
            return;
        }
        aboutDialog = new AboutDialog();
        aboutDialog.Closed += (_, _) => aboutDialog = null;
        aboutDialog.Show(owner);
    }

    private void onOpenAbout(object? sender, EventArgs args)
    {
        if (rootWindows.FirstOrDefault() is Window owner && quitDecision is null)
            showAbout(owner);
    }

    internal void ActivateFromLaunch(string? projectFilePath)
    {
        if (!ready)
        {
            if (projectFilePath is not null)
                initialProjects.Add(projectFilePath);
            return;
        }
        if (projectFilePath is null)
            restoreWindow();
        else
            openExternalProject(projectFilePath);
    }

    private void onActivated(object? sender, ActivatedEventArgs args)
    {
        if (args is FileActivatedEventArgs fileArgs)
        {
            foreach (IStorageItem item in fileArgs.Files.Where(item => item.Path.IsFile
                && item.Path.LocalPath.EndsWith(".proj", StringComparison.OrdinalIgnoreCase)))
                ActivateFromLaunch(item.Path.LocalPath);
        }
        else if (args.Kind == ActivationKind.Reopen)
            ActivateFromLaunch(null);
    }
}
