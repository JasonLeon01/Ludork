using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Globalization;

namespace Ludork.Views;

public sealed class PerformanceMonitorWindow : Window
{
    private readonly PerformanceMonitorCanvas canvas = new();
    private double? lastMainFrameTime;

    public PerformanceMonitorWindow()
    {
        Title = LocaleService.Get("PERFORMANCE_MONITOR");
        Width = 800;
        Height = 400;
        MinWidth = 720;
        MinHeight = 340;
        Background = new SolidColorBrush(Color.Parse("#1e1e1e"));
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);
        Content = canvas;
    }

    public void ClearData()
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(ClearData);
            return;
        }
        canvas.ClearData();
        lastMainFrameTime = null;
    }

    public void AddSample(PerformanceSample sample)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => AddSample(sample));
            return;
        }
        if (sample.ProtocolVersion != 2 || sample.Fps <= 0 || !double.IsFinite(sample.Fps))
            return;
        if (lastMainFrameTime is double previousTime
            && sample.MainFrames.Count != 0
            && sample.MainFrames[0].Time < previousTime)
        {
            canvas.ClearData();
        }
        if (sample.MainFrames.Count != 0)
            lastMainFrameTime = sample.MainFrames[^1].Time;
        canvas.AddSample(sample);
    }

    public void AddSample(double fps, double memoryMegabytes)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => AddSample(fps, memoryMegabytes));
            return;
        }
        if (!double.IsFinite(fps) || fps <= 0)
            return;
        double active = 1000.0 / fps;
        double time = lastMainFrameTime is double previousTime
            ? previousTime + active / 1000.0
            : 0.0;
        MainFrameTiming frame = new(
            time,
            active,
            active,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0);
        AddSample(new PerformanceSample(fps, memoryMegabytes)
        {
            SampleFrames = 1,
            MainFrames = new[] { frame },
        });
    }
}

internal sealed partial class PerformanceMonitorCanvas : Control
{
    private const double HistorySeconds = 30.0;
    private const double VisibleSeconds = 5.0;
    private const double GraphLeft = 42.0;
    private const double GraphRight = 12.0;
    private const double GraphTop = 43.0;
    private static readonly Typeface typeface = new("Arial");
    private static readonly IBrush backgroundBrush = new SolidColorBrush(Color.Parse("#1e1e1e"));
    private static readonly IBrush panelBrush = new SolidColorBrush(Color.Parse("#252525"));
    private static readonly IBrush waitingBrush = new SolidColorBrush(Color.Parse("#b4b4b4"));
    private static readonly IBrush labelBrush = new SolidColorBrush(Color.Parse("#c8c8c8"));
    private static readonly IBrush mutedBrush = new SolidColorBrush(Color.Parse("#9e9e9e"));
    private static readonly IBrush whiteBrush = Brushes.White;
    private static readonly IBrush liveBrush = new SolidColorBrush(Color.Parse("#2e7d32"));
    private static readonly IBrush inactiveActionBrush = new SolidColorBrush(Color.Parse("#3a3a3a"));
    private static readonly Pen panelPen = new(new SolidColorBrush(Color.Parse("#484848")), 1);
    private static readonly Pen dividerPen = new(new SolidColorBrush(Color.Parse("#505050")), 1);
    private static readonly Pen gridPen = new(
        new SolidColorBrush(Color.Parse("#5b5b5b")),
        1,
        DashStyle.Dash);
    private static readonly Pen curvePen = new(new SolidColorBrush(Color.Parse("#00e676")), 1.5);
    private static readonly Pen targetPen = new(
        new SolidColorBrush(Color.Parse("#64b5f6")),
        1,
        DashStyle.Dash);
    private static readonly Pen liveSelectionPen = new(new SolidColorBrush(Color.Parse("#69f0ae")), 1);
    private static readonly Pen lockedSelectionPen = new(new SolidColorBrush(Color.Parse("#ffb74d")), 1.5);
    private static readonly Pen previewSelectionPen = new(
        new SolidColorBrush(Color.Parse("#ffff00")),
        1,
        DashStyle.Dash);
    private readonly List<FrameEntry> mainFrames = [];
    private readonly List<LogicTickTiming> logicTicks = [];
    private double? lockedFrameTime;
    private double? lockedRangeMinimum;
    private double? lockedRangeMaximum;
    private double? hoveredFrameTime;
    private Rect graphBounds;
    private Rect liveActionBounds;

