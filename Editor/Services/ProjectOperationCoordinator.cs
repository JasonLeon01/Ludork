using System;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class ProjectOperationCoordinator : IDisposable
{
    private readonly ProjectRunnerService runner;
    private readonly ProjectConfigService config;
    private readonly IProjectOperationInteraction interaction;
    private CancellationTokenSource? cancellation;
    private TaskCompletionSource? completion;
    private bool disposed;

    public ProjectOperationCoordinator(ProjectRunnerService runner, ProjectConfigService config,
        IProjectOperationInteraction interaction)
    {
        this.runner = runner;
        this.config = config;
        this.interaction = interaction;
    }

    public ProjectRunState State { get; private set; }
    public bool IsPending => completion is not null;
    public Task Completion => completion?.Task ?? Task.CompletedTask;
    public event EventHandler<ProjectRunState>? StateChanged;

    public void ObserveRunnerState(ProjectRunState state)
    {
        if (!disposed && !(IsPending && state == ProjectRunState.Idle))
            setState(state);
    }

    public void Cancel() => cancellation?.Cancel();

    public async Task StopAsync()
    {
        Cancel();
        long generation = runner.RunGeneration;
        await runner.SetPerformanceMonitoringAsync(false, generation);
        await runner.StopAsync(generation);
    }

    public async Task ExecuteAsync(EditorProjectOperationKind action)
    {
        if (!canBegin())
            return;
        bool sourceProject = !config.IsStandalone;
        if (action == EditorProjectOperationKind.Construct && !sourceProject)
            return;
        if (action == EditorProjectOperationKind.Play)
        {
            runner.ExportState.RefreshAvailability();
            if (sourceProject)
                runner.NativeBuildState.RefreshAvailability();
            if (sourceProject && !runner.NativeBuildState.HasSuccessfulBuild
                || !runner.ExportState.HasSuccessfulExport)
                return;
        }
        using CancellationTokenSource operationCancellation = new();
        TaskCompletionSource operationCompletion = begin(operationCancellation);
        CancellationToken token = operationCancellation.Token;
        ProjectRunResult? result = null;
        bool building = action == EditorProjectOperationKind.Construct;
        try
        {
            if (action == EditorProjectOperationKind.Play)
                interaction.PrepareRun();
            setState(action switch
            {
                EditorProjectOperationKind.Construct => ProjectRunState.Building,
                EditorProjectOperationKind.Export => ProjectRunState.Exporting,
                _ => ProjectRunState.Preparing,
            });
            interaction.BeginOutput();
            if (action == EditorProjectOperationKind.Export)
            {
                result = await interaction.ExportAsync(token);
                return;
            }
            while (true)
            {
                token.ThrowIfCancellationRequested();
                bool needsBuild = action == EditorProjectOperationKind.Construct
                    || sourceProject && !await runner.NativeBuildState.CheckAsync(token);
                if (needsBuild && action == EditorProjectOperationKind.Play)
                {
                    interaction.RestoreViewport();
                    if (!await interaction.ConfirmRebuildAsync(token))
                    {
                        result = ProjectRunResult.CancelledResult();
                        return;
                    }
                }
                result = null;
                building = needsBuild;
                setState(building ? ProjectRunState.Building : ProjectRunState.Preparing);
                token.ThrowIfCancellationRequested();
                if (!await interaction.SaveAsync(needsBuild))
                    return;
                token.ThrowIfCancellationRequested();
                if (needsBuild)
                {
                    result = await runner.BuildAsync(token);
                    token.ThrowIfCancellationRequested();
                    if (!result.Success || action == EditorProjectOperationKind.Construct)
                        return;
                    building = false;
                    setState(ProjectRunState.Preparing);
                }
                if (!await runner.ExportState.CheckAsync(token))
                {
                    interaction.RestoreViewport();
                    if (!await interaction.ConfirmReexportAsync(token))
                    {
                        result = ProjectRunResult.CancelledResult();
                        return;
                    }
                    setState(ProjectRunState.Exporting);
                    result = await interaction.ExportAsync(token);
                    token.ThrowIfCancellationRequested();
                    if (!result.Success)
                        return;
                    setState(ProjectRunState.Preparing);
                }
                ProjectWindowMode mode = interaction.IndividualWindow
                    ? ProjectWindowMode.Individual : ProjectWindowMode.Embedded;
                nint handle = await interaction.PrepareViewportAsync(mode);
                token.ThrowIfCancellationRequested();
                if (mode == ProjectWindowMode.Embedded && handle == nint.Zero)
                {
                    result = ProjectRunResult.Failed(ProjectRunFailure.EmbeddedHandleUnavailable, string.Empty);
                    return;
                }
                result = await runner.StartAsync(new ProjectRunOptions(
                    config.IsStandalone, mode, handle, interaction.LiveDebugRequested), token);
                token.ThrowIfCancellationRequested();
                if (result.Failure is not (ProjectRunFailure.BuildRequired or ProjectRunFailure.ExportRequired))
                    return;
            }
        }
        catch (OperationCanceledException)
        {
            result = ProjectRunResult.CancelledResult();
        }
        catch (ProjectStateCheckException exception)
        {
            result = ProjectRunResult.Failed(ProjectRunFailure.LaunchFailed, exception.Message);
        }
        finally
        {
            interaction.EndOutput();
            finish(operationCompletion);
            if (result is { Success: false, Cancelled: false } && !interaction.IsClosing)
                await interaction.ShowFailureAsync(result, building);
        }
    }

    public async Task PackAsync(Func<CancellationToken, Task> pack)
    {
        if (!canBegin())
            return;
        using CancellationTokenSource operationCancellation = new();
        TaskCompletionSource operationCompletion = begin(operationCancellation);
        try
        {
            setState(ProjectRunState.Packing);
            await pack(operationCancellation.Token);
        }
        finally
        {
            finish(operationCompletion);
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        Cancel();
    }

    private bool canBegin() => !disposed && !IsPending
        && runner.State == ProjectRunState.Idle && State == ProjectRunState.Idle;

    private TaskCompletionSource begin(CancellationTokenSource source)
    {
        cancellation = source;
        completion = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        return completion;
    }

    private void finish(TaskCompletionSource operationCompletion)
    {
        cancellation = null;
        completion = null;
        setState(runner.State);
        operationCompletion.TrySetResult();
    }

    private void setState(ProjectRunState state)
    {
        State = state;
        StateChanged?.Invoke(this, state);
    }
}
