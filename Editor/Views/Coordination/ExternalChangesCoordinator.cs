using Avalonia;
using Avalonia.Controls;
using Avalonia.Threading;
using Ludork.Composition;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views.Coordination;

internal sealed class ExternalChangesCoordinator : IDisposable
{
    private readonly Window owner;
    private readonly EditorProjectSession session;
    private readonly CancellationTokenSource cancellation = new();
    private readonly IDisposable activationSubscription;
    private readonly Func<Task<bool>> saveCheck;
    private Window activeWindow;
    private bool requested;
    private bool queued;
    private bool running;
    private bool disposed;

    public ExternalChangesCoordinator(Window owner, EditorProjectSession session)
    {
        this.owner = owner;
        this.session = session;
        activeWindow = owner;
        saveCheck = () => checkAsync(true);
        session.ProjectSave.SessionExternalChangeCheck = session.ProjectSave.ExternalChangeCheck = saveCheck;
        session.EditOperations.Changed += onEditOperationChanged;
        activationSubscription = Window.IsActiveProperty.Changed.AddClassHandler<Window>((window, _) =>
        {
            if (!window.IsActive || !belongsToProject(window) || disposed || running || session.ExternalChanges.IsSynchronizing)
                return;
            activeWindow = window;
            requested = true;
            queueCheck();
        });
    }

    private bool belongsToProject(Window window)
    {
        for (Window? current = window; current is not null; current = current.Owner as Window)
            if (ReferenceEquals(current, owner))
                return true;
        return false;
    }

    private void onEditOperationChanged(object? sender, EventArgs args)
    {
        if (requested && !session.EditOperations.IsBusy)
            queueCheck();
    }

    private void queueCheck()
    {
        if (queued || running || disposed || session.EditOperations.IsBusy)
            return;
        queued = true;
        Dispatcher.UIThread.Post(async () =>
        {
            queued = false;
            if (requested && !disposed && activeWindow.IsActive && !session.EditOperations.IsBusy
                && owner is not MainWindow { IsClosingProject: true } && Application.Current is not App { IsQuitRequested: true })
                await checkAsync(false);
        }, DispatcherPriority.Background);
    }

    private async Task<bool> checkAsync(bool showProgress)
    {
        if (disposed || running || session.EditOperations.IsBusy)
            return false;
        requested = false;
        running = true;
        Window dialogOwner = activeWindow.IsVisible ? activeWindow : owner;
        try
        {
            return await EditorEditWorkflow.RunAsync(dialogOwner, session.GameData,
                LocaleService.Get("EXTERNAL_CHANGES"), async progress =>
                {
                    return await session.ExternalChanges.SynchronizeAsync(
                        changes => EditorEditWorkflow.RunInteractionAsync(progress, dialogOwner,
                            window => ExternalChangesDialog.ShowAsync(window, changes, session.ProjectPath)),
                        cancellation.Token, progress);
                }, false, showProgress);
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested)
        {
            return false;
        }
        catch (InvalidDataException exception)
        {
            if (!disposed)
                await AlertDialog.ShowAsync(dialogOwner, LocaleService.Get("ERROR"), exception.Message);
            return false;
        }
        finally
        {
            await Dispatcher.UIThread.InvokeAsync(() => { }, DispatcherPriority.Background);
            running = false;
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        activationSubscription.Dispose();
        session.EditOperations.Changed -= onEditOperationChanged;
        if (session.ProjectSave.ExternalChangeCheck == saveCheck)
            session.ProjectSave.ExternalChangeCheck = null;
        if (session.ProjectSave.SessionExternalChangeCheck == saveCheck)
            session.ProjectSave.SessionExternalChangeCheck = null;
        cancellation.Cancel();
        cancellation.Dispose();
    }
}
