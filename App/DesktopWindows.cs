using Avalonia.Controls;
using Avalonia.Threading;
using Ludork.Composition;
using Ludork.Services;
using Ludork.ViewModels;
using Ludork.Views;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork;

public partial class App
{
    private readonly List<Window> rootWindows = [];
    private readonly Dictionary<string, Window> projectWindows = new(StringComparer.Ordinal);
    private readonly Dictionary<string, PendingOpen> openingProjects = new(StringComparer.Ordinal);
    private Window? pickerOwner;
    private WeakReference<Window>? lastActiveWindow;

    private sealed class PendingOpen(Window owner)
    {
        public Window Owner { get; } = owner;
        public CancellationTokenSource Cancellation { get; } = new();
        public TaskCompletionSource Finished { get; } = new(TaskCreationOptions.RunContinuationsAsynchronously);
    }

    private StartWindow createStartWindow()
    {
        StartWindow window = new(editorSettings!) { DataContext = new StartViewModel(editorSettings!) };
        registerRoot(window);
        return window;
    }

    private void registerRoot(Window window, string? projectKey = null)
    {
        rootWindows.Insert(0, window);
        window.Closing += (_, args) =>
        {
            if (quitDecision is not null && !committingQuit)
                args.Cancel = true;
        };
        window.Closed += (_, _) =>
        {
            rootWindows.Remove(window);
            if (projectKey is not null)
                projectWindows.Remove(projectKey);
            if (desktopLifetime!.MainWindow == window)
                desktopLifetime.MainWindow = rootWindows.FirstOrDefault();
            Dispatcher.UIThread.Post(retentionChanged);
        };
        registerPluginFailureNotification(window);
    }

    private void trackActiveWindow(Window window)
    {
        if (!window.IsActive)
            return;
        Window root = window;
        while (root.Owner is Window parent)
            root = parent;
        if (!rootWindows.Remove(root))
            return;
        rootWindows.Insert(0, root);
        desktopLifetime!.MainWindow = root;
        lastActiveWindow = new(window);
    }

    private void newWindow()
    {
        if (quitDecision is null)
            createStartWindow().Show();
    }

    private void restoreWindow()
    {
        if (quitDecision is not null)
            return;
        if (lastActiveWindow?.TryGetTarget(out Window? target) == true && target.IsVisible)
            activateWindow(target);
        else if (rootWindows.FirstOrDefault() is Window window)
            activateWindow(window);
        else
            newWindow();
    }

    private void activateWindow(Window window)
    {
        for (Window? owner = window; owner is not null; owner = owner.Owner as Window)
        {
            if (owner.WindowState == WindowState.Minimized)
                owner.WindowState = WindowState.Normal;
        }
        while (desktopLifetime!.Windows.LastOrDefault(child =>
            child.Owner == window && child.IsDialog && child.IsVisible) is Window dialog)
            window = dialog;
        window.Activate();
    }