    public PerformanceMonitorCanvas()
    {
        ClipToBounds = true;
        Focusable = true;
        PointerExited += (_, _) =>
        {
            hoveredFrameTime = null;
            Cursor = Cursor.Default;
            InvalidateVisual();
        };
    }

    public void ClearData()
    {
        mainFrames.Clear();
        logicTicks.Clear();
        lockedFrameTime = null;
        lockedRangeMinimum = null;
        lockedRangeMaximum = null;
        hoveredFrameTime = null;
        InvalidateVisual();
    }

    public void AddSample(PerformanceSample sample)
    {
        if (sample.ProtocolVersion != 2)
            return;
        double memory = double.IsFinite(sample.MemoryMegabytes)
            ? Math.Max(0.0, sample.MemoryMegabytes)
            : 0.0;
        double? targetFps = sample.TargetFps is double currentTarget
            && double.IsFinite(currentTarget)
            && currentTarget > 0
            ? currentTarget
            : null;
        foreach (MainFrameTiming frame in sample.MainFrames)
        {
            if (!double.IsFinite(frame.Time)
                || frame.Time < 0
                || !double.IsFinite(frame.Interval)
                || frame.Interval < 0
                || !double.IsFinite(frame.Active)
                || frame.Active < 0)
            {
                continue;
            }
            mainFrames.Add(new FrameEntry(
                frame,
                memory,
                targetFps,
                Math.Max(0L, sample.DroppedLogicTicks)));
        }
        foreach (LogicTickTiming tick in sample.LogicTicks)
        {
            if (double.IsFinite(tick.Time) && tick.Time >= 0)
                logicTicks.Add(tick);
        }
        mainFrames.Sort((left, right) => left.Frame.Time.CompareTo(right.Frame.Time));
        logicTicks.Sort((left, right) => left.Time.CompareTo(right.Time));
        trimHistory();
        if (lockedFrameTime is double lockedTime && findFrame(lockedTime) is null)
        {
            lockedFrameTime = null;
            lockedRangeMinimum = null;
            lockedRangeMaximum = null;
        }
        if (hoveredFrameTime is double hoverTime && findFrame(hoverTime) is null)
            hoveredFrameTime = null;
        InvalidateVisual();
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);
        Rect bounds = new(Bounds.Size);
        context.FillRectangle(backgroundBrush, bounds);
        double dividerY = getDividerY(bounds, hasWorldStreaming());
        graphBounds = new Rect(
            GraphLeft,
            GraphTop,
            Math.Max(1.0, bounds.Width - GraphLeft - GraphRight),
            Math.Max(1.0, dividerY - GraphTop - 14.0));
        liveActionBounds = new Rect(Math.Max(8.0, bounds.Right - 104.0), 8.0, 92.0, 24.0);
        if (mainFrames.Count == 0)
        {
            drawLiveAction(context);
            drawWaiting(context, bounds);
            return;
        }

