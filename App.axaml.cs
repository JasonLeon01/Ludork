using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Avalonia.Platform.Storage;
using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Ludork.Services;
using Ludork.Services.Plugins;
using Ludork.ViewModels;
using Ludork.Views;

namespace Ludork;

public partial class App : Application
{
    private EditorSettings? editorSettings;
    private AboutDialog? aboutDialog;
    private string? pendingProjectPath;

    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
#if DEBUG
        this.AttachDeveloperTools();
#endif
    }

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            IActivatableLifetime? activatableLifetime = this.TryGetFeature<IActivatableLifetime>();
            if (activatableLifetime is not null)
                activatableLifetime.Activated += onActivated;
            editorSettings = EditorSettings.Load();
            LocaleService.Initialize(editorSettings.Language);
            pluginHost = new PluginHost(LocaleService.CurrentLanguage);
            Task.Run(() => pluginHost.InitializeAsync()).GetAwaiter().GetResult();
            TextHintService.Configure(pluginHost);
            desktop.Exit += (_, _) => pluginHost?.Dispose();
            NativeMenu? applicationMenu = NativeMenu.GetMenu(this);
            if (applicationMenu?.Items.FirstOrDefault() is NativeMenuItem aboutMenuItem)
                aboutMenuItem.Header = LocaleService.Get("ABOUT_TITLE");
            string? projectArgument = pendingProjectPath ?? (desktop.Args ?? Array.Empty<string>())
                .FirstOrDefault(argument => !string.IsNullOrWhiteSpace(argument) && !argument.StartsWith("--"));
            pendingProjectPath = null;
            MainWindow? projectWindow = string.IsNullOrWhiteSpace(projectArgument)
                ? null
                : createProjectWindow(projectArgument);
            if (projectWindow is not null)
            {
                desktop.MainWindow = projectWindow;
            }
            else
            {
                StartWindow startWindow = new StartWindow(editorSettings)
                {
                    DataContext = new StartViewModel(),
                };
                registerPluginFailureNotification(startWindow);
                desktop.MainWindow = startWindow;
            }
        }

        base.OnFrameworkInitializationCompleted();
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
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop
            && desktop.MainWindow is Window owner)
        {
            showAbout(owner);
        }
    }

    private void onActivated(object? sender, ActivatedEventArgs args)
    {
        if (args is not FileActivatedEventArgs fileArgs)
            return;
        IStorageItem? projectItem = fileArgs.Files.FirstOrDefault(item =>
            item.Path.IsFile
            && item.Path.LocalPath.EndsWith(".proj", StringComparison.OrdinalIgnoreCase)
            && File.Exists(item.Path.LocalPath));
        if (projectItem is null)
            return;
        string projectPath = projectItem.Path.LocalPath;
        if (ApplicationLifetime is not IClassicDesktopStyleApplicationLifetime desktop
            || editorSettings is null
            || desktop.MainWindow is null)
        {
            pendingProjectPath = projectPath;
            return;
        }
        openProject(desktop.MainWindow, projectPath);
    }

    public bool openProject(Window currentWindow, string projectFilePath)
    {
        if (ApplicationLifetime is not IClassicDesktopStyleApplicationLifetime desktop)
            return false;
        MainWindow? mainWindow = createProjectWindow(projectFilePath);
        if (mainWindow is null)
            return false;

        desktop.MainWindow = mainWindow;
        mainWindow.Show();
        if (currentWindow is MainWindow previousMain)
            previousMain.CloseForProjectSwitch();
        else
            currentWindow.Close();
        return true;
    }

    private MainWindow? createProjectWindow(string projectFilePath)
    {
        if (editorSettings is null || !Path.IsPathFullyQualified(projectFilePath))
            return null;
        string fullPath = Path.GetFullPath(projectFilePath);
        if (!fullPath.EndsWith(".proj", StringComparison.OrdinalIgnoreCase) || !File.Exists(fullPath))
            return null;
        string? projectPath = Path.GetDirectoryName(fullPath);
        if (string.IsNullOrWhiteSpace(projectPath))
            return null;

        editorSettings.setLastOpenPath(projectPath);
        TextHintService.SetProjectPath(projectPath);
        MainWindow mainWindow = new MainWindow(editorSettings, projectPath)
        {
            DataContext = new MainViewModel(projectPath),
        };
        registerPluginFailureNotification(mainWindow);
        return mainWindow;
    }
}
