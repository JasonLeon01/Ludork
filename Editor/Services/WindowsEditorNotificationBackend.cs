using Avalonia;
using Avalonia.Labs.Notifications;
using Avalonia.Threading;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Versioning;

namespace Ludork.Services;

[SupportedOSPlatform("windows")]
internal sealed class WindowsEditorNotificationBackend : IEditorNotificationBackend
{
    private readonly INativeNotificationManager manager;
    private readonly Dictionary<uint, INativeNotification> notifications = [];
    private readonly Dictionary<uint, uint> identifiers = [];
    private bool disposed;

    internal WindowsEditorNotificationBackend(INativeNotificationManager manager)
    {
        this.manager = manager;
        manager.NotificationCompleted += onNotificationCompleted;
    }

    public event EventHandler<EditorNotificationCompletion>? Completed;

    public static WindowsEditorNotificationBackend? Create()
    {
        AppNotificationOptions options = new() { AppName = "Ludork", ClearOnAppClose = true };
        AppBuilder builder = AppBuilder.Configure<App>().WithAppNotifications(options);
        builder.AfterSetupCallback?.Invoke(builder);
        return NativeNotificationManager.Current is INativeNotificationManager manager
            ? new WindowsEditorNotificationBackend(manager) : null;
    }

    public void Show(uint id, string title, string message)
    {
        Dispatcher.UIThread.VerifyAccess();
        ObjectDisposedException.ThrowIf(disposed, this);
        INativeNotification? notification = manager.CreateNotification(null);
        if (notification is null)
        {
            Completed?.Invoke(this, new(id, EditorNotificationStatus.Failed, "Could not create a system notification."));
            return;
        }
        notification.Title = title;
        notification.Message = message;
        notifications.Add(id, notification);
        identifiers.Add(notification.Id, id);
        notification.Show();
    }

    public void Close(uint id)
    {
        Dispatcher.UIThread.VerifyAccess();
        if (!notifications.Remove(id, out INativeNotification? notification))
            return;
        identifiers.Remove(notification.Id);
        notification.Close();
    }

    public void Dispose()
    {
        Dispatcher.UIThread.VerifyAccess();
        if (disposed)
            return;
        disposed = true;
        manager.NotificationCompleted -= onNotificationCompleted;
        foreach (uint id in notifications.Keys.ToArray())
            Close(id);
    }

    private void onNotificationCompleted(object? sender, NativeNotificationCompletedEventArgs args)
    {
        Dispatcher.UIThread.Post(() =>
        {
            if (!disposed && args.NotificationId is uint nativeId && identifiers.TryGetValue(nativeId, out uint id))
                Completed?.Invoke(this, new(id, args.IsActivated ? EditorNotificationStatus.Activated : EditorNotificationStatus.Dismissed));
        });
    }
}
