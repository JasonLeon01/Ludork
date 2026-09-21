using Ludork.Models;
using Ludork.Services;
using Ludork.ViewModels;
using System;

namespace Ludork.Views.Coordination;

internal sealed class LiveDebugCoordinator(MapWorkspaceViewModel workspace, ProjectDataStore data, ProjectRunnerService runner) : IDisposable
{
    private LiveDebugSession? session;
    private string? context;
    private bool disposed;

    public bool Requested { get; private set; }
    public bool IsActive => session is not null;
    public bool IsEditable => session?.IsEditable == true;
    public string? Status => session?.Status;
    public event EventHandler<IMapEditingContext>? EditingContextChanged;
    public event EventHandler? Started;
    public event EventHandler? Ended;
    public event EventHandler? ContextChanged;
    public event EventHandler? StatusChanged;
    public event EventHandler<string>? ErrorReceived;

    public void Prepare() => Requested = !disposed && workspace.LiveDebug;
    public void SetConnection(bool available) => session?.SetConnection(available);
    public void SelectActor(string? runtimeId) => session?.SelectActor(runtimeId);

    public void SetRunState(ProjectRunState state)
    {
        if (disposed)
            return;
        if (state == ProjectRunState.Idle)
        {
            end();
            return;
        }
        if (state != ProjectRunState.Running || !Requested || session is not null)
            return;
        session = new LiveDebugSession(runner);
        workspace.SetLiveDebugSession(session);
        EditingContextChanged?.Invoke(this, session);
        session.StateChanged += onStateChanged;
        session.ErrorReceived += onError;
        Started?.Invoke(this, EventArgs.Empty);
        session.SetConnection(runner.CanSendCommand);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        end();
    }

    private void onError(object? sender, string error) => ErrorReceived?.Invoke(this, error);

    private void onStateChanged(object? sender, EventArgs args)
    {
        if (session is null)
            return;
        if (context != session.Context)
        {
            context = session.Context;
            ContextChanged?.Invoke(this, EventArgs.Empty);
        }
        workspace.RefreshLiveDebugState();
        StatusChanged?.Invoke(this, EventArgs.Empty);
    }

    private void end()
    {
        if (session is not null)
        {
            session.StateChanged -= onStateChanged;
            session.ErrorReceived -= onError;
            session.Dispose();
            session = null;
            workspace.SetLiveDebugSession(null);
            EditingContextChanged?.Invoke(this, new ProjectMapEditingContext(data));
        }
        Requested = false;
        context = null;
        Ended?.Invoke(this, EventArgs.Empty);
        StatusChanged?.Invoke(this, EventArgs.Empty);
    }
}
