using Avalonia.Controls;
using Avalonia;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using AvaloniaEdit.Document;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Plugin.Abstractions;
using Ludork.Services;
using Ludork.Services.BlueprintAssistant;
using Ludork.Services.Plugins;
using Ludork.ViewModels;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class MainWindow
{
    private void refreshMapPanelState()
    {
        string? layerName = viewModel?.SelectedLayerTab is { IsOverview: false } layer ? layer.Name : null;
        EditorPanel.setSelectedLayer(layerName);
        EditorPanel.setSelectedLayerEditable(viewModel?.IsSelectedLayerEditable == true);
        ActorInfoPanel.setLayerEditable(viewModel?.IsSelectedLayerEditable == true);
        if (tileSelect is not null)
        {
            tileSelect.IsLayerSelected = EditorPanel.EditMode == MapEditMode.Tile && layerName is not null;
        }
        syncTileSelection();
    }

    private void onLightSelectionChanged(object? sender, LightSelectionChangedEventArgs args)
    {
        if (EditorPanel.EditMode == MapEditMode.Light)
            LightInfoPanel.setLight(args.LightData);
    }

    private void onLightDataChanged(object? sender, LightDataChangedEventArgs args)
    {
        if (EditorPanel.EditMode == MapEditMode.Light)
            LightInfoPanel.updateLight(args.LightData);
    }

    private void onLightEdited(object? sender, LightInfoEditedEventArgs args)
    {
        EditorPanel.updateSelectedLight(args.LightData);
    }

    private void onTileSelectPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (args.PropertyName is nameof(TileSelectViewModel.SelectedTiles) or nameof(TileSelectViewModel.SelectedAutoTile))
            syncTileSelection();
    }

    private void syncTileSelection()
    {
        EditorPanel.setTileSelection(tileSelect?.SelectedTiles, tileSelect?.SelectedAutoTile?.Key);
    }

    private void onTileSelectionPicked(object? sender, TileSelectionChangedEventArgs args)
    {
        if (tileSelect is null)
            return;
        if (!string.IsNullOrWhiteSpace(args.AutoTileKey))
        {
            tileSelect.SelectedAutoTile = tileSelect.AutoTiles.FirstOrDefault(item => item.Key == args.AutoTileKey);
            return;
        }
        if (args.Tiles is { } tiles)
        {
            tileSelect.selectTiles(tiles.OriginTileNumber, tiles.Width, tiles.Height);
            return;
        }
        tileSelect.ClearSelection();
    }

    private void onTileModeClick(object? sender, RoutedEventArgs args) => selectPreviewMode(MapEditMode.Tile);

    private void onLightModeClick(object? sender, RoutedEventArgs args) => selectPreviewMode(MapEditMode.Light);

    private void onActorModeClick(object? sender, RoutedEventArgs args) => selectPreviewMode(MapEditMode.Actor);

    private async void onPlayClick(object? sender, RoutedEventArgs args)
    {
        if (projectOperationState is ProjectRunState.Building or ProjectRunState.Exporting or ProjectRunState.Packing)
            return;
        if (projectLaunchPending || projectRunner?.State is ProjectRunState.Preparing or ProjectRunState.Running)
        {
            projectLaunchCancellation?.Cancel();
            GamePanel.SetInputEnabled(false);
            if (projectRunner is not null)
            {
                long generation = projectRunner.RunGeneration;
                await projectRunner.SetPerformanceMonitoringAsync(false, generation);
                await projectRunner.StopAsync(generation);
            }
            return;
        }
        await executeProjectOperationAsync(ToolbarAction.Play);
    }

    private async void onConstructClick(object? sender, RoutedEventArgs args)
    {
        if (viewModel?.ProjectConfig.IsStandalone != false)
            return;
        await executeProjectOperationAsync(ToolbarAction.Construct);
    }

    private async void onExportClick(object? sender, RoutedEventArgs args)
    {
        await executeProjectOperationAsync(ToolbarAction.Export);
    }

    private enum ToolbarAction
    {
        Construct,
        Export,
        Play,
    }

    private async Task executeProjectOperationAsync(ToolbarAction action)
    {
        if (projectLaunchPending || projectRunner is null || viewModel is null
            || projectRunner.State != ProjectRunState.Idle || projectOperationState != ProjectRunState.Idle)
            return;
        bool sourceProject = !viewModel.ProjectConfig.IsStandalone;
        if (action == ToolbarAction.Play)
        {
            projectRunner.ExportState.RefreshAvailability();
            if (sourceProject)
                projectRunner.NativeBuildState.RefreshAvailability();
            if (sourceProject && !projectRunner.NativeBuildState.HasSuccessfulBuild
                || !projectRunner.ExportState.HasSuccessfulExport)
                return;
        }
        using CancellationTokenSource cancellation = new();
        projectLaunchCancellation = cancellation;
        projectLaunchPending = true;
        TaskCompletionSource completion = new(TaskCreationOptions.RunContinuationsAsynchronously);
        projectOperationCompletion = completion;
        ProjectRunState initialState = action switch
        {
            ToolbarAction.Construct => ProjectRunState.Building,
            ToolbarAction.Export => ProjectRunState.Exporting,
            _ => ProjectRunState.Preparing,
        };
        setProjectRunState(initialState);
        ProjectRunResult? result = null;
        bool building = action == ToolbarAction.Construct;
        resetConsoleOutput();
        performanceMonitorWindow?.ClearData();
        BottomTabs.SelectedIndex = 1;
        string? logError = consoleLogSession.Start(ProjectPath);
        if (logError is not null)
            appendConsoleLine("[Console] Failed to create the log file: " + logError);
        try
        {
            if (action == ToolbarAction.Export)
            {
                result = await exportProjectAsync(appendConsoleLine, cancellation.Token);
                return;
            }
            while (true)
            {
                cancellation.Token.ThrowIfCancellationRequested();
                bool needsBuild = action == ToolbarAction.Construct
                    || sourceProject && !await projectRunner.NativeBuildState.CheckAsync(cancellation.Token);
                if (needsBuild && action == ToolbarAction.Play)
                {
                    restoreEditorViewport();
                    if (!await ConfirmationDialog.ShowAsync(this,
                        LocaleService.Get("RUN_REBUILD_TITLE"), LocaleService.Get("RUN_REBUILD_CONFIRM"), cancellation.Token))
                    {
                        result = ProjectRunResult.CancelledResult();
                        return;
                    }
                }
                result = null;
                building = needsBuild;
                setProjectRunState(building ? ProjectRunState.Building : ProjectRunState.Preparing);
                cancellation.Token.ThrowIfCancellationRequested();
                if (!await EditorSaveWorkflow.TrySaveAsync(this, viewModel.ProjectSave, false, needsBuild))
                    return;
                cancellation.Token.ThrowIfCancellationRequested();
                if (needsBuild)
                {
                    result = await projectRunner.BuildAsync(cancellation.Token);
                    cancellation.Token.ThrowIfCancellationRequested();
                    if (!result.Success || action == ToolbarAction.Construct)
                        return;
                    building = false;
                    setProjectRunState(ProjectRunState.Preparing);
                }
                if (!await projectRunner.ExportState.CheckAsync(cancellation.Token))
                {
                    restoreEditorViewport();
                    if (!await ConfirmationDialog.ShowAsync(this,
                        LocaleService.Get("RUN_REEXPORT_TITLE"), LocaleService.Get("RUN_REEXPORT_CONFIRM"), cancellation.Token))
                    {
                        result = ProjectRunResult.CancelledResult();
                        return;
                    }
                    setProjectRunState(ProjectRunState.Exporting);
                    result = await exportProjectAsync(appendConsoleLine, cancellation.Token);
                    cancellation.Token.ThrowIfCancellationRequested();
                    if (!result.Success)
                        return;
                    setProjectRunState(ProjectRunState.Preparing);
                }
                ProjectWindowMode windowMode = viewModel.IndividualWindow
                    ? ProjectWindowMode.Individual : ProjectWindowMode.Embedded;
                nint windowHandle = await prepareGameViewportAsync(windowMode);
                cancellation.Token.ThrowIfCancellationRequested();
                if (windowMode == ProjectWindowMode.Embedded && windowHandle == nint.Zero)
                {
                    result = ProjectRunResult.Failed(ProjectRunFailure.EmbeddedHandleUnavailable, string.Empty);
                    return;
                }
                result = await projectRunner.StartAsync(new ProjectRunOptions(
                    viewModel.ProjectConfig.IsStandalone, windowMode, windowHandle), cancellation.Token);
                cancellation.Token.ThrowIfCancellationRequested();
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
            projectLaunchPending = false;
            projectLaunchCancellation = null;
            logError = consoleLogSession.Stop();
            if (logError is not null)
                appendConsoleLine("[Console] Failed to close the log file: " + logError);
            setProjectRunState(projectRunner.State);
            projectOperationCompletion = null;
            completion.TrySetResult();
            if (result is { Success: false, Cancelled: false } && !closingPrompt && !closeConfirmed)
            {
                string title = result.Failure == ProjectRunFailure.ExportFailed ? "EXPORT_FAILED_TITLE"
                    : building ? "BUILD_FAILED_TITLE" : "RUN_FAILED_TITLE";
                await AlertDialog.ShowAsync(this, LocaleService.Get(title), getProjectRunFailureMessage(result));
            }
        }
    }

    private async Task<ProjectRunResult> exportProjectAsync(Action<string> writeOutput, CancellationToken cancellationToken)
    {
        ProjectExportResult result = await EditorExportWorkflow.ExportAsync(
            this, viewModel!.ProjectSave, projectRunner!.ExportState, writeOutput, cancellationToken);
        return result.Success ? ProjectRunResult.Completed()
            : result.Cancelled ? ProjectRunResult.CancelledResult()
            : ProjectRunResult.Failed(ProjectRunFailure.ExportFailed, result.Detail);
    }

    private void onExportStateChanged(object? sender, EventArgs args)
    {
        Dispatcher.UIThread.Post(updateRunButtons);
    }

    private void onNativeBuildStateChanged(object? sender, EventArgs args)
    {
        Dispatcher.UIThread.Post(updateRunButtons);
    }

    private void onMainWindowActivated(object? sender, EventArgs args)
    {
        projectRunner?.ExportState.RefreshAvailability();
        if (viewModel?.ProjectConfig.IsStandalone == false && projectRunner is not null)
            projectRunner.NativeBuildState.RefreshAvailability();
    }

    private void onProjectOutputReceived(object? sender, string line)
    {
        appendConsoleLine(line);
    }

    private void onPerformanceSampleReceived(object? sender, PerformanceSample sample)
    {
        performanceMonitorWindow?.AddSample(sample);
    }

    private void onProjectRunStateChanged(object? sender, ProjectRunState state)
    {
        if (sender is not ProjectRunnerService runner)
            return;
        long generation = runner.RunGeneration;
        Dispatcher.UIThread.Post(() =>
        {
            if (!ReferenceEquals(projectRunner, runner) || generation != runner.RunGeneration)
                return;
            if (state == ProjectRunState.Running)
                projectRunReachedRunning = true;
            if (state == runner.State && !(projectLaunchPending && state == ProjectRunState.Idle))
                setProjectRunState(state);
        });
    }

    private void onCommandAvailabilityChanged(object? sender, bool available)
    {
        if (sender is not ProjectRunnerService runner)
            return;
        long generation = runner.RunGeneration;
        long connection = runner.ConnectionGeneration;
        Dispatcher.UIThread.Post(() =>
        {
            if (!ReferenceEquals(projectRunner, runner)
                || runner.RunGeneration != generation
                || runner.ConnectionGeneration != connection)
                return;
            GamePanel.ResetTextInput();
            updateConsoleInputState();
            GamePanel.SetInputEnabled(
                available
                && projectRunner?.State == ProjectRunState.Running
                && activeWindowMode == ProjectWindowMode.Embedded);
            if (!available || projectRunner is null)
                return;
            bool enabled = performanceMonitorWindow is not null;
            if (enabled)
                performanceMonitorWindow?.ClearData();
            _ = projectRunner.SetPerformanceMonitoringAsync(enabled, projectRunner.RunGeneration);
        });
    }

    private void onOpenPerformanceMonitor(object? sender, EventArgs args)
    {
        if (performanceMonitorWindow is not null)
        {
            performanceMonitorWindow.Show();
            performanceMonitorWindow.Activate();
            return;
        }
        PerformanceMonitorWindow window = new();
        performanceMonitorWindow = window;
        window.Closed += onPerformanceMonitorClosed;
        window.Show(this);
        if (projectRunner?.CanSendCommand == true)
            _ = projectRunner.SetPerformanceMonitoringAsync(true, projectRunner.RunGeneration);
    }

    private void onPerformanceMonitorClosed(object? sender, EventArgs args)
    {
        if (sender is PerformanceMonitorWindow window)
            window.Closed -= onPerformanceMonitorClosed;
        performanceMonitorWindow = null;
        if (projectRunner?.CanSendCommand == true)
            _ = projectRunner.SetPerformanceMonitoringAsync(false, projectRunner.RunGeneration);
    }

    private void onRuntimeTextInputReceived(object? sender, RuntimeTextInputMessage message)
    {
        Dispatcher.UIThread.Post(() =>
        {
            if (sender is ProjectRunnerService runner
                && ReferenceEquals(projectRunner, runner)
                && runner.State == ProjectRunState.Running
                && activeWindowMode == ProjectWindowMode.Embedded
                && runner.IsCurrentConnection(message.RunGeneration, message.ConnectionGeneration))
            {
                GamePanel.ApplyTextInput(message);
            }
        });
    }

    private void onGameInputBatchReady(object? sender, GameInputBatchEventArgs args)
    {
        if (projectRunner?.State != ProjectRunState.Running
            || !projectRunner.CanSendCommand
            || activeWindowMode != ProjectWindowMode.Embedded)
        {
            return;
        }
        gameInputSendTail = sendGameInputBatchAsync(
            gameInputSendTail, args.Events, projectRunner,
            projectRunner.RunGeneration, projectRunner.ConnectionGeneration);
    }

    private async Task sendGameInputBatchAsync(
        Task previousBatch,
        IReadOnlyList<RuntimeInputEvent> events,
        ProjectRunnerService runner,
        long generation,
        long connection)
    {
        await previousBatch;
        if (!ReferenceEquals(projectRunner, runner)
            || runner.State != ProjectRunState.Running
            || !runner.IsCurrentConnection(generation, connection)
            || activeWindowMode != ProjectWindowMode.Embedded)
        {
            return;
        }
        await runner.SendInputBatchAsync(events, generation, connection);
    }

    private async void onConsoleSendClick(object? sender, RoutedEventArgs args)
    {
        await sendConsoleCommandAsync();
    }

    private async void onConsoleInputKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.Enter)
        {
            args.Handled = true;
            await sendConsoleCommandAsync();
        }
        else if (args.Key == Key.Up)
        {
            args.Handled = navigateConsoleHistory(-1);
        }
        else if (args.Key == Key.Down)
        {
            args.Handled = navigateConsoleHistory(1);
        }
    }

    private void onConsoleInputTextChanged()
    {
        if (!settingConsoleHistoryText && consoleHistoryIndex != consoleHistory.Count)
        {
            consoleHistoryIndex = consoleHistory.Count;
            consoleDraft = ConsoleInput.Text ?? string.Empty;
        }
        updateConsoleInputState();
    }

    private async Task sendConsoleCommandAsync()
    {
        if (projectRunner is null || consoleSendPending)
            return;
        string command = ConsoleInput.Text?.Trim() ?? string.Empty;
        if (command.Length == 0)
            return;

        consoleSendPending = true;
        updateConsoleInputState();
        bool sent = await projectRunner.SendCommandAsync(command);
        consoleSendPending = false;
        if (!sent)
        {
            appendConsoleLine(LocaleService.Get("CONSOLE_SEND_FAILED"));
            updateConsoleInputState();
            return;
        }

        appendConsoleLine(">>> " + command);
        consoleHistory.Add(command);
        consoleHistoryIndex = consoleHistory.Count;
        consoleDraft = string.Empty;
        setConsoleInputText(string.Empty);
    }

    private bool navigateConsoleHistory(int direction)
    {
        if (consoleHistory.Count == 0)
            return false;
        if (direction < 0)
        {
            if (consoleHistoryIndex == consoleHistory.Count)
                consoleDraft = ConsoleInput.Text ?? string.Empty;
            if (consoleHistoryIndex > 0)
                consoleHistoryIndex--;
        }
        else
        {
            if (consoleHistoryIndex >= consoleHistory.Count)
                return false;
            consoleHistoryIndex++;
        }

        string text = consoleHistoryIndex < consoleHistory.Count
            ? consoleHistory[consoleHistoryIndex]
            : consoleDraft;
        setConsoleInputText(text);
        return true;
    }

    private void setConsoleInputText(string text)
    {
        settingConsoleHistoryText = true;
        ConsoleInput.Text = text;
        ConsoleInput.CaretIndex = text.Length;
        settingConsoleHistoryText = false;
        updateConsoleInputState();
    }

    private void updateConsoleInputState()
    {
        bool available = projectRunner?.State == ProjectRunState.Running
            && projectRunner.CanSendCommand;
        ConsoleInput.IsEnabled = available;
        ConsoleSendButton.IsEnabled = available
            && !consoleSendPending
            && !string.IsNullOrWhiteSpace(ConsoleInput.Text);
    }

    private void appendConsoleLine(string line)
    {
        lock (consoleOutputSync)
        {
            consoleLogView.AppendLine(line);
            string? logError = consoleLogSession.WriteLine(line);
            if (logError is not null)
                consoleLogView.AppendLine("[Console] Failed to write the log file: " + logError);
        }
    }

    private void resetConsoleOutput()
    {
        lock (consoleOutputSync)
            consoleLogView.Clear();
    }

    private void updateRunButtons()
    {
        bool busy = projectOperationState is ProjectRunState.Building or ProjectRunState.Exporting or ProjectRunState.Packing;
        bool playing = projectOperationState is ProjectRunState.Preparing or ProjectRunState.Running;
        bool sourceProject = viewModel?.ProjectConfig.IsStandalone == false;
        bool hasBuild = !sourceProject || projectRunner?.NativeBuildState.HasSuccessfulBuild == true;
        ConstructButton.IsVisible = sourceProject;
        ConstructButton.IsEnabled = viewModel is not null && projectOperationState == ProjectRunState.Idle;
        bool hasExport = projectRunner?.ExportState.HasSuccessfulExport == true;
        ExportButton.IsEnabled = viewModel is not null && projectOperationState == ProjectRunState.Idle;
        PlayButton.IsEnabled = viewModel is not null && !busy && (playing || hasBuild && hasExport);
        PlayIcon.Source = EditorIconResources.GetImage(playing ? "EditorImage.Stop" : "EditorImage.Play");
        string playTip = LocaleService.Get(playing ? "RUN_STOP" : !hasBuild ? "RUN_BUILD_REQUIRED" : !hasExport ? "RUN_EXPORT_REQUIRED" : "RUN_PLAY");
        ToolTip.SetTip(PlayButtonHint, playTip);
        ToolTip.SetTip(ConstructButton, LocaleService.Get("CONSTRUCT"));
        ToolTip.SetTip(ExportButton, LocaleService.Get("EXPORT"));
        Avalonia.Automation.AutomationProperties.SetName(ExportButton, LocaleService.Get("EXPORT"));
        Avalonia.Automation.AutomationProperties.SetName(PlayButton, LocaleService.Get(playing ? "RUN_STOP" : "RUN_PLAY"));
        Avalonia.Automation.AutomationProperties.SetName(ConstructButton, LocaleService.Get("CONSTRUCT"));
    }

    private void setProjectRunState(ProjectRunState state)
    {
        projectOperationState = state;
        bool active = state != ProjectRunState.Idle;
        if (state == ProjectRunState.Running)
            projectRunReachedRunning = true;
        bool returnedFromRun = !active && projectRunReachedRunning;
        if (returnedFromRun)
            projectRunReachedRunning = false;
        if (viewModel is not null)
            viewModel.CanEdit = !active;
        updateRunButtons();
        EditModeToggles.IsEnabled = !active;
        EditorPanel.IsEnabled = !active;
        WorldEditorPanel.IsEnabled = !active;
        MapList.IsEnabled = !active;
        LayerTabs.IsEnabled = !active && EditorPanel.EditMode != MapEditMode.Light;
        RightList.IsEnabled = !active;
        LightInfoPanel.IsEnabled = !active;
        ActorInfoPanel.IsEnabled = !active;
        RightModePanel.IsEnabled = !active;
        FileExplorerPanel.IsEnabled = !active;
        ActorOutliner.IsEnabled = !active;
        setDocumentWindowsEnabled(!active);
        if (!active)
        {
            if (returnedFromRun && viewModel is not null)
                viewModel.GameConfig.Reload();
            restoreEditorViewport();
        }
        else
            GamePanel.SetInputEnabled(
                projectRunner?.CanSendCommand == true
                && state == ProjectRunState.Running
                && activeWindowMode == ProjectWindowMode.Embedded);
        updateConsoleInputState();
    }

    private void setDocumentWindowsEnabled(bool enabled)
    {
        foreach (Window window in OwnedWindows)
        {
            if (!enabled && window is ParticleWindow particle)
                particle.PausePreview();
            if (!enabled && window is ParticleOverviewWindow particles)
                particles.PausePreview();
            if (window is AnimationOverviewWindow or AnimationWindow or ParticleOverviewWindow or ParticleWindow or TilesetEditorWindow
                or GeneralDataEditorWindow or CommonFunctionWindow or GameVariableManagerWindow
                or CurveWindow or TextConfigEditorWindow or BlueprintEditorWindow or UiAssetEditorWindow)
                window.IsEnabled = enabled;
        }
    }

    private async Task<nint> prepareGameViewportAsync(ProjectWindowMode windowMode)
    {
        activeWindowMode = windowMode;
        if (windowMode == ProjectWindowMode.Individual)
            return nint.Zero;
        EditorPanel.IsVisible = false;
        GameViewport.IsVisible = true;
        GameViewport.InvalidateMeasure();
        await Dispatcher.UIThread.InvokeAsync(
            () => GamePanel.TryUpdateNativeControlPosition(),
            DispatcherPriority.Render);
        lockGameLayout();
        if (GamePanel.NativeHandle == nint.Zero)
        {
            TaskCompletionSource handleReady = new();
            void onHandleReady(object? sender, EventArgs args) => handleReady.TrySetResult();
            GamePanel.NativeHandleReady += onHandleReady;
            GameViewport.InvalidateMeasure();
            Task completed = await Task.WhenAny(handleReady.Task, Task.Delay(TimeSpan.FromSeconds(1)));
            GamePanel.NativeHandleReady -= onHandleReady;
            if (completed != handleReady.Task)
                return nint.Zero;
        }
        nint handle = GamePanel.NativeHandle;
        await Dispatcher.UIThread.InvokeAsync(
            () => GamePanel.TryUpdateNativeControlPosition(),
            DispatcherPriority.Render);
        return handle != nint.Zero && handle == GamePanel.NativeHandle
            ? handle
            : nint.Zero;
    }

    private void lockGameLayout()
    {
        if (gameLayoutLocked)
            return;
        double width = CenterArea.Bounds.Width;
        double height = UpperGrid.Bounds.Height;
        if (width <= 0 || height <= 0)
            return;
        previousCenterWidth = centerColumn.Width;
        previousCenterMinWidth = centerColumn.MinWidth;
        previousCenterMaxWidth = centerColumn.MaxWidth;
        previousUpperHeight = upperRow.Height;
        previousUpperMinHeight = upperRow.MinHeight;
        previousUpperMaxHeight = upperRow.MaxHeight;
        centerColumn.Width = new GridLength(width);
        centerColumn.MinWidth = width;
        centerColumn.MaxWidth = width;
        upperRow.Height = new GridLength(height);
        upperRow.MinHeight = height;
        upperRow.MaxHeight = height;
        UpperLeftSplitter.IsEnabled = false;
        UpperRightSplitter.IsEnabled = false;
        UpperLowerSplitter.IsEnabled = false;
        gameLayoutLocked = true;
    }

    private void restoreEditorViewport()
    {
        GamePanel.SetInputEnabled(false);
        GameViewport.IsVisible = false;
        EditorPanel.IsVisible = true;
        activeWindowMode = null;
        if (gameLayoutLocked)
        {
            centerColumn.Width = previousCenterWidth;
            centerColumn.MinWidth = previousCenterMinWidth;
            centerColumn.MaxWidth = previousCenterMaxWidth;
            upperRow.Height = previousUpperHeight;
            upperRow.MinHeight = previousUpperMinHeight;
            upperRow.MaxHeight = previousUpperMaxHeight;
            UpperLeftSplitter.IsEnabled = true;
            UpperRightSplitter.IsEnabled = true;
            UpperLowerSplitter.IsEnabled = true;
            gameLayoutLocked = false;
        }
        Dispatcher.UIThread.Post(() =>
        {
            if (!IsActive
                || projectRunner?.State != ProjectRunState.Idle
                || activeWindowMode is not null
                || !EditorPanel.IsVisible
                || !EditorPanel.IsEnabled)
            {
                return;
            }
            EditorPanel.Focus();
        }, DispatcherPriority.Input);
    }

    private static string getProjectRunFailureMessage(ProjectRunResult result)
    {
        string key = result.Failure switch
        {
            ProjectRunFailure.ProjectInvalid => "RUN_PROJECT_INVALID",
            ProjectRunFailure.PluginPreparationFailed => "RUN_PLUGIN_PREPARATION_FAILED",
            ProjectRunFailure.BuildToolMissing => "RUN_BUILD_TOOL_MISSING",
            ProjectRunFailure.BuildFailed => "RUN_BUILD_FAILED",
            ProjectRunFailure.BuildRequired => "RUN_BUILD_REQUIRED",
            ProjectRunFailure.ExportRequired => "RUN_EXPORT_REQUIRED",
            ProjectRunFailure.ExportFailed => "EXPORT_FAILED_TITLE",
            ProjectRunFailure.ExecutableMissing => "RUN_EXECUTABLE_MISSING",
            ProjectRunFailure.EmbeddedHandleUnavailable => "RUN_EMBED_HANDLE_UNAVAILABLE",
            ProjectRunFailure.ProtocolMismatch => "RUN_PROTOCOL_MISMATCH",
            ProjectRunFailure.GameFailed => "RUN_GAME_FAILED",
            _ => "RUN_LAUNCH_FAILED",
        };
        string message = LocaleService.Get(key);
        return string.IsNullOrWhiteSpace(result.Detail)
            ? message
            : message + Environment.NewLine + result.Detail;
    }

}
