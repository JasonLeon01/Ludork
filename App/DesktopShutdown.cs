using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Threading;
using Ludork.Views;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork;

public partial class App
{
    private TaskCompletionSource? quitDecision;
    private bool committingQuit;
    internal bool IsQuitRequested => quitDecision is not null;

    private void retentionChanged()
    {
        if (rootWindows.Count == 0 && editorSettings?.KeepRunningInBackground == false)
            requestQuit();
    }

    private void onShutdownRequested(object? sender, ShutdownRequestedEventArgs args)
    {
        if (quitDecision is null && openingProjects.Count == 0 && desktopLifetime!.Windows.All(window => window.Owner is null)
            && rootWindows.OfType<MainWindow>().All(project => project.CanCloseImmediately()))
            return;
        args.Cancel = true;
        if (quitDecision is null)
            Dispatcher.UIThread.Post(requestQuit);
    }

    internal async void requestQuit()
    {
        if (quitDecision is not null || desktopLifetime is null)
            return;
        TaskCompletionSource decision = new(TaskCreationOptions.RunContinuationsAsynchronously);
        quitDecision = decision;
        Window[] windows = [];
        try
        {
            MainWindow[] projects = rootWindows.OfType<MainWindow>().ToArray();
            await Task.WhenAll(projects.Select(project => project.PendingEdit));
            windows = desktopLifetime.Windows.Where(window => window.IsEnabled).ToArray();
            foreach (Window window in windows)
                window.IsEnabled = false;
            foreach (MainWindow project in projects)
            {
                if (!await project.ConfirmQuitAsync())
                    return;
            }
            committingQuit = true;
            PendingOpen[] pending = openingProjects.Values.ToArray();
            foreach (PendingOpen open in pending)
                open.Cancellation.Cancel();
            decision.TrySetResult();
            await Task.WhenAll(pending.Select(open => open.Finished.Task));
            foreach (MainWindow project in projects)
                await project.CloseConfirmedAsync();
            desktopLifetime.ShutdownRequested -= onShutdownRequested;
            desktopLifetime.Shutdown();
        }
        finally
        {
            decision.TrySetResult();
            if (!committingQuit)
            {
                foreach (Window window in windows)
                    window.IsEnabled = true;
                quitDecision = null;
            }
        }
    }
}