    private async Task pickExternalProjectAsync()
    {
        if (quitDecision is not null)
            return;
        if (pickerOwner is not null)
        {
            activateWindow(pickerOwner);
            return;
        }
        Window? previous = rootWindows.FirstOrDefault(window => window.IsActive);
        Window? existing = rootWindows.FirstOrDefault(window => window.WindowState != WindowState.Minimized
            && !desktopLifetime!.Windows.Any(dialog => dialog.IsDialog && dialog.Owner == window));
        Window owner = existing ?? createStartWindow();
        pickerOwner = owner;
        string? path = null;
        try
        {
            if (existing is null)
                owner.Show();
            path = await ProjectFilePicker.PickAsync(owner, editorSettings!);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or InvalidOperationException)
        {
            if (owner.IsVisible && quitDecision is null)
                await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), exception.Message);
        }
        finally
        {
            pickerOwner = null;
            if (existing is null && (path is null || quitDecision is not null))
            {
                owner.Close();
                if (previous?.IsVisible == true)
                    activateWindow(previous);
            }
        }
        if (path is not null && quitDecision is null)
            openExternalProject(path, existing is null ? owner : null);
    }

    private void openExternalProject(string path) => openExternalProject(path, null);

    private void openExternalProject(string path, Window? temporaryOwner)
    {
        if (quitDecision is not null)
            return;
        string? key = getProjectKey(path);
        if (key is null)
        {
            temporaryOwner?.Close();
            editorSettings?.removeMissingRecentProjects();
            return;
        }
        if (activateExistingProject(key, path))
        {
            temporaryOwner?.Close();
            return;
        }
        Window owner = temporaryOwner ?? rootWindows.OfType<StartWindow>().FirstOrDefault(window =>
            window != pickerOwner && !openingProjects.Values.Any(open => open.Owner == window)
            && !desktopLifetime!.Windows.Any(dialog => dialog.Owner == window && dialog.IsDialog)) ?? createStartWindow();
        if (!owner.IsVisible)
            owner.Show();
        activateWindow(owner);
        openProject(owner, path);
    }

    private static string? getProjectKey(string path)
    {
        if (!Path.IsPathFullyQualified(path) || !path.EndsWith(".proj", StringComparison.OrdinalIgnoreCase)
            || !File.Exists(path))
            return null;
        return FileSystemPathIdentity.GetKey(Path.GetDirectoryName(Path.GetFullPath(path))!);
    }

    private bool activateExistingProject(string key, string path)
    {
        if (projectWindows.TryGetValue(key, out Window? existing))
        {
            editorSettings!.recordOpenedProject(path);
            activateWindow(existing);
            return true;
        }
        if (!openingProjects.TryGetValue(key, out PendingOpen? pending))
            return false;
        activateWindow(pending.Owner);
        return true;
    }

    public async void openProject(Window currentWindow, string projectFilePath)
    {
        if (quitDecision is not null || getProjectKey(projectFilePath) is not string key
            || !rootWindows.Contains(currentWindow) || activateExistingProject(key, projectFilePath)
            || openingProjects.Values.Any(open => open.Owner == currentWindow))
            return;
        PendingOpen pending = new(currentWindow);
        openingProjects.Add(key, pending);
        using CancellationTokenSource cancellation = pending.Cancellation;
        void cancel(object? sender, EventArgs args) => cancellation.Cancel();
        currentWindow.Closed += cancel;
        try
        {
            string? error = ProjectVersionService.GetOpenError(projectFilePath);
            if (error is not null)
            {
                await showOpenError(currentWindow, error, cancellation.Token);
                return;
            }
            string directory = Path.GetDirectoryName(Path.GetFullPath(projectFilePath))!;
            ProjectDataStore? data = await EditorProjectOpenWorkflow.LoadAsync(
                currentWindow, directory, pluginWorkspace, cancellation.Token);
            if (data is null)
                return;
            if (quitDecision is not null)
                await quitDecision.Task;
            if (cancellation.IsCancellationRequested)
            {
                data.Dispose();
                return;
            }
            showProjectWindow(data, key);
            editorSettings!.recordOpenedProject(projectFilePath);
            currentWindow.Closed -= cancel;
            if (currentWindow is MainWindow previous)
                await previous.CloseConfirmedAsync();
            else
                currentWindow.Close();
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested)
        {
        }
        catch (Exception exception) when (exception is IOException or InvalidDataException or UnauthorizedAccessException
            or InvalidOperationException or System.Text.Json.JsonException or ProjectStateCheckException)
        {
            if (!cancellation.IsCancellationRequested)
                await showOpenError(currentWindow, exception.Message, cancellation.Token);
        }
        finally
        {
            currentWindow.Closed -= cancel;
            openingProjects.Remove(key);
            pending.Finished.TrySetResult();
        }
    }

    private void showProjectWindow(ProjectDataStore data, string key)
    {
        EditorProjectSession? session = null;
        MainWindow? window = null;
        try
        {
            session = new EditorProjectSession(data);
            window = new MainWindow(editorSettings!, session);
            registerRoot(window, key);
            projectWindows.Add(key, window);
            window.Show();
        }
        catch
        {
            if (window is not null)
            {
                rootWindows.Remove(window);
                projectWindows.Remove(key);
                if (desktopLifetime!.MainWindow == window)
                    desktopLifetime.MainWindow = rootWindows.FirstOrDefault();
            }
            if (session is null)
                data.Dispose();
            else
                session.Dispose();
            throw;
        }
    }

    private static async Task showOpenError(Window owner, string message, CancellationToken token)
    {
        AlertDialog dialog = new(LocaleService.Get("ERROR"), message);
        using CancellationTokenRegistration registration = token.Register(() =>
            Dispatcher.UIThread.Post(() => { if (dialog.IsVisible) dialog.Close(); }));
        await dialog.ShowDialog(owner);
    }
}