        FrameEntry selectedFrame = getSelectedFrame();
        SelectionMode selectionMode = getSelectionMode();
        (double minimumTime, double maximumTime) = getGraphTimeRange();
        FpsStatistics statistics = getFpsStatistics(minimumTime, maximumTime);
        drawHeader(context, statistics, selectedFrame);
        drawLiveAction(context);
        drawFpsGrid(context, statistics.AxisMaximum);
        drawTargetFps(context, selectedFrame, statistics.AxisMaximum);
        drawFpsCurve(context, minimumTime, maximumTime, statistics.AxisMaximum);
        drawTimeLabels(context, minimumTime, maximumTime);
        drawSelection(context, selectedFrame, selectionMode, minimumTime, maximumTime, statistics.AxisMaximum);
        context.DrawLine(dividerPen, new Point(0, dividerY), new Point(bounds.Right, dividerY));
        drawFrameDetails(context, bounds, dividerY, selectedFrame, selectionMode);
    }

    protected override void OnPointerMoved(PointerEventArgs args)
    {
        base.OnPointerMoved(args);
        Point position = args.GetPosition(this);
        bool interactive = graphBounds.Contains(position) || liveActionBounds.Contains(position);
        Cursor = interactive ? new Cursor(StandardCursorType.Hand) : Cursor.Default;
        if (lockedFrameTime is not null || !graphBounds.Contains(position) || mainFrames.Count == 0)
        {
            if (hoveredFrameTime is not null)
            {
                hoveredFrameTime = null;
                InvalidateVisual();
            }
            return;
        }
        FrameEntry frame = findNearestFrame(position.X);
        if (hoveredFrameTime == frame.Frame.Time)
            return;
        hoveredFrameTime = frame.Frame.Time;
        InvalidateVisual();
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed)
            return;
        if (liveActionBounds.Contains(point.Position))
        {
            lockedFrameTime = null;
            lockedRangeMinimum = null;
            lockedRangeMaximum = null;
            hoveredFrameTime = null;
            Focus();
            args.Handled = true;
            InvalidateVisual();
            return;
        }
        if (!graphBounds.Contains(point.Position) || mainFrames.Count == 0)
            return;
        FrameEntry frame = findNearestFrame(point.Position.X);
        if (lockedFrameTime == frame.Frame.Time)
        {
            lockedFrameTime = null;
            lockedRangeMinimum = null;
            lockedRangeMaximum = null;
        }
        else
        {
            (double minimumTime, double maximumTime) = getGraphTimeRange();
            lockedFrameTime = frame.Frame.Time;
            lockedRangeMinimum = minimumTime;
            lockedRangeMaximum = maximumTime;
        }
        hoveredFrameTime = null;
        Focus();
        args.Handled = true;
        InvalidateVisual();
    }

    private void trimHistory()
    {
        double latestTime = double.MinValue;
        if (mainFrames.Count != 0)
            latestTime = Math.Max(latestTime, mainFrames[^1].Frame.Time);
        if (logicTicks.Count != 0)
            latestTime = Math.Max(latestTime, logicTicks[^1].Time);
        if (!double.IsFinite(latestTime))
            return;
        double minimumTime = latestTime - HistorySeconds;
        mainFrames.RemoveAll(value => value.Frame.Time < minimumTime);
        logicTicks.RemoveAll(value => value.Time < minimumTime);
    }

    private FrameEntry getSelectedFrame()
    {
        if (lockedFrameTime is double lockedTime && findFrame(lockedTime) is FrameEntry locked)
            return locked;
        if (hoveredFrameTime is double hoverTime && findFrame(hoverTime) is FrameEntry hovered)
            return hovered;
        return mainFrames[^1];
    }

    private SelectionMode getSelectionMode()
    {
        if (lockedFrameTime is not null)
            return SelectionMode.Locked;
        if (hoveredFrameTime is not null)
            return SelectionMode.Preview;
        return SelectionMode.Live;
    }

    private FrameEntry? findFrame(double time)
    {
        foreach (FrameEntry frame in mainFrames)
        {
            if (frame.Frame.Time == time)
                return frame;
        }
        return null;
    }

    private FrameEntry findNearestFrame(double x)
    {
        (double minimumTime, double maximumTime) = getGraphTimeRange();
        double targetTime = minimumTime
            + Math.Clamp((x - graphBounds.Left) / graphBounds.Width, 0.0, 1.0)
            * (maximumTime - minimumTime);
        FrameEntry? closest = null;
        double closestDistance = double.MaxValue;
        for (int index = 0; index < mainFrames.Count; index += 1)
        {
            FrameEntry candidate = mainFrames[index];
            if (candidate.Frame.Time < minimumTime || candidate.Frame.Time > maximumTime)
                continue;
            double distance = Math.Abs(candidate.Frame.Time - targetTime);
            if (distance >= closestDistance)
                continue;
            closest = candidate;
            closestDistance = distance;
        }
        return closest ?? mainFrames[^1];
    }

    private (double Minimum, double Maximum) getGraphTimeRange()
    {
        if (lockedFrameTime is not null
            && lockedRangeMinimum is double frozenMinimum
            && lockedRangeMaximum is double frozenMaximum
            && frozenMaximum > frozenMinimum)
        {
            return (frozenMinimum, frozenMaximum);
        }
        double maximum = mainFrames[^1].Frame.Time;
        double minimum = Math.Max(mainFrames[0].Frame.Time, maximum - VisibleSeconds);
        if (maximum - minimum < 0.001)
        {
            minimum = Math.Max(0.0, maximum - 1.0);
            if (maximum - minimum < 0.001)
                maximum = minimum + 1.0;
        }
        return (minimum, maximum);
    }

    private FpsStatistics getFpsStatistics(double minimumTime, double maximumTime)
    {
        double minimum = double.MaxValue;
        double maximum = 0.0;
        double totalActive = 0.0;
        int count = 0;
        foreach (FrameEntry entry in mainFrames)
        {
            if (entry.Frame.Time < minimumTime || entry.Frame.Time > maximumTime)
                continue;
            double fps = getFrameFps(entry.Frame);
            if (fps <= 0)
                continue;
            minimum = Math.Min(minimum, fps);
            maximum = Math.Max(maximum, fps);
            totalActive += duration(entry.Frame.Active);
            count += 1;
        }
        if (count == 0)
            return new FpsStatistics(0.0, 0.0, 0.0, 65.0, 0);
        return new FpsStatistics(
            count * 1000.0 / totalActive,
            minimum,
            maximum,
            Math.Max(65.0, maximum * 1.1),
            count);
    }

    private void drawHeader(
        DrawingContext context,
        FpsStatistics statistics,
        FrameEntry selectedFrame)
    {
        string text = LocaleService.Get("PERFORMANCE_MONITOR_FRAME_STATS")
            .Replace("{avg:.1f}", statistics.Average.ToString("F1", CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{max:.1f}", statistics.Maximum.ToString("F1", CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{min:.1f}", statistics.Minimum.ToString("F1", CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{memory:.1f}", selectedFrame.MemoryMegabytes.ToString("F1", CultureInfo.CurrentCulture), StringComparison.Ordinal)
            .Replace("{count}", statistics.Count.ToString(CultureInfo.CurrentCulture), StringComparison.Ordinal);
        using (context.PushClip(new Rect(
            0.0,
            0.0,
            Math.Max(1.0, liveActionBounds.Left - 8.0),
            GraphTop)))
        {
            context.DrawText(createText(text, 9, whiteBrush), new Point(10, 7));
            context.DrawText(
                createText(LocaleService.Get("PERFORMANCE_MONITOR_INTERACTION_HINT"), 8, mutedBrush),
                new Point(10, 24));
        }
    }

    private void drawLiveAction(DrawingContext context)
    {
        IBrush brush = lockedFrameTime is null ? liveBrush : inactiveActionBrush;
        context.DrawRectangle(brush, panelPen, liveActionBounds, 4, 4);
        FormattedText text = createText(LocaleService.Get("PERFORMANCE_MONITOR_LIVE_ACTION"), 9, whiteBrush);
        context.DrawText(text, new Point(
            liveActionBounds.Left + Math.Max(0.0, (liveActionBounds.Width - text.Width) / 2.0),
            liveActionBounds.Top + Math.Max(0.0, (liveActionBounds.Height - text.Height) / 2.0)));
    }

    private void drawFpsGrid(DrawingContext context, double maximum)
    {
        context.DrawRectangle(null, panelPen, graphBounds);
        foreach (int mark in new[] { 30, 60, 90, 120, 150 })
        {
            double y = graphBounds.Bottom - mark / maximum * graphBounds.Height;
            if (y < graphBounds.Top || y > graphBounds.Bottom)
                continue;
            context.DrawLine(gridPen, new Point(graphBounds.Left, y), new Point(graphBounds.Right, y));
            FormattedText label = createText(mark.ToString(CultureInfo.InvariantCulture), 8, labelBrush);
            context.DrawText(label, new Point(5, y - label.Height / 2.0));
        }
    }

    private void drawTargetFps(DrawingContext context, FrameEntry selectedFrame, double maximum)
    {
        if (selectedFrame.TargetFps is not double target || target <= 0)
            return;
        double y = graphBounds.Bottom - target / maximum * graphBounds.Height;
        if (y < graphBounds.Top || y > graphBounds.Bottom)
            return;
        context.DrawLine(targetPen, new Point(graphBounds.Left, y), new Point(graphBounds.Right, y));
    }

    private void drawFpsCurve(
        DrawingContext context,
        double minimumTime,
        double maximumTime,
        double maximumFps)
    {
        Point? previous = null;
        foreach (FrameEntry entry in mainFrames)
        {
            if (entry.Frame.Time < minimumTime || entry.Frame.Time > maximumTime)
            {
                previous = null;
                continue;
            }
            double fps = getFrameFps(entry.Frame);
            if (fps <= 0)
            {
                previous = null;
                continue;
            }
            Point current = new(
                timeToX(entry.Frame.Time, minimumTime, maximumTime),
                graphBounds.Bottom - Math.Clamp(fps / maximumFps, 0.0, 1.0) * graphBounds.Height);
            if (previous is Point start)
                context.DrawLine(curvePen, start, current);
            else
                context.DrawEllipse(curvePen.Brush, null, current, 1.5, 1.5);
            previous = current;
        }
    }

    private void drawTimeLabels(DrawingContext context, double minimumTime, double maximumTime)
    {
        FormattedText left = createText(minimumTime.ToString("F1", CultureInfo.CurrentCulture) + "s", 7, mutedBrush);
        FormattedText right = createText(maximumTime.ToString("F1", CultureInfo.CurrentCulture) + "s", 7, mutedBrush);
        context.DrawText(left, new Point(graphBounds.Left, graphBounds.Bottom + 1.0));
        context.DrawText(right, new Point(graphBounds.Right - right.Width, graphBounds.Bottom + 1.0));
    }

    private void drawSelection(
        DrawingContext context,
        FrameEntry frame,
        SelectionMode mode,
        double minimumTime,
        double maximumTime,
        double maximumFps)
    {
        double fps = getFrameFps(frame.Frame);
        double x = timeToX(frame.Frame.Time, minimumTime, maximumTime);
        double y = graphBounds.Bottom - Math.Clamp(fps / maximumFps, 0.0, 1.0) * graphBounds.Height;
        Pen pen = mode switch
        {
            SelectionMode.Preview => previewSelectionPen,
            SelectionMode.Locked => lockedSelectionPen,
            _ => liveSelectionPen,
        };
        context.DrawLine(pen, new Point(x, graphBounds.Top), new Point(x, graphBounds.Bottom));
        context.DrawEllipse(pen.Brush, null, new Point(x, y), 3.0, 3.0);
        string text = frame.Frame.Time.ToString("F3", CultureInfo.CurrentCulture)
            + "s  "
            + fps.ToString("F1", CultureInfo.CurrentCulture)
            + " FPS";
        FormattedText formatted = createText(text, 8, whiteBrush);
        double left = x + 7.0;
        if (left + formatted.Width + 8.0 > graphBounds.Right)
            left = x - formatted.Width - 15.0;
        double top = Math.Clamp(y - formatted.Height - 8.0, graphBounds.Top + 3.0, graphBounds.Bottom - formatted.Height - 7.0);
        Rect tooltip = new(left, top, formatted.Width + 8.0, formatted.Height + 5.0);
        context.DrawRectangle(new SolidColorBrush(Color.Parse("#d0000000")), null, tooltip, 3, 3);
        context.DrawText(formatted, new Point(left + 4.0, top + 2.0));
    }

    private static string getSelectionModeText(SelectionMode mode)
    {
        return mode switch
        {
            SelectionMode.Preview => LocaleService.Get("PERFORMANCE_MONITOR_PREVIEW"),
            SelectionMode.Locked => LocaleService.Get("PERFORMANCE_MONITOR_LOCKED"),
            _ => LocaleService.Get("PERFORMANCE_MONITOR_LIVE"),
        };
    }

    private static double getFrameFps(MainFrameTiming frame)
    {
        double active = duration(frame.Active);
        return active > 0 ? 1000.0 / active : 0.0;
    }

    private static double duration(double value)
    {
        return double.IsFinite(value) ? Math.Max(0.0, value) : 0.0;
    }

    private double timeToX(double time, double minimumTime, double maximumTime)
    {
        double range = Math.Max(0.001, maximumTime - minimumTime);
        return graphBounds.Left + (time - minimumTime) / range * graphBounds.Width;
    }

    private static void drawWaiting(DrawingContext context, Rect bounds)
    {
        FormattedText text = createText(LocaleService.Get("PERFORMANCE_MONITOR_WAITING"), 12, waitingBrush);
        context.DrawText(text, new Point(
            Math.Max(0.0, (bounds.Width - text.Width) / 2.0),
            Math.Max(0.0, (bounds.Height - text.Height) / 2.0)));
    }

    private static FormattedText createText(string text, double size, IBrush brush)
    {
        return new FormattedText(
            text,
            CultureInfo.CurrentCulture,
            FlowDirection.LeftToRight,
            typeface,
            size,
            brush);
    }

    private enum SelectionMode
    {
        Live,
        Preview,
        Locked,
    }

    private sealed record FrameEntry(
        MainFrameTiming Frame,
        double MemoryMegabytes,
        double? TargetFps,
        long DroppedLogicTicks);

    private sealed record FpsStatistics(
        double Average,
        double Minimum,
        double Maximum,
        double AxisMaximum,
        int Count);
}
