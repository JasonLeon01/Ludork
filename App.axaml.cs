using Ludork.Composition;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Avalonia.Platform.Storage;
using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Ludork.Services;
using Ludork.Services.Plugins;
using Ludork.ViewModels;
using Ludork.Views;
using Ludork.Views.Utils;

namespace Ludork;

public partial class App : Application
{
    private EditorSettings? editorSettings;
    private AboutDialog? aboutDialog;
    private string? pendingProjectPath;
    private bool openingProject;
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
            string? projectArgument = pendingProjectPath ?? (desktop.Args ?? Array.Empty<string>())
                .FirstOrDefault(argument => !string.IsNullOrWhiteSpace(argument) && !argument.StartsWith("--"));
            pendingProjectPath = null;
            StartWindow startWindow = new(editorSettings)
            {
                DataContext = new StartViewModel(editorSettings),
            };
            registerPluginFailureNotification(startWindow);
            if (!string.IsNullOrWhiteSpace(projectArgument))
                startWindow.Opened += (_, _) => openProject(startWindow, projectArgument);
            desktop.MainWindow = startWindow;
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

    public async void openProject(Window currentWindow, string projectFilePath)
    {
        if (openingProject || ApplicationLifetime is not IClassicDesktopStyleApplicationLifetime desktop)
            return;
        openingProject = true;
        using CancellationTokenSource cancellation = new();
        void cancel(object? sender, EventArgs args) => cancellation.Cancel();
        currentWindow.Closed += cancel;
        try
        {
            MainWindow? mainWindow = await createProjectWindowAsync(currentWindow, projectFilePath, cancellation.Token);
            if (mainWindow is null)
                return;
            desktop.MainWindow = mainWindow;
            mainWindow.Show();
            if (currentWindow is MainWindow previousMain)
                previousMain.CloseForProjectSwitch();
            else
                currentWindow.Close();
        }
        catch (OperationCanceledException)
        {
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException
            or InvalidOperationException or System.Text.Json.JsonException or ProjectStateCheckException)
        {
            if (!cancellation.IsCancellationRequested)
                await AlertDialog.ShowAsync(currentWindow, LocaleService.Get("ERROR"), exception.Message);
        }
        finally
        {
            currentWindow.Closed -= cancel;
            openingProject = false;
        }
    }

    private async Task<MainWindow?> createProjectWindowAsync(Window owner, string projectFilePath, CancellationToken token)
    {
        if (editorSettings is null || !Path.IsPathFullyQualified(projectFilePath))
            return null;
        string fullPath = Path.GetFullPath(projectFilePath);
        if (!fullPath.EndsWith(".proj", StringComparison.OrdinalIgnoreCase) || !File.Exists(fullPath))
            return null;
        string? projectPath = Path.GetDirectoryName(fullPath);
        if (string.IsNullOrWhiteSpace(projectPath))
            return null;

        string? error = ProjectVersionService.GetOpenError(fullPath);
        if (error is not null)
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), error);
            return null;
        }
        if (pluginWorkspace is not null)
            await pluginWorkspace.InitializeProjectAsync(projectPath, token);
        token.ThrowIfCancellationRequested();
        ProjectDataStore? gameData = await EditorProjectOpenWorkflow.LoadAsync(owner, projectPath, token);
        if (token.IsCancellationRequested)
        {
            gameData?.Dispose();
            token.ThrowIfCancellationRequested();
        }
        if (gameData is null)
        {
            (ApplicationLifetime as IClassicDesktopStyleApplicationLifetime)?.Shutdown();
            return null;
        }
        TextHintService.SetProjectPath(projectPath);
        EditorProjectSession session = new(gameData);
        MainWindow mainWindow = new(editorSettings, session);
        registerPluginFailureNotification(mainWindow);
        editorSettings.recordOpenedProject(fullPath);
        return mainWindow;
    }
}
