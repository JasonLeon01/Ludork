using Avalonia;
using Avalonia.Media;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Globalization;

namespace Ludork.Views;

internal sealed partial class PerformanceMonitorCanvas
{
    private static readonly MainStage[] mainStages =
    [
        new("Runtime", new SolidColorBrush(Color.Parse("#42a5f5")), value => value.Runtime),
        new("Scene Ops", new SolidColorBrush(Color.Parse("#7e57c2")), value => value.SceneOps),
        new("Input", new SolidColorBrush(Color.Parse("#26c6da")), value => value.Input),
        new("UI Update", new SolidColorBrush(Color.Parse("#ab47bc")), value => value.UiUpdate),
        new("Render CPU", new SolidColorBrush(Color.Parse("#ffa726")), value => value.RenderCpu),
        new("Present/VSync", new SolidColorBrush(Color.Parse("#ef5350")), value => value.PresentWait),
        new("LateUpdate", new SolidColorBrush(Color.Parse("#8d6e63")), value => value.LateUpdate),
        new("Audio", new SolidColorBrush(Color.Parse("#d4e157")), value => value.Audio),
    ];
    private static readonly LogicStage[] logicStages =
    [
        new("Update", new SolidColorBrush(Color.Parse("#42a5f5")), value => value.Update),
        new("FixedUpdate", new SolidColorBrush(Color.Parse("#ffa726")), value => value.FixedUpdate),
        new("Maintenance", new SolidColorBrush(Color.Parse("#7e57c2")), value => value.Maintenance),
        new("Idle/Sleep", new SolidColorBrush(Color.Parse("#66bb6a")), value => value.Sleep),
    ];

