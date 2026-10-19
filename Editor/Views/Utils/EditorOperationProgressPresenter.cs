using Avalonia.Controls;
using Avalonia.Threading;
using Ludork.Models;
using System;
using System.Diagnostics;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

internal sealed class EditorOperationProgressPresenter : IProgress<EditorOperationProgress>, IDisposable
{
    private readonly Window owner;
    private readonly string title;
    private readonly EditorOperationProgressReporter reporter = new();
    private readonly DispatcherTimer timer = new() { Interval = TimeSpan.FromMilliseconds(50) };
    private readonly Stopwatch delay = Stopwatch.StartNew();
    private EditorOperationProgressWindow? dialog;
    private EditorOperationProgress? displayed;
    private bool suspended;

    internal EditorOperationProgressPresenter(Window owner, string title, bool enabled)
    {
        this.owner = owner;
        this.title = title;
        timer.Tick += update;
        if (enabled)
            timer.Start();
    }

    public void Report(EditorOperationProgress value) => reporter.Report(value);

    internal async Task<T> RunInteractionAsync<T>(Func<Window, Task<T>> interaction)
    {
        suspended = true;
        dialog?.Finish();
        dialog = null;
        displayed = null;
        try
        {
            await Dispatcher.UIThread.InvokeAsync(static () => { }, DispatcherPriority.Background);
            return await interaction(owner);
        }
        finally
        {
            delay.Restart();
            suspended = false;
        }
    }

    private void update(object? sender, EventArgs args)
    {
        if (suspended || delay.ElapsedMilliseconds < 200)
            return;
        EditorOperationProgress latest = reporter.Current;
        if (dialog is null)
        {
            dialog = new EditorOperationProgressWindow(title);
            dialog.Update(latest);
            _ = dialog.ShowDialog(owner);
        }
        else if (!ReferenceEquals(displayed, latest))
            dialog.Update(latest);
        displayed = latest;
    }

    public void Dispose()
    {
        timer.Stop();
        timer.Tick -= update;
        dialog?.Finish();
    }
}
