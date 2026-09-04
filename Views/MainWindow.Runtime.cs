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

    private async void onEditRunModeClick(object? sender, RoutedEventArgs args)
    {
        if (projectLaunchPending)
        {
            projectLaunchCancelled = true;
            setProjectRunState(ProjectRunState.Idle);
            return;
        }
        if (projectRunner is not null && projectRunner.State != ProjectRunState.Idle)
        {
            GamePanel.SetInputEnabled(false);
            long generation = projectRunner.RunGeneration;
            await projectRunner.SetPerformanceMonitoringAsync(false, generation);
            await projectRunner.StopAsync(generation);
            return;
        }
        setProjectRunState(ProjectRunState.Idle);
    }

    private async void onPlayRunModeClick(object? sender, RoutedEventArgs args)
    {
        if (projectLaunchPending
            || projectRunner is null
            || viewModel is null
            || projectRunner.State != ProjectRunState.Idle)
        {
            setProjectRunState(projectRunner?.State ?? ProjectRunState.Idle);
            return;
        }
        if (!await EditorSaveWorkflow.TrySaveAsync(
                this,
                viewModel.ProjectSave,
                false))
        {
            return;
        }

        resetConsoleOutput();
        performanceMonitorWindow?.ClearData();
        BottomTabs.SelectedIndex = 1;
        ProjectWindowMode windowMode = viewModel.IndividualWindow
            ? ProjectWindowMode.Individual
            : ProjectWindowMode.Embedded;
        projectLaunchPending = true;
        projectLaunchCancelled = false;
        setProjectRunState(ProjectRunState.Building);
        nint windowHandle = await prepareGameViewportAsync(windowMode);
        if (projectLaunchCancelled)
        {
            projectLaunchPending = false;
            setProjectRunState(ProjectRunState.Idle);
            return;
        }
        if (windowMode == ProjectWindowMode.Embedded && windowHandle == nint.Zero)
        {
            projectLaunchPending = false;
            setProjectRunState(ProjectRunState.Idle);
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("RUN_FAILED_TITLE"),
                LocaleService.Get("RUN_EMBED_HANDLE_UNAVAILABLE")
            );
            return;
        }
        ProjectRunOptions options = new(
            viewModel.ProjectConfig.IsStandalone,
            windowMode,
            windowHandle);
        string? logError = consoleLogSession.Start(ProjectPath);
        if (logError is not null)
            appendConsoleLine("[Console] Failed to create the log file: " + logError);
        Task<ProjectRunResult> runTask = projectRunner.StartAsync(options);
        projectLaunchPending = false;
        ProjectRunResult result;
        try
        {
            result = await runTask;
        }
        finally
        {
            string? logStopError = consoleLogSession.Stop();
            if (logStopError is not null)
                appendConsoleLine("[Console] Failed to close the log file: " + logStopError);
        }
        setProjectRunState(projectRunner.State);
        if (result.Success || result.Cancelled)
            return;
        await AlertDialog.ShowAsync(
            this,
            LocaleService.Get("RUN_FAILED_TITLE"),
            getProjectRunFailureMessage(result)
        );
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
        Dispatcher.UIThread.Post(() =>
        {
            setProjectRunState(state);
        });
    }

    private void onCommandAvailabilityChanged(object? sender, bool available)
    {
        Dispatcher.UIThread.Post(() =>
        {
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

    private void onGameInputBatchReady(object? sender, GameInputBatchEventArgs args)
    {
        if (projectRunner?.State != ProjectRunState.Running
            || !projectRunner.CanSendCommand
            || activeWindowMode != ProjectWindowMode.Embedded)
        {
            return;
        }
        gameInputSendTail = sendGameInputBatchAsync(gameInputSendTail, args.Events);
    }

    private async Task sendGameInputBatchAsync(
        Task previousBatch,
        IReadOnlyList<RuntimeInputEvent> events)
    {
        await previousBatch;
        if (projectRunner?.State != ProjectRunState.Running
            || !projectRunner.CanSendCommand
            || activeWindowMode != ProjectWindowMode.Embedded)
        {
            return;
        }
        await projectRunner.SendInputBatchAsync(events);
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
        bool scheduleFlush = false;
        lock (consoleOutputSync)
        {
            enqueuePendingConsoleText(line);
            string? logError = consoleLogSession.WriteLine(line);
            if (logError is not null)
                enqueuePendingConsoleText("[Console] Failed to write the log file: " + logError);
            if (!consoleFlushScheduled)
            {
                consoleFlushScheduled = true;
                scheduleFlush = true;
            }
        }
        if (scheduleFlush)
        {
            Dispatcher.UIThread.Post(
                flushConsoleLines,
                DispatcherPriority.Background);
        }
    }

    private void enqueuePendingConsoleText(string text)
    {
        string normalized = text.Replace("\r\n", "\n", StringComparison.Ordinal).Replace('\r', '\n');
        string[] lines = normalized.Split('\n');
        foreach (string line in lines)
            pendingConsoleLines.Enqueue(line);
    }

    private void flushConsoleLines()
    {
        List<string> batch = new();
        lock (consoleOutputSync)
        {
            while (pendingConsoleLines.Count > 0)
                batch.Add(pendingConsoleLines.Dequeue());
            consoleFlushScheduled = false;
        }
        if (batch.Count == 0)
            return;

        ScrollViewer? scrollViewer = findConsoleScrollViewer();
        Vector previousOffset = scrollViewer?.Offset ?? default;
        bool followBottom = consoleFollowsLatest;
        TextDocument document = ConsoleOutput.Document;
        StringBuilder appendedText = new();
        if (visibleConsoleLines.Count > 0)
            appendedText.Append(Environment.NewLine);
        appendedText.AppendJoin(Environment.NewLine, batch);
        foreach (string line in batch)
            visibleConsoleLines.Enqueue(line);

        int removeCount = Math.Max(0, visibleConsoleLines.Count - MaximumConsoleLineCount);
        int removedCharacterCount = 0;
        for (int index = 0; index < removeCount; index++)
        {
            string removed = visibleConsoleLines.Dequeue();
            removedCharacterCount += removed.Length + Environment.NewLine.Length;
        }

        consoleViewportUpdatePending = true;
        using (document.RunUpdate())
        {
            document.Insert(document.TextLength, appendedText.ToString());
            if (removedCharacterCount > 0)
                document.Remove(0, removedCharacterCount);
        }

        int generation = ++consoleViewportGeneration;
        Dispatcher.UIThread.Post(
            () => restoreConsoleViewport(
                generation,
                previousOffset,
                followBottom),
            DispatcherPriority.Background);
    }

    private ScrollViewer? findConsoleScrollViewer()
    {
        if (consoleScrollViewer is not null)
            return consoleScrollViewer;
        consoleScrollViewer = ConsoleOutput.GetVisualDescendants()
            .OfType<ScrollViewer>()
            .FirstOrDefault(viewer => viewer.Name == "PART_ScrollViewer")
            ?? ConsoleOutput.GetVisualDescendants()
                .OfType<ScrollViewer>()
                .FirstOrDefault();
        if (consoleScrollViewer is not null)
            consoleScrollViewer.ScrollChanged += onConsoleScrollChanged;
        return consoleScrollViewer;
    }

    private void onConsoleScrollChanged(object? sender, ScrollChangedEventArgs args)
    {
        if (consoleViewportUpdatePending || sender is not ScrollViewer scrollViewer)
            return;
        double maximumY = Math.Max(0, scrollViewer.Extent.Height - scrollViewer.Viewport.Height);
        if (scrollViewer.Offset.Y >= maximumY - 2)
            consoleFollowsLatest = true;
        else if (args.OffsetDelta.Y < 0)
            consoleFollowsLatest = false;
    }

    private void restoreConsoleViewport(int generation, Vector previousOffset, bool followBottom)
    {
        if (generation != consoleViewportGeneration)
            return;
        ScrollViewer? scrollViewer = findConsoleScrollViewer();
        if (scrollViewer is null)
        {
            consoleViewportUpdatePending = false;
            return;
        }
        double maximumX = Math.Max(0, scrollViewer.Extent.Width - scrollViewer.Viewport.Width);
        double maximumY = Math.Max(0, scrollViewer.Extent.Height - scrollViewer.Viewport.Height);
        double targetX = Math.Clamp(previousOffset.X, 0, maximumX);
        double targetY = followBottom
            ? maximumY
            : Math.Clamp(previousOffset.Y, 0, maximumY);
        scrollViewer.Offset = new Vector(targetX, targetY);
        consoleViewportUpdatePending = false;
    }

    private void resetConsoleOutput()
    {
        lock (consoleOutputSync)
        {
            pendingConsoleLines.Clear();
            visibleConsoleLines.Clear();
        }
        consoleViewportGeneration++;
        consoleViewportUpdatePending = true;
        ConsoleOutput.Clear();
        ScrollViewer? scrollViewer = findConsoleScrollViewer();
        if (scrollViewer is not null)
            scrollViewer.Offset = default;
        consoleViewportUpdatePending = false;
        consoleFollowsLatest = true;
    }

    private void setProjectRunState(ProjectRunState state)
    {
        bool active = state != ProjectRunState.Idle;
        if (state == ProjectRunState.Running)
            projectRunReachedRunning = true;
        bool returnedFromRun = !active && projectRunReachedRunning;
        if (returnedFromRun)
            projectRunReachedRunning = false;
        EditRunModeToggle.IsChecked = !active;
        PlayRunModeToggle.IsChecked = active;
        EditRunModeToggle.IsEnabled = true;
        PlayRunModeToggle.IsEnabled = !active;
        EditModeToggles.IsEnabled = !active;
        EditorPanel.IsEnabled = !active;
        MapList.IsEnabled = !active;
        LayerTabs.IsEnabled = !active;
        RightList.IsEnabled = !active;
        LightInfoPanel.IsEnabled = !active;
        ActorInfoPanel.IsEnabled = !active;
        RightModePanel.IsEnabled = !active;
        FileExplorerPanel.IsEnabled = !active;
        if (!active)
        {
            if (returnedFromRun
                && viewModel is not null
                && !viewModel.GameConfig.IsModified)
            {
                viewModel.GameConfig.Reload();
            }
            restoreEditorViewport();
        }
        else
            GamePanel.SetInputEnabled(
                projectRunner?.CanSendCommand == true
                && state == ProjectRunState.Running
                && activeWindowMode == ProjectWindowMode.Embedded);
        updateConsoleInputState();
    }

    private async Task<nint> prepareGameViewportAsync(ProjectWindowMode windowMode)
    {
        activeWindowMode = windowMode;
        EditorPanel.IsVisible = false;
        GameViewport.IsVisible = true;
        GameViewport.InvalidateMeasure();
        await Dispatcher.UIThread.InvokeAsync(
            () => GamePanel.TryUpdateNativeControlPosition(),
            DispatcherPriority.Render);
        lockGameLayout();
        if (windowMode != ProjectWindowMode.Embedded)
            return nint.Zero;
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