    private void drawFrameDetails(
        DrawingContext context,
        Rect bounds,
        double dividerY,
        FrameEntry selectedFrame,
        SelectionMode selectionMode)
    {
        int frameIndex = mainFrames.IndexOf(selectedFrame);
        double intervalEnd = getFrameIntervalEnd(frameIndex);
        LogicAggregate logic = getLogicAggregate(selectedFrame.Frame.Time, intervalEnd);
        string mode = getSelectionModeText(selectionMode);
        string summary = LocaleService.Get("PERFORMANCE_MONITOR_SELECTED_FRAME")
            .Replace("{mode}", mode, StringComparison.Ordinal)
            .Replace("{time:.3f}", selectedFrame.Frame.Time.ToString("F3", CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{fps:.1f}", getFrameFps(selectedFrame.Frame).ToString("F1", CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{active:.2f}", selectedFrame.Frame.Active.ToString("F2", CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{interval:.2f}", selectedFrame.Frame.Interval.ToString("F2", CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{memory:.2f}", selectedFrame.MemoryMegabytes.ToString("F2", CultureInfo.CurrentCulture), StringComparison.Ordinal);
        context.DrawText(createText(summary, 9, whiteBrush), new Point(10, dividerY + 7.0));

        double panelTop = dividerY + 27.0;
        double minimumPanelHeight = logic.WorldStreaming is null ? 96.0 : 144.0;
        double panelBottom = Math.Max(panelTop + minimumPanelHeight, bounds.Bottom - 25.0);
        double availableWidth = Math.Max(1.0, bounds.Width - 30.0);
        double mainWidth = Math.Max(1.0, availableWidth * 0.62);
        Rect mainPanel = new(10.0, panelTop, mainWidth, Math.Max(1.0, panelBottom - panelTop));
        Rect logicPanel = new(
            mainPanel.Right + 10.0,
            panelTop,
            Math.Max(1.0, bounds.Right - mainPanel.Right - 20.0),
            Math.Max(1.0, panelBottom - panelTop));
        drawMainThreadPanel(context, mainPanel, selectedFrame.Frame);
        drawLogicThreadPanel(context, logicPanel, logic, selectedFrame.DroppedLogicTicks);
        string note = LocaleService.Get("PERFORMANCE_MONITOR_THREADS_INDEPENDENT");
        context.DrawText(createText(note, 8, mutedBrush), new Point(10.0, bounds.Bottom - 18.0));
    }

    private void drawMainThreadPanel(DrawingContext context, Rect panel, MainFrameTiming frame)
    {
        context.DrawRectangle(panelBrush, panelPen, panel, 4, 4);
        string heading = LocaleService.Get("PERFORMANCE_MONITOR_MAIN_THREAD")
            .Replace("{active:.2f}", duration(frame.Active).ToString("F2", CultureInfo.CurrentCulture), StringComparison.Ordinal);
        context.DrawText(createText(heading, 9, whiteBrush), new Point(panel.Left + 8.0, panel.Top + 6.0));
        StageValue[] values = new StageValue[mainStages.Length];
        for (int index = 0; index < mainStages.Length; index += 1)
        {
            MainStage stage = mainStages[index];
            values[index] = new StageValue(stage.Name, stage.Brush, duration(stage.Value(frame)));
        }
        Rect bar = new(panel.Left + 8.0, panel.Top + 24.0, Math.Max(1.0, panel.Width - 16.0), 11.0);
        drawStageBar(context, bar, values, duration(frame.Active));
        drawStageValues(context, panel, values, panel.Top + 42.0, 2);
    }

    private void drawLogicThreadPanel(
        DrawingContext context,
        Rect panel,
        LogicAggregate logic,
        long droppedTicks)
    {
        context.DrawRectangle(panelBrush, panelPen, panel, 4, 4);
        string heading = logic.Count == 0
            ? LocaleService.Get("PERFORMANCE_MONITOR_NO_LOGIC_TICKS")
            : LocaleService.Get("PERFORMANCE_MONITOR_LOGIC_THREAD")
                .Replace("{count}", logic.Count.ToString(CultureInfo.CurrentCulture), StringComparison.Ordinal)
                .Replace("{iteration:.2f}", logic.Iteration.ToString("F2", CultureInfo.CurrentCulture), StringComparison.Ordinal);
        if (droppedTicks > 0)
            heading += " | " + LocaleService.Get("PERFORMANCE_MONITOR_INCOMPLETE");
        context.DrawText(createText(heading, 9, whiteBrush), new Point(panel.Left + 8.0, panel.Top + 6.0));
        StageValue[] values = new StageValue[logicStages.Length];
        for (int index = 0; index < logicStages.Length; index += 1)
        {
            LogicStage stage = logicStages[index];
            values[index] = new StageValue(stage.Name, stage.Brush, stage.Value(logic));
        }
        Rect bar = new(panel.Left + 8.0, panel.Top + 24.0, Math.Max(1.0, panel.Width - 16.0), 11.0);
        drawStageBar(context, bar, values, logic.Iteration);
        drawStageValues(context, panel, values, panel.Top + 42.0, 2);
        string footer = "fixedSteps: " + logic.FixedSteps.ToString(CultureInfo.CurrentCulture);
        if (droppedTicks > 0)
        {
            footer += " | "
                + LocaleService.Get("PERFORMANCE_MONITOR_DROPPED_LOGIC_TICKS")
                    .Replace("{count}", droppedTicks.ToString(CultureInfo.CurrentCulture), StringComparison.Ordinal);
        }
        context.DrawText(createText(footer, 8, labelBrush), new Point(panel.Left + 8.0, panel.Top + 78.0));
        drawWorldStreaming(context, panel, logic.WorldStreaming);
    }

    private static void drawWorldStreaming(
        DrawingContext context,
        Rect panel,
        WorldStreamingTiming? streaming)
    {
        if (streaming is null)
            return;
        string states = LocaleService.Get("PERFORMANCE_MONITOR_WORLD_STREAMING_STATES")
            .Replace("{queue}", streaming.QueueDepth.ToString(CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{reading}", streaming.Reading.ToString(CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{prepared}", streaming.Prepared.ToString(CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{active}", streaming.Active.ToString(CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{dormant}", streaming.Dormant.ToString(CultureInfo.CurrentCulture), StringComparison.Ordinal);
        string cache = LocaleService.Get("PERFORMANCE_MONITOR_WORLD_STREAMING_CACHE")
            .Replace("{cache:.2f}", (streaming.CacheBytes / (1024.0 * 1024.0)).ToString("F2", CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{chunks}", streaming.VisibleTileChunks.ToString(CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{actors}", streaming.ActiveActors.ToString(CultureInfo.CurrentCulture), StringComparison.Ordinal);
        string publish = LocaleService.Get("PERFORMANCE_MONITOR_WORLD_STREAMING_PUBLISH")
            .Replace("{publish:.2f}", streaming.PublishMilliseconds.ToString("F2", CultureInfo.CurrentCulture), StringComparison.Ordinal);
        context.DrawText(createText(states, 7, mutedBrush), new Point(panel.Left + 8.0, panel.Top + 94.0));
        context.DrawText(createText(cache, 7, mutedBrush), new Point(panel.Left + 8.0, panel.Top + 110.0));
        context.DrawText(createText(publish, 7, mutedBrush), new Point(panel.Left + 8.0, panel.Top + 126.0));
    }

    private static void drawStageBar(
        DrawingContext context,
        Rect bar,
        IReadOnlyList<StageValue> values,
        double measuredTotal)
    {
        context.DrawRectangle(new SolidColorBrush(Color.Parse("#171717")), panelPen, bar, 2, 2);
        double phaseTotal = 0.0;
        foreach (StageValue value in values)
            phaseTotal += value.Milliseconds;
        double denominator = Math.Max(measuredTotal, phaseTotal);
        if (denominator <= 0)
            return;
        double x = bar.Left;
        foreach (StageValue value in values)
        {
            double width = bar.Width * value.Milliseconds / denominator;
            if (width <= 0)
                continue;
            context.FillRectangle(value.Brush, new Rect(x, bar.Top, width, bar.Height));
            x += width;
        }
    }

    private static void drawStageValues(
        DrawingContext context,
        Rect panel,
        IReadOnlyList<StageValue> values,
        double top,
        int columns)
    {
        double columnWidth = Math.Max(1.0, (panel.Width - 16.0) / columns);
        for (int index = 0; index < values.Count; index += 1)
        {
            StageValue value = values[index];
            int column = index % columns;
            int row = index / columns;
            double x = panel.Left + 8.0 + column * columnWidth;
            double y = top + row * 16.0;
            context.FillRectangle(value.Brush, new Rect(x, y + 3.0, 7.0, 7.0));
            string text = value.Name
                + " "
                + value.Milliseconds.ToString("F2", CultureInfo.CurrentCulture)
                + " ms";
            context.DrawText(createText(text, 8, labelBrush), new Point(x + 11.0, y));
        }
    }

    private double getFrameIntervalEnd(int frameIndex)
    {
        MainFrameTiming frame = mainFrames[frameIndex].Frame;
        if (frameIndex + 1 < mainFrames.Count
            && mainFrames[frameIndex + 1].Frame.Time > frame.Time)
        {
            return mainFrames[frameIndex + 1].Frame.Time;
        }
        double intervalSeconds = Math.Max(duration(frame.Interval), duration(frame.Active)) / 1000.0;
        return frame.Time + Math.Max(0.001, intervalSeconds);
    }

    private LogicAggregate getLogicAggregate(double startTime, double endTime)
    {
        int count = 0;
        double iteration = 0.0;
        double update = 0.0;
        double maintenance = 0.0;
        double fixedUpdate = 0.0;
        double sleep = 0.0;
        long fixedSteps = 0;
        WorldStreamingTiming? worldStreaming = null;
        foreach (LogicTickTiming tick in logicTicks)
        {
            if (tick.Time < startTime || tick.Time >= endTime)
                continue;
            count += 1;
            iteration += duration(tick.Iteration);
            update += duration(tick.SceneTick);
            maintenance += duration(tick.Maintenance);
            fixedUpdate += duration(tick.FixedTick);
            sleep += duration(tick.Sleep);
            fixedSteps = tick.FixedSteps > long.MaxValue - fixedSteps
                ? long.MaxValue
                : fixedSteps + Math.Max(0, tick.FixedSteps);
            if (tick.WorldStreaming is not null)
                worldStreaming = tick.WorldStreaming;
        }
        return new LogicAggregate(count, iteration, update, maintenance, fixedUpdate, sleep, fixedSteps, worldStreaming);
    }

    private bool hasWorldStreaming()
    {
        foreach (LogicTickTiming tick in logicTicks)
        {
            if (tick.WorldStreaming is not null)
                return true;
        }
        return false;
    }

    private static double getDividerY(Rect bounds, bool hasWorldStreaming)
    {
        double detailReserve = hasWorldStreaming ? 196.0 : 148.0;
        double maximum = Math.Max(125.0, bounds.Height - detailReserve);
        return Math.Clamp(bounds.Height * 0.5, 125.0, maximum);
    }

    private sealed record MainStage(
        string Name,
        IBrush Brush,
        Func<MainFrameTiming, double> Value);

    private sealed record LogicStage(
        string Name,
        IBrush Brush,
        Func<LogicAggregate, double> Value);

    private sealed record StageValue(string Name, IBrush Brush, double Milliseconds);

    private sealed record LogicAggregate(
        int Count,
        double Iteration,
        double Update,
        double Maintenance,
        double FixedUpdate,
        double Sleep,
        long FixedSteps,
        WorldStreamingTiming? WorldStreaming);
}
