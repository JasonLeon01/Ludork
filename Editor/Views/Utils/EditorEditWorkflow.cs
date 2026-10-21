using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Services;
using Ludork.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public static class EditorEditWorkflow
{
    private static readonly ConditionalWeakTable<Window, EditorEditOperationService> windowOperations = new();

    public static Task<T> RunAsync<T>(Window owner, ProjectDataStore store, string title,
        Func<IProgress<EditorOperationProgress>, Task<T>> action, T busyResult, bool showProgress = true)
        => runWithFeedbackAsync(owner, store.EditOperations, title, action, busyResult, showProgress);

    public static Task<T> RunAsync<T>(Window owner, string title,
        Func<IProgress<EditorOperationProgress>, Task<T>> action, T busyResult, bool showProgress = true)
        => runWithFeedbackAsync(owner, (getRoot(owner).DataContext as MainViewModel)?.GameData.EditOperations,
            title, action, busyResult, showProgress);

    private static async Task<T> runWithFeedbackAsync<T>(Window owner, EditorEditOperationService? operations,
        string title, Func<IProgress<EditorOperationProgress>, Task<T>> action, T busyResult, bool showProgress)
    {
        try
        {
            return await runAsync(owner, operations, title, action, busyResult, showProgress);
        }
        catch (Exception exception) when (exception is InvalidOperationException or IOException or UnauthorizedAccessException)
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("ERROR"), exception.Message);
            return busyResult;
        }
    }

    private static async Task<T> runAsync<T>(Window owner, EditorEditOperationService? operations, string title,
        Func<IProgress<EditorOperationProgress>, Task<T>> action, T busyResult, bool showProgress)
    {
        Window root = getRoot(owner);
        EditorEditOperationService windowOperation = windowOperations.GetOrCreateValue(root);
        if (windowOperation.IsBusy || operations?.IsBusy == true)
            return busyResult;
        windowOperation.TryBegin();
        operations?.TryBegin();
        EditorOperationProgressPresenter progress = new(owner, title, showProgress);
        List<(Window Window, bool HitTestVisible)> windows = [];
        void blockClosing(object? sender, WindowClosingEventArgs args) => args.Cancel = true;
        void blockInput(object? sender, KeyEventArgs args) => args.Handled = true;
        try
        {
            IEnumerable<Window> candidates = Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop
                ? desktop.Windows : new[] { owner };
            foreach (Window window in candidates.Where(window => belongsTo(window, root)).ToArray())
            {
                windows.Add((window, window.IsHitTestVisible));
                window.SetCurrentValue(InputElement.IsHitTestVisibleProperty, false);
                window.Closing += blockClosing;
                window.AddHandler(InputElement.KeyDownEvent, blockInput, RoutingStrategies.Tunnel);
            }
            await Dispatcher.UIThread.InvokeAsync(() => { }, DispatcherPriority.Background);
            return await action(progress);
        }
        finally
        {
            progress.Dispose();
            foreach ((Window window, bool hitTestVisible) in windows)
            {
                window.Closing -= blockClosing;
                window.RemoveHandler(InputElement.KeyDownEvent, blockInput);
                window.SetCurrentValue(InputElement.IsHitTestVisibleProperty, hitTestVisible);
            }
            windowOperation.Complete();
            operations?.Complete();
        }
    }

    internal static Task<T> RunInteractionAsync<T>(IProgress<EditorOperationProgress> progress, Window owner,
        Func<Window, Task<T>> interaction) => progress is EditorOperationProgressPresenter presenter
            ? presenter.RunInteractionAsync(interaction) : interaction(owner);

    private static bool belongsTo(Window window, Window root)
    {
        for (Window? current = window; current is not null; current = current.Owner as Window)
            if (ReferenceEquals(current, root))
                return true;
        return false;
    }

    private static Window getRoot(Window window)
    {
        while (window.Owner is Window parent)
            window = parent;
        return window;
    }
}
