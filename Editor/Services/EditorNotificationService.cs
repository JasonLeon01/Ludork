using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Labs.Notifications;
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
    private readonly INativeNotificationManager? manager;
    private readonly Dictionary<uint, PendingNotification> pending = [];
    private bool disposed;

    public EditorNotificationService(IClassicDesktopStyleApplicationLifetime desktop)
        : this(desktop, createManager())
    {
    }

    internal EditorNotificationService(IClassicDesktopStyleApplicationLifetime desktop, INativeNotificationManager? manager)
    {
        this.desktop = desktop;
        this.manager = manager;
        if (manager is not null)
            manager.NotificationCompleted += onNotificationCompleted;
    }

    private static INativeNotificationManager? createManager()
    {
        if (!OperatingSystem.IsWindows() && !OperatingSystem.IsMacOS())
            return null;
        try
        {
            AppNotificationOptions options = new() { ClearOnAppClose = true };
            if (OperatingSystem.IsWindows())
                options = new AppNotificationOptions { AppName = "Ludork", ClearOnAppClose = true };
            AppBuilder notifications = AppBuilder.Configure<App>().WithAppNotifications(options);
            notifications.AfterSetupCallback?.Invoke(notifications);
            return NativeNotificationManager.Current;
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
        if (disposed || manager is null || !desktop.Windows.Contains(project) || !desktop.Windows.Contains(target)
            || desktop.MainWindow != project || desktop.Windows.Any(window => window.IsActive))
            return;
        uint? notificationId = null;
        try
        {
            INativeNotification? notification = manager.CreateNotification(null);
            if (notification is null)
                return;
            notification.Title = "Ludork — " + Path.GetFileName(Path.TrimEndingDirectorySeparator(projectPath));
            notification.Message = string.Format(LocaleService.Get(success ? "TASK_NOTIFICATION_SUCCESS" : "TASK_NOTIFICATION_FAILED"),
                LocaleService.Get(operationKey));
            PendingNotification entry = new(notification, project, target, showResult);
            pending.Add(notification.Id, entry);
            notificationId = notification.Id;
            project.Closed += onWindowClosed;
            if (target != project)
                target.Closed += onWindowClosed;
            notification.Show();
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
        if (disposed)
            return;
        disposed = true;
        if (manager is not null)
            manager.NotificationCompleted -= onNotificationCompleted;
        foreach (uint id in pending.Keys.ToArray())
            remove(id);
    }

    private void onWindowClosed(object? sender, EventArgs args)
    {
        foreach (uint id in pending.Where(pair => pair.Value.Project == sender || pair.Value.Target == sender)
                     .Select(pair => pair.Key).ToArray())
            remove(id);
    }

    private void onNotificationCompleted(object? sender, NativeNotificationCompletedEventArgs args)
    {
        Dispatcher.UIThread.Post(() =>
        {
            if (disposed || args.NotificationId is not uint id || !pending.TryGetValue(id, out PendingNotification? entry))
                return;
            remove(id);
            if (!args.IsActivated || desktop.MainWindow != entry.Project
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
            entry.Notification.Close();
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

    private sealed record PendingNotification(INativeNotification Notification, Window Project, Window Target, Action? ShowResult);
}
