using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public static class EditorEditWorkflow
{
    public static async Task<T> RunAsync<T>(Window owner, ProjectDataStore store, string title,
        Func<IProgress<EditorOperationProgress>, Task<T>> action, T busyResult)
    {
        try
        {
            return await runAsync(owner, store, title, action, busyResult);
        }
        catch (Exception exception) when (exception is InvalidOperationException or IOException or UnauthorizedAccessException)
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), exception.Message);
            return busyResult;
        }
    }

    private static async Task<T> runAsync<T>(Window owner, ProjectDataStore store, string title,
        Func<IProgress<EditorOperationProgress>, Task<T>> action, T busyResult)
    {
        if (!store.EditOperations.TryBegin())
            return busyResult;
        Task displayDelay = Task.Delay(200);
        EditorOperationProgressWindow? dialog = null;
        DispatcherTimer? progressTimer = null;
        List<(Window Window, bool HitTestVisible)> windows = [];
        void blockClosing(object? sender, WindowClosingEventArgs args) => args.Cancel = true;
        void blockInput(object? sender, KeyEventArgs args) => args.Handled = true;
        try
        {
            Window root = owner;
            while (root.Owner is Window parent)
                root = parent;
            IEnumerable<Window> candidates = Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop
                ? desktop.Windows : new[] { owner };
            foreach (Window window in candidates.Where(window => belongsTo(window, root)).ToArray())
            {
                windows.Add((window, window.IsHitTestVisible));
                window.SetCurrentValue(InputElement.IsHitTestVisibleProperty, false);
                window.Closing += blockClosing;
                window.AddHandler(InputElement.KeyDownEvent, blockInput, RoutingStrategies.Tunnel);
            }
            EditorOperationProgressReporter progress = new();
            EditorOperationProgress? displayed = null;
            progressTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(50) };
            progressTimer.Tick += (_, _) =>
            {
                EditorOperationProgress latest = progress.Current;
                if (dialog is not null && !ReferenceEquals(displayed, latest))
                {
                    displayed = latest;
                    dialog.Update(latest);
                }
            };
            progressTimer.Start();
            await Dispatcher.UIThread.InvokeAsync(() => { }, DispatcherPriority.Background);
            Task<T> operation = action(progress);
            if (await Task.WhenAny(operation, displayDelay) != operation)
            {
                dialog = new EditorOperationProgressWindow(title);
                dialog.Update(progress.Current);
                _ = dialog.ShowDialog(owner);
            }
            return await operation;
        }
        finally
        {
            progressTimer?.Stop();
            dialog?.Finish();
            foreach ((Window window, bool hitTestVisible) in windows)
            {
                window.Closing -= blockClosing;
                window.RemoveHandler(InputElement.KeyDownEvent, blockInput);
                window.SetCurrentValue(InputElement.IsHitTestVisibleProperty, hitTestVisible);
            }
            store.EditOperations.Complete();
        }
    }

    private static bool belongsTo(Window window, Window root)
    {
        for (Window? current = window; current is not null; current = current.Owner as Window)
            if (ReferenceEquals(current, root))
                return true;
        return false;
    }
}
