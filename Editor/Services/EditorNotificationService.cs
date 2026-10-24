using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Threading;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace Ludork.Services;

internal sealed class EditorNotificationService : IDisposable
{
    private readonly IClassicDesktopStyleApplicationLifetime desktop;
    private readonly IEditorNotificationBackend? backend;
    private readonly Dictionary<uint, PendingNotification> pending = [];
    private uint nextNotificationId;
    private bool disposed;

    public EditorNotificationService(IClassicDesktopStyleApplicationLifetime desktop)
        : this(desktop, createBackend())
    {
    }

    internal EditorNotificationService(IClassicDesktopStyleApplicationLifetime desktop, IEditorNotificationBackend? backend)
    {
        this.desktop = desktop;
        this.backend = backend;
        if (backend is not null)
            backend.Completed += onNotificationCompleted;
    }

    private static IEditorNotificationBackend? createBackend()
    {
        try
        {
            if (OperatingSystem.IsMacOS())
                return new MacOSEditorNotificationBackend();
            if (OperatingSystem.IsWindows())
                return WindowsEditorNotificationBackend.Create();
            return null;
        }
        catch (Exception exception)
        {
            Trace.WriteLine("System notifications are unavailable: " + exception.Message);
            return null;
        }
    }

    public void ShowCompleted(Window project, Window target, string projectPath, string operationKey,
        bool success, Action? showResult = null)
    {
        Dispatcher.UIThread.VerifyAccess();
        if (disposed || backend is null || !desktop.Windows.Contains(project) || !desktop.Windows.Contains(target)
            || desktop.MainWindow != project || desktop.Windows.Any(window => window.IsActive))
            return;
        uint? notificationId = null;
        try
        {
            uint id = checked(++nextNotificationId);
            string title = "Ludork — " + Path.GetFileName(Path.TrimEndingDirectorySeparator(projectPath));
            string message = string.Format(LocaleService.Get(success ? "TASK_NOTIFICATION_SUCCESS" : "TASK_NOTIFICATION_FAILED"),
                LocaleService.Get(operationKey));
            PendingNotification entry = new(project, target, showResult);
            pending.Add(id, entry);
            notificationId = id;
            project.Closed += onWindowClosed;
            if (target != project)
                target.Closed += onWindowClosed;
            backend.Show(id, title, message);
        }
        catch (Exception exception)
        {
            if (notificationId is uint id)
                remove(id);
            Trace.WriteLine("Could not show system notification: " + exception.Message);
        }
    }

    public void Dispose()
    {
        Dispatcher.UIThread.VerifyAccess();
        if (disposed)
            return;
        disposed = true;
        if (backend is not null)
            backend.Completed -= onNotificationCompleted;
        foreach (uint id in pending.Keys.ToArray())
            remove(id);
        backend?.Dispose();
    }

    private void onWindowClosed(object? sender, EventArgs args)
    {
        foreach (uint id in pending.Where(pair => pair.Value.Project == sender || pair.Value.Target == sender)
                     .Select(pair => pair.Key).ToArray())
            remove(id);
    }

    private void onNotificationCompleted(object? sender, EditorNotificationCompletion args)
    {
        Dispatcher.UIThread.Post(() =>
        {
            if (disposed || !pending.TryGetValue(args.Id, out PendingNotification? entry))
                return;
            remove(args.Id);
            if (args.Status is EditorNotificationStatus.Denied or EditorNotificationStatus.Failed)
                Trace.WriteLine("System notification was not shown: " + (args.Error ?? args.Status.ToString()));
            if (args.Status != EditorNotificationStatus.Activated || desktop.MainWindow != entry.Project
                || !desktop.Windows.Contains(entry.Project) || !desktop.Windows.Contains(entry.Target))
                return;
            restore(entry.Project);
            entry.ShowResult?.Invoke();
            Window target = entry.Target;
            while (desktop.Windows.LastOrDefault(window => window.Owner == target && window.IsVisible && window.IsDialog) is Window modal)
                target = modal;
            restore(target);
            target.Activate();
        });
    }

    private void remove(uint id)
    {
        if (!pending.Remove(id, out PendingNotification? entry))
            return;
        entry.Project.Closed -= onWindowClosed;
        if (entry.Target != entry.Project)
            entry.Target.Closed -= onWindowClosed;
        try
        {
            backend?.Close(id);
        }
        catch (Exception exception)
        {
            Trace.WriteLine("Could not close system notification: " + exception.Message);
        }
    }

    private static void restore(Window window)
    {
        if (window.WindowState == WindowState.Minimized)
            window.WindowState = WindowState.Normal;
    }

    private sealed record PendingNotification(Window Project, Window Target, Action? ShowResult);
}
