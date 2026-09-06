using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.VisualTree;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class AnimationTimeline : Control
{
    internal const string AssetDragPrefix = "ludork-animation-assets:";
    private const double SegmentMarqueeThreshold = 4;
    private readonly string projectPath;
    private readonly Func<JsonObject> getData;
    private readonly HashSet<(int Track, int Segment)> segmentSelectionBeforeMarquee = [];
    private readonly HashSet<(int Track, int Segment)> selectedSegments = [];
    private Point dragStart;
    private Point segmentMarqueeStart;
    private Rect segmentMarqueeRect;
    private double originalStart;
    private double originalEnd;
    private JsonObject? draggedTimeTag;
    private double timeTagPointerOffset;
    private bool timeTagMoved;
    private bool segmentMarqueeActive;
    private bool segmentMarqueeAdditive;
    private bool segmentMarqueeToggle;
    private int dragMode;
    private const int ScrubDragMode = 4;
    private const int TimeTagDragMode = 5;
    private const int SegmentMarqueeDragMode = 6;
    private const double HeaderHeight = 28;
    private const double TrackHeight = 38;
    private const double BasePixelsPerSecond = 300;
    private readonly EditorZoomInput zoomInput = new();
    private ScrollViewer? hostScrollViewer;
    private double zoom = 1.0;
    private TimelineZoomAnchor? pendingZoomAnchor;

    public AnimationTimeline(string projectPath, Func<JsonObject> getData)
    {
        this.projectPath = projectPath;
        this.getData = getData;
        Focusable = true;
        DragDrop.SetAllowDrop(this, true);
        AddHandler(DragDrop.DragOverEvent, onDragOver);
        AddHandler(DragDrop.DropEvent, onDrop);
        AddHandler(
            PointerCaptureLostEvent,
            onPointerCaptureLost,
            RoutingStrategies.Tunnel
        );
        PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
    }

    public event Action<int, int>? SegmentSelected;
    public event Action? SegmentChanged;
    public event Action? TimeTagChanged;
    public event Action<int>? TimeTagRenameRequested;
    public event Action<double>? TimeChanged;
    public event Action<double>? ZoomChanged;
    public double CurrentTime { get; private set; }
    public int SelectedTrack { get; set; } = -1;
    public int SelectedSegment { get; set; } = -1;
    public int SelectedTimeTag { get; private set; } = -1;

    public double PixelsPerSecond => BasePixelsPerSecond * zoom;

    public void Refresh()
    {
        JsonObject? selectedTimeTag = GetTimeTag(SelectedTimeTag);
        SelectedTimeTag = sortTimeTags(selectedTimeTag);
        selectedSegments.IntersectWith(validSelectedSegments());
        updatePrimarySegmentSelection();
        updateCanvasSize();
        InvalidateVisual();
    }

    public void SetZoom(double value)
    {
        setZoom(value, null);
    }

    public void AdjustZoom(double delta) => SetZoom(zoom + delta);

    public void ClearTimeTagSelection()
    {
        SelectedTimeTag = -1;
        InvalidateVisual();
    }

    public void SetSegmentSelection(int track, int segment)
    {
        selectedSegments.Clear();
        if (segmentAt(track, segment) is not null)
            selectedSegments.Add((track, segment));
        updatePrimarySegmentSelection();
        InvalidateVisual();
    }

    public void AddTimeTag(string tag)
    {
        JsonObject timeTag = new()
        {
            ["tag"] = tag,
            ["time"] = CurrentTime,
        };
        timeTags().Add(timeTag);
        SelectedTimeTag = sortTimeTags(timeTag);
        TimeTagChanged?.Invoke();
        Refresh();
    }

    public JsonObject? GetTimeTag(int index)
    {
        JsonArray tags = timeTags();
        return index >= 0 && index < tags.Count ? tags[index] as JsonObject : null;
    }

    public void RenameTimeTag(int index, string tag)
    {
        if (GetTimeTag(index) is not JsonObject timeTag)
            return;
        timeTag["tag"] = tag;
        SelectedTimeTag = index;
        TimeTagChanged?.Invoke();
        Refresh();
    }

    public bool DeleteSelectedTimeTag()
    {
        JsonArray tags = timeTags();
        if (SelectedTimeTag < 0 || SelectedTimeTag >= tags.Count)
            return false;
        tags.RemoveAt(SelectedTimeTag);
        SelectedTimeTag = -1;
        TimeTagChanged?.Invoke();
        Refresh();
        return true;
    }

    public int FindAvailableTrack(double start, double end)
    {
        JsonArray tracks = lines();
        for (int track = 0; track < tracks.Count; track += 1)
        {
            if (!overlaps(track, start, end, -1))
                return track;
        }
        return tracks.Count;
    }

    public (int Track, JsonObject Segment)? GetSelectedSegmentData()
    {
        return selectedSegments.Count == 1
            && segmentAt(SelectedTrack, SelectedSegment) is JsonObject segment
            ? (SelectedTrack, (JsonObject)segment.DeepClone())
            : null;
    }

    public bool DeleteSelectedSegment()
    {
        HashSet<(int Track, int Segment)> selection = validSelectedSegments();
        if (selection.Count == 0)
            return false;
        JsonArray tracks = lines();
        foreach (IGrouping<int, (int Track, int Segment)> group in selection
            .GroupBy(item => item.Track)
            .OrderByDescending(group => group.Key))
        {
            if (tracks[group.Key] is not JsonObject line
                || line["timeSegments"] is not JsonArray segments)
            {
                continue;
            }
            foreach ((int Track, int Segment) item in group.OrderByDescending(item => item.Segment))
                segments.RemoveAt(item.Segment);
        }
        selectedSegments.Clear();
        updatePrimarySegmentSelection();
        SegmentSelected?.Invoke(-1, -1);
        SegmentChanged?.Invoke();
        Refresh();
        return true;
    }

    public bool InsertSegmentAt(int track, JsonObject segment, double startTime)
    {
        JsonArray tracks = lines();
        while (tracks.Count <= track)
            tracks.Add(new JsonObject { ["timeSegments"] = new JsonArray() });
        JsonArray segments = ((JsonObject)tracks[track]!)["timeSegments"]!.AsArray();
        JsonObject startFrame = segment["startFrame"] as JsonObject ?? new JsonObject();
        JsonObject endFrame = segment["endFrame"] as JsonObject ?? new JsonObject();
        double duration = Math.Max(minimumDuration(segment), AnimationEditor.number(endFrame["time"]) - AnimationEditor.number(startFrame["time"]));
        double start = clampInsertStart(track, -1, Math.Max(0, snap(startTime)), duration);
        if (overlaps(track, start, start + duration, -1))
            return false;
        startFrame["time"] = start;
        endFrame["time"] = start + duration;
        segment["startFrame"] = startFrame;
        segment["endFrame"] = endFrame;
        segments.Add(segment);
        SetSegmentSelection(track, segments.Count - 1);
        SegmentSelected?.Invoke(SelectedTrack, SelectedSegment);
        SegmentChanged?.Invoke();
        Refresh();
        return true;
    }

    public bool DuplicateSelectedSegment()
    {
        if (selectedSegments.Count != 1
            || segmentAt(SelectedTrack, SelectedSegment) is not JsonObject source)
            return false;
        double end = AnimationEditor.number((source["endFrame"] as JsonObject)?["time"]);
        return InsertSegmentAt(SelectedTrack, (JsonObject)source.DeepClone(), end);
    }

    public bool NudgeSelectedSegment(int frameDelta)
    {
        if (selectedSegments.Count != 1
            || segmentAt(SelectedTrack, SelectedSegment) is not JsonObject segment)
            return false;
        JsonObject startFrame = segment["startFrame"] as JsonObject ?? new JsonObject();
        JsonObject endFrame = segment["endFrame"] as JsonObject ?? new JsonObject();
        double start = AnimationEditor.number(startFrame["time"]);
        double end = AnimationEditor.number(endFrame["time"]);
        double duration = end - start;
        double savedStart = originalStart;
        double savedEnd = originalEnd;
        originalStart = start;
        originalEnd = end;
        (double left, double right) = findBounds(SelectedTrack, SelectedSegment);
        originalStart = savedStart;
        originalEnd = savedEnd;
        double next = Math.Max(Math.Max(0, left), Math.Min(snap(start + frameDelta / (double)frameRate()), right - duration));
        if (Math.Abs(next - start) < 0.0001)
            return false;
        startFrame["time"] = next;
        endFrame["time"] = next + duration;
        SegmentChanged?.Invoke();
        Refresh();
        return true;
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);
        Rect bounds = new(Bounds.Size);
        context.FillRectangle(new SolidColorBrush(Color.Parse("#292929")), bounds);
        context.FillRectangle(new SolidColorBrush(Color.Parse("#383838")), new Rect(0, 0, bounds.Width, HeaderHeight));
        Pen gridPen = new(new SolidColorBrush(Color.Parse("#505050")), 1);
        for (double second = 0; second * PixelsPerSecond < bounds.Width; second += 1)
            context.DrawLine(gridPen, new Point(second * PixelsPerSecond, 0), new Point(second * PixelsPerSecond, bounds.Height));

        JsonArray tracks = lines();
        for (int track = 0; track < Math.Max(5, tracks.Count); track += 1)
        {
            double y = HeaderHeight + track * TrackHeight;
            context.FillRectangle(new SolidColorBrush(Color.Parse(track % 2 == 0 ? "#303030" : "#2c2c2c")), new Rect(0, y, bounds.Width, TrackHeight));
            if (track >= tracks.Count || tracks[track] is not JsonObject line || line["timeSegments"] is not JsonArray segments)
                continue;
            for (int segmentIndex = 0; segmentIndex < segments.Count; segmentIndex += 1)
            {
                if (segments[segmentIndex] is not JsonObject segment)
                    continue;
                Rect rect = segmentRect(track, segment);
                bool selected = selectedSegments.Contains((track, segmentIndex));
                bool sound = segment["type"]?.GetValue<string>() == "sound";
                context.FillRectangle(new SolidColorBrush(Color.Parse(selected ? "#8ab4f8" : sound ? "#76573b" : "#4f89b8")), rect, 4);
                context.DrawRectangle(selected ? new Pen(Brushes.White, 2) : new Pen(new SolidColorBrush(Color.Parse("#a9cce8")), 1), rect, 4);
            }
        }
        if (segmentMarqueeActive)
        {
            context.FillRectangle(
                new SolidColorBrush(Color.Parse("#338ab4f8")),
                segmentMarqueeRect
            );
            context.DrawRectangle(
                new Pen(new SolidColorBrush(Color.Parse("#8ab4f8")), 1),
                segmentMarqueeRect
            );
        }
        JsonArray tags = timeTags();
        using (context.PushClip(new Rect(0, 0, bounds.Width, HeaderHeight)))
        {
            for (int index = 0; index < tags.Count; index += 1)
            {
                if (tags[index] is not JsonObject timeTag)
                    continue;
                double actualX = AnimationEditor.number(timeTag["time"]) * PixelsPerSecond;
                double markerX = timeTagDisplayX(index);
                bool selected = index == SelectedTimeTag;
                SolidColorBrush markerBrush = new(Color.Parse(selected ? "#ffd166" : "#5ad1c4"));
                if (Math.Abs(markerX - actualX) > 0.01)
                    context.DrawLine(new Pen(markerBrush, 1), new Point(actualX, HeaderHeight - 1), new Point(markerX, HeaderHeight - 1));
                StreamGeometry marker = new();
                using (StreamGeometryContext geometry = marker.Open())
                {
                    geometry.BeginFigure(new Point(markerX, HeaderHeight), true);
                    geometry.LineTo(new Point(markerX - 6, HeaderHeight - 9));
                    geometry.LineTo(new Point(markerX + 6, HeaderHeight - 9));
                    geometry.EndFigure(true);
                }
                context.DrawGeometry(markerBrush, selected ? new Pen(Brushes.White, 1) : null, marker);
                string tag = timeTag["tag"]?.GetValue<string>() ?? string.Empty;
                FormattedText label = new(
                    tag,
                    CultureInfo.CurrentUICulture,
                    FlowDirection.LeftToRight,
                    Typeface.Default,
                    10,
                    markerBrush
                );
                context.DrawText(label, new Point(markerX + 7, 2));
            }
        }
        if (SelectedTimeTag >= 0 && GetTimeTag(SelectedTimeTag) is JsonObject selectedTimeTag)
        {
            double timeTagX = AnimationEditor.number(selectedTimeTag["time"]) * PixelsPerSecond;
            context.DrawLine(new Pen(new SolidColorBrush(Color.Parse("#66ffd166")), 1), new Point(timeTagX, HeaderHeight), new Point(timeTagX, bounds.Height));
        }
        double playhead = CurrentTime * PixelsPerSecond;
        SolidColorBrush playheadBrush = new(Color.Parse("#ff5c5c"));
        context.DrawLine(new Pen(playheadBrush, 1), new Point(playhead, 0), new Point(playhead, bounds.Height));
        StreamGeometry head = new();
        using (StreamGeometryContext geometry = head.Open())
        {
            geometry.BeginFigure(new Point(playhead, 18), true);
            geometry.LineTo(new Point(playhead - 6, 12));
            geometry.LineTo(new Point(playhead - 6, 0));
            geometry.LineTo(new Point(playhead + 6, 0));
            geometry.LineTo(new Point(playhead + 6, 12));
            geometry.EndFigure(true);
        }
        context.DrawGeometry(playheadBrush, null, head);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        Point position = e.GetPosition(this);
        if (e.GetCurrentPoint(this).Properties.IsRightButtonPressed)
        {
            int timeTag = hitTimeTag(position);
            if (timeTag >= 0)
            {
                Focus();
                SelectedTimeTag = timeTag;
                selectedSegments.Clear();
                updatePrimarySegmentSelection();
                SegmentSelected?.Invoke(-1, -1);
                showTimeTagContextMenu();
                InvalidateVisual();
                e.Handled = true;
            }
            return;
        }
        if (!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        Focus();
        int selectedTimeTag = hitTimeTag(position);
        if (selectedTimeTag >= 0)
        {
            SelectedTimeTag = selectedTimeTag;
            selectedSegments.Clear();
            updatePrimarySegmentSelection();
            SegmentSelected?.Invoke(-1, -1);
            if (e.ClickCount == 2)
            {
                TimeTagRenameRequested?.Invoke(selectedTimeTag);
                e.Handled = true;
                InvalidateVisual();
                return;
            }
            draggedTimeTag = GetTimeTag(selectedTimeTag);
            double tagTime = AnimationEditor.number(draggedTimeTag?["time"]);
            timeTagPointerOffset = position.X - tagTime * PixelsPerSecond;
            timeTagMoved = false;
            dragMode = TimeTagDragMode;
            dragStart = position;
            SetTime(tagTime);
            e.Pointer.Capture(this);
            InvalidateVisual();
            return;
        }
        SelectedTimeTag = -1;
        (int track, int segment, int mode) = hitTest(position);
        if (segment >= 0)
        {
            selectSegmentFromPointer(track, segment, e.KeyModifiers);
            if (selectedSegments.Count == 1 && selectedSegments.Contains((track, segment)))
            {
                dragMode = mode;
                dragStart = position;
                JsonObject current = segmentAt(track, segment)!;
                originalStart = AnimationEditor.number((current["startFrame"] as JsonObject)?["time"]);
                originalEnd = AnimationEditor.number((current["endFrame"] as JsonObject)?["time"]);
            }
            else
                dragMode = 0;
        }
        else if (position.Y < HeaderHeight)
        {
            selectedSegments.Clear();
            updatePrimarySegmentSelection();
            SegmentSelected?.Invoke(-1, -1);
            dragMode = ScrubDragMode;
            dragStart = position;
            SetTime(position.X / PixelsPerSecond);
        }
        else
        {
            beginSegmentMarquee(position, e.KeyModifiers);
        }
        e.Pointer.Capture(this);
        InvalidateVisual();
    }

    protected override void OnPointerMoved(PointerEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed is false)
        {
            if (dragMode == SegmentMarqueeDragMode)
            {
                completeSegmentMarquee(false);
                e.Pointer.Capture(null);
            }
            return;
        }
        if (dragMode == 0)
            return;
        if (dragMode == SegmentMarqueeDragMode)
        {
            Point position = e.GetPosition(this);
            if (!segmentMarqueeActive
                && Math.Abs(position.X - segmentMarqueeStart.X) < SegmentMarqueeThreshold
                && Math.Abs(position.Y - segmentMarqueeStart.Y) < SegmentMarqueeThreshold)
            {
                return;
            }
            segmentMarqueeActive = true;
            updateSegmentMarquee(position);
            return;
        }
        if (dragMode == TimeTagDragMode)
        {
            if (draggedTimeTag is not null)
            {
                double next = Math.Max(0, snap((e.GetPosition(this).X - timeTagPointerOffset) / PixelsPerSecond));
                double current = AnimationEditor.number(draggedTimeTag["time"]);
                if (Math.Abs(next - current) > 0.000001)
                {
                    draggedTimeTag["time"] = next;
                    timeTagMoved = true;
                    SetTime(next);
                }
            }
            return;
        }
        if (dragMode == ScrubDragMode)
        {
            SetTime(e.GetPosition(this).X / PixelsPerSecond);
            return;
        }
        if (segmentAt(SelectedTrack, SelectedSegment) is not JsonObject segment)
            return;
        double delta = (e.GetPosition(this).X - dragStart.X) / PixelsPerSecond;
        double start = originalStart;
        double end = originalEnd;
        double minimum = segment["type"]?.GetValue<string>() == "sound" ? 1.0 / frameRate() : 0.05;
        double maximum = segment["type"]?.GetValue<string>() == "sound"
            ? AnimationEditor.number(segment["originalDuration"], double.PositiveInfinity)
            : double.PositiveInfinity;
        if (dragMode == 1)
        {
            double duration = originalEnd - originalStart;
            double potentialStart = Math.Max(0, snap(originalStart + delta));
            (double left, double right) = findBounds(SelectedTrack, SelectedSegment);
            start = Math.Max(Math.Max(0, left), Math.Min(potentialStart, right - duration));
            end = start + duration;
        }
        else if (dragMode == 2)
        {
            (double left, _) = findBounds(SelectedTrack, SelectedSegment);
            start = Math.Max(Math.Max(left, end - maximum), Math.Min(snap(originalStart + delta), end - minimum));
        }
        else
        {
            (_, double right) = findBounds(SelectedTrack, SelectedSegment);
            end = Math.Min(Math.Min(right, start + maximum), Math.Max(start + minimum, snap(originalEnd + delta)));
        }
        (segment["startFrame"] as JsonObject)!["time"] = start;
        (segment["endFrame"] as JsonObject)!["time"] = end;
        InvalidateVisual();
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        if (dragMode == SegmentMarqueeDragMode)
        {
            if (segmentMarqueeActive)
                updateSegmentMarquee(e.GetPosition(this));
            completeSegmentMarquee(true);
        }
        else if (dragMode == TimeTagDragMode)
        {
            if (timeTagMoved && draggedTimeTag is not null)
            {
                SelectedTimeTag = sortTimeTags(draggedTimeTag);
                TimeTagChanged?.Invoke();
                Refresh();
            }
            draggedTimeTag = null;
            timeTagMoved = false;
        }
        else if (dragMode != 0 && dragMode != ScrubDragMode)
            SegmentChanged?.Invoke();
        dragMode = 0;
        e.Pointer.Capture(null);
    }

    private void beginSegmentMarquee(Point position, KeyModifiers modifiers)
    {
        segmentMarqueeStart = position;
        segmentMarqueeRect = new Rect(position, new Size());
        segmentMarqueeActive = false;
        segmentMarqueeAdditive = modifiers.HasFlag(KeyModifiers.Shift);
        segmentMarqueeToggle = !segmentMarqueeAdditive
            && EditorShortcuts.HasPrimaryModifier(modifiers);
        segmentSelectionBeforeMarquee.Clear();
        segmentSelectionBeforeMarquee.UnionWith(selectedSegments);
        SelectedTimeTag = -1;
        dragMode = SegmentMarqueeDragMode;
    }

    private void updateSegmentMarquee(Point position)
    {
        Rect trackBounds = new(
            0,
            HeaderHeight,
            Bounds.Width,
            Math.Max(0, Bounds.Height - HeaderHeight)
        );
        segmentMarqueeRect = AnimationEditor.createMarqueeRect(
            segmentMarqueeStart,
            position,
            trackBounds
        );
        List<(int Track, int Segment)> hits = [];
        JsonArray tracks = lines();
        for (int track = 0; track < tracks.Count; track += 1)
        {
            if (tracks[track] is not JsonObject line
                || line["timeSegments"] is not JsonArray segments)
            {
                continue;
            }
            for (int segment = 0; segment < segments.Count; segment += 1)
            {
                if (segments[segment] is JsonObject data
                    && AnimationEditor.rectsOverlap(segmentMarqueeRect, segmentRect(track, data)))
                {
                    hits.Add((track, segment));
                }
            }
        }
        HashSet<(int Track, int Segment)> selection = AnimationEditor.buildMarqueeSelection(
            segmentSelectionBeforeMarquee,
            hits,
            segmentMarqueeAdditive,
            segmentMarqueeToggle
        );
        selectedSegments.Clear();
        selectedSegments.UnionWith(selection);
        updatePrimarySegmentSelection();
        InvalidateVisual();
    }

    private void completeSegmentMarquee(bool clearOnBlank)
    {
        if (dragMode != SegmentMarqueeDragMode)
            return;
        if (!segmentMarqueeActive
            && clearOnBlank
            && !segmentMarqueeAdditive
            && !segmentMarqueeToggle)
        {
            selectedSegments.Clear();
        }
        segmentMarqueeActive = false;
        segmentMarqueeAdditive = false;
        segmentMarqueeToggle = false;
        segmentSelectionBeforeMarquee.Clear();
        segmentMarqueeRect = default;
        updatePrimarySegmentSelection();
        SegmentSelected?.Invoke(SelectedTrack, SelectedSegment);
        dragMode = 0;
        InvalidateVisual();
    }

    private void selectSegmentFromPointer(
        int track,
        int segment,
        KeyModifiers modifiers
    )
    {
        bool additive = modifiers.HasFlag(KeyModifiers.Shift);
        bool toggle = !additive && EditorShortcuts.HasPrimaryModifier(modifiers);
        (int Track, int Segment) selection = (track, segment);
        if (toggle)
        {
            if (!selectedSegments.Add(selection))
                selectedSegments.Remove(selection);
        }
        else if (additive)
            selectedSegments.Add(selection);
        else
        {
            selectedSegments.Clear();
            selectedSegments.Add(selection);
        }
        updatePrimarySegmentSelection();
        SegmentSelected?.Invoke(SelectedTrack, SelectedSegment);
    }

    private void updatePrimarySegmentSelection()
    {
        if (selectedSegments.Count == 1)
        {
            (int Track, int Segment) selection = selectedSegments.Single();
            SelectedTrack = selection.Track;
            SelectedSegment = selection.Segment;
            return;
        }
        SelectedTrack = -1;
        SelectedSegment = -1;
    }

    private HashSet<(int Track, int Segment)> validSelectedSegments()
    {
        HashSet<(int Track, int Segment)> selection = [];
        foreach ((int Track, int Segment) item in selectedSegments)
        {
            if (segmentAt(item.Track, item.Segment) is not null)
                selection.Add(item);
        }
        return selection;
    }

    private Rect segmentRect(int track, JsonObject segment)
    {
        double start = AnimationEditor.number(
            (segment["startFrame"] as JsonObject)?["time"]
        );
        double end = AnimationEditor.number(
            (segment["endFrame"] as JsonObject)?["time"]
        );
        return new Rect(
            start * PixelsPerSecond,
            HeaderHeight + track * TrackHeight + 5,
            Math.Max(3, (end - start) * PixelsPerSecond),
            TrackHeight - 10
        );
    }

    private void onPointerCaptureLost(object? sender, PointerCaptureLostEventArgs args)
    {
        if (dragMode == SegmentMarqueeDragMode)
            completeSegmentMarquee(false);
    }

    protected override void OnPointerWheelChanged(PointerWheelEventArgs e)
    {
        if (zoomInput.ShouldSuppressWheel())
        {
            e.Handled = true;
            return;
        }
        if (EditorZoomInput.ShouldZoomWheel(e.KeyModifiers, true)
            && e.Delta.Y != 0)
        {
            double nextZoom = zoom + (e.Delta.Y > 0 ? 0.1 : -0.1);
            setZoom(nextZoom, createZoomAnchor(e));
            e.Handled = true;
            return;
        }
        base.OnPointerWheelChanged(e);
    }

    protected override void OnAttachedToVisualTree(
        VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        LayoutUpdated += onLayoutUpdated;
        hostScrollViewer = this.FindAncestorOfType<ScrollViewer>();
    }

    protected override void OnDetachedFromVisualTree(
        VisualTreeAttachmentEventArgs args)
    {
        LayoutUpdated -= onLayoutUpdated;
        pendingZoomAnchor = null;
        hostScrollViewer = null;
        base.OnDetachedFromVisualTree(args);
    }

    private void onPointerTouchPadGestureMagnify(
        object? sender,
        PointerDeltaEventArgs args)
    {
        if (!EditorZoomInput.IsMacOS)
            return;
        zoomInput.MarkMagnify();
        double nextZoom = EditorZoomInput.ScaleByFactor(
            zoom,
            EditorZoomInput.GetMagnifyFactor(args.Delta.Y),
            0.2,
            5.0);
        setZoom(nextZoom, createZoomAnchor(args));
        args.Handled = true;
    }

    private TimelineZoomAnchor createZoomAnchor(PointerEventArgs args)
    {
        Point contentPoint = args.GetPosition(this);
        Point viewportPoint = hostScrollViewer is null
            ? contentPoint
            : args.GetPosition(hostScrollViewer);
        return new TimelineZoomAnchor(
            contentPoint.X / PixelsPerSecond,
            contentPoint.Y,
            viewportPoint);
    }

    private void setZoom(
        double value,
        TimelineZoomAnchor? anchor)
    {
        double nextZoom = Math.Clamp(value, 0.2, 5.0);
        if (Math.Abs(nextZoom - zoom) < 0.000001)
            return;
        zoom = nextZoom;
        pendingZoomAnchor = anchor;
        Refresh();
        ZoomChanged?.Invoke(zoom);
    }

    private void onLayoutUpdated(object? sender, EventArgs args)
    {
        if (pendingZoomAnchor is null)
            return;
        applyZoomAnchor();
    }

    private void applyZoomAnchor()
    {
        if (pendingZoomAnchor is not TimelineZoomAnchor anchor
            || hostScrollViewer is null)
        {
            pendingZoomAnchor = null;
            return;
        }
        pendingZoomAnchor = null;
        Point contentAnchor = new(
            anchor.Time * PixelsPerSecond,
            anchor.ContentY);
        hostScrollViewer.Offset = EditorZoomInput.GetAnchoredOffset(
            contentAnchor,
            anchor.ViewportPoint,
            hostScrollViewer.Extent,
            hostScrollViewer.Viewport);
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.Key == Key.F2 && SelectedTimeTag >= 0)
        {
            TimeTagRenameRequested?.Invoke(SelectedTimeTag);
            e.Handled = true;
            return;
        }
        if (e.Key is Key.Delete or Key.Back)
        {
            if (SelectedTimeTag >= 0)
                DeleteSelectedTimeTag();
            else
                DeleteSelectedSegment();
            e.Handled = true;
            return;
        }
        base.OnKeyDown(e);
    }

    private void onDragOver(object? sender, DragEventArgs e)
    {
        string? payload = e.DataTransfer.TryGetText();
        Point position = e.GetPosition(this);
        int track = trackAt(position.Y);
        e.DragEffects = payload is not null && tryBuildAssetSegments(payload, track, position.X / PixelsPerSecond, out _)
            ? DragDropEffects.Copy : DragDropEffects.None;
        e.Handled = true;
    }

    private void onDrop(object? sender, DragEventArgs e)
    {
        if (e.DataTransfer.TryGetText() is not string payload)
            return;
        Point position = e.GetPosition(this);
        int track = trackAt(position.Y);
        if (tryBuildAssetSegments(payload, track, position.X / PixelsPerSecond, out List<JsonObject> segments))
            insertAssetSegments(track, segments);
        e.Handled = true;
    }

    public void SetTime(double time, bool snapToFrame = true)
    {
        CurrentTime = Math.Max(0, snapToFrame ? snap(time) : time);
        TimeChanged?.Invoke(CurrentTime);
        InvalidateVisual();
    }

    private (int track, int segment, int mode) hitTest(Point position)
    {
        int track = trackAt(position.Y);
        if (track < 0 || track >= lines().Count || lines()[track] is not JsonObject line || line["timeSegments"] is not JsonArray segments)
            return (-1, -1, 0);
        double time = position.X / PixelsPerSecond;
        double handleWidth = 5.0 / PixelsPerSecond;
        for (int index = 0; index < segments.Count; index += 1)
        {
            if (segments[index] is not JsonObject segment)
                continue;
            double start = AnimationEditor.number((segment["startFrame"] as JsonObject)?["time"]);
            double end = AnimationEditor.number((segment["endFrame"] as JsonObject)?["time"]);
            if (time < start - handleWidth || time > end + handleWidth)
                continue;
            int mode = Math.Abs(time - start) <= handleWidth ? 2 : Math.Abs(time - end) <= handleWidth ? 3 : 1;
            return (track, index, mode);
        }
        return (-1, -1, 0);
    }

    private int hitTimeTag(Point position)
    {
        if (position.Y < 0 || position.Y >= HeaderHeight)
            return -1;
        JsonArray tags = timeTags();
        int result = -1;
        double distance = 8;
        for (int index = 0; index < tags.Count; index += 1)
        {
            if (tags[index] is not JsonObject)
                continue;
            double currentDistance = Math.Abs(position.X - timeTagDisplayX(index));
            if (currentDistance <= distance)
            {
                result = index;
                distance = currentDistance;
            }
        }
        return result;
    }

    private void showTimeTagContextMenu()
    {
        MenuItem rename = new() { Header = LocaleService.Get("RENAME_TIME_TAG") };
        rename.Click += (_, _) =>
        {
            if (SelectedTimeTag >= 0)
                TimeTagRenameRequested?.Invoke(SelectedTimeTag);
        };
        MenuItem delete = new() { Header = LocaleService.Get("DELETE") };
        delete.Click += (_, _) => DeleteSelectedTimeTag();
        ContextMenu menu = new() { ItemsSource = new object[] { rename, delete } };
        menu.Open(this);
    }

    private bool overlaps(int track, double start, double end, int ignored)
    {
        return overlaps(lines(), track, start, end, ignored);
    }

    private bool overlaps(JsonArray tracks, int track, double start, double end, int ignored)
    {
        if (track < 0 || track >= tracks.Count || tracks[track] is not JsonObject line || line["timeSegments"] is not JsonArray segments)
            return false;
        for (int index = 0; index < segments.Count; index += 1)
        {
            if (index == ignored || segments[index] is not JsonObject segment)
                continue;
            double currentStart = AnimationEditor.number((segment["startFrame"] as JsonObject)?["time"]);
            double currentEnd = AnimationEditor.number((segment["endFrame"] as JsonObject)?["time"]);
            if (start < currentEnd && end > currentStart)
                return true;
        }
        return false;
    }

    private (double Left, double Right) findBounds(int track, int ignored)
    {
        double left = 0;
        double right = double.PositiveInfinity;
        if (track < 0 || track >= lines().Count || lines()[track] is not JsonObject line || line["timeSegments"] is not JsonArray segments)
            return (left, right);
        foreach ((JsonNode? node, int index) in segments.Select((node, index) => (node, index)))
        {
            if (index == ignored || node is not JsonObject segment)
                continue;
            double start = AnimationEditor.number((segment["startFrame"] as JsonObject)?["time"]);
            double end = AnimationEditor.number((segment["endFrame"] as JsonObject)?["time"]);
            if (end <= originalStart + 0.0001)
                left = Math.Max(left, end);
            if (start >= originalEnd - 0.0001)
                right = Math.Min(right, start);
        }
        return (left, right);
    }

    private double clampInsertStart(int track, int ignored, double start, double duration)
    {
        double oldStart = originalStart;
        double oldEnd = originalEnd;
        originalStart = start;
        originalEnd = start + duration;
        (double left, double right) = findBounds(track, ignored);
        originalStart = oldStart;
        originalEnd = oldEnd;
        return Math.Max(left, Math.Min(start, right - duration));
    }

    private int trackAt(double y) => y < HeaderHeight ? -1 : (int)((y - HeaderHeight) / TrackHeight);

    private double minimumDuration(JsonObject segment) => segment["type"]?.GetValue<string>() == "sound" ? 1.0 / frameRate() : 0.05;
    private double defaultDuration(string asset)
    {
        if (!AnimationEditor.isAudioAsset(asset))
            return 0.05;
        double duration = GameAssetPath.TryResolveExistingFile(projectPath, asset, out string path)
            ? AnimationAudioPlayback.GetDuration(path) ?? 0.1
            : 0.1;
        return duration > 0 ? duration : 0.1;
    }

    private bool tryGetExactAudioDuration(string asset, out double duration)
    {
        duration = GameAssetPath.TryResolveExistingFile(projectPath, asset, out string path)
            ? AnimationAudioPlayback.GetDuration(path) ?? 0
            : 0;
        return duration > 0;
    }

    private bool tryBuildAssetSegments(string payload, int track, double dropTime, out List<JsonObject> segments)
    {
        segments = [];
        if (track < 0 || !tryResolveAssetIndexes(payload, out List<int> assetIndexes))
            return false;
        JsonArray assets = getData()["assets"] as JsonArray ?? new JsonArray();
        JsonArray existingTracks = getData()["timeLines"] as JsonArray ?? new JsonArray();
        bool batch = assetIndexes.Count > 1;
        double start = Math.Max(0, batch ? Math.Round(dropTime * 10.0) / 10.0 : snap(dropTime));
        double nextStart = start;
        foreach (int assetIndex in assetIndexes)
        {
            if (assetIndex < 0 || assetIndex >= assets.Count || assets[assetIndex] is not JsonValue assetValue
                || !assetValue.TryGetValue<string>(out string? asset) || string.IsNullOrWhiteSpace(asset))
            {
                segments.Clear();
                return false;
            }
            bool sound = AnimationEditor.isAudioAsset(asset);
            double duration;
            if (sound && batch)
            {
                if (!tryGetExactAudioDuration(asset, out duration))
                {
                    segments.Clear();
                    return false;
                }
            }
            else
                duration = sound ? defaultDuration(asset) : 0.05;
            double originalAudioDuration = duration;
            if (sound && !batch && track < existingTracks.Count && existingTracks[track] is JsonObject trackData
                && trackData["timeSegments"] is JsonArray existing)
            {
                foreach (JsonNode? node in existing)
                {
                    if (node is not JsonObject other)
                        continue;
                    double otherStart = AnimationEditor.number((other["startFrame"] as JsonObject)?["time"]);
                    if (otherStart > nextStart)
                        duration = Math.Min(duration, otherStart - nextStart);
                }
                originalAudioDuration = duration;
            }
            JsonObject segment = new()
            {
                ["type"] = sound ? "sound" : "frame",
                ["asset"] = assetIndex,
                ["startFrame"] = createFrame(nextStart),
                ["endFrame"] = createFrame(nextStart + duration),
            };
            if (sound)
                segment["originalDuration"] = originalAudioDuration;
            else
                segment["flipX"] = false;
            segments.Add(segment);
            nextStart += duration;
        }
        if (segments.Count == 0 || overlaps(existingTracks, track, start, nextStart, -1))
        {
            segments.Clear();
            return false;
        }
        return true;
    }

    private bool tryResolveAssetIndexes(string payload, out List<int> assetIndexes)
    {
        assetIndexes = [];
        JsonArray assets = getData()["assets"] as JsonArray ?? new JsonArray();
        if (!payload.StartsWith(AssetDragPrefix, StringComparison.Ordinal))
        {
            for (int index = 0; index < assets.Count; index += 1)
            {
                if (string.Equals(assets[index]?.GetValue<string>(), payload, StringComparison.Ordinal))
                {
                    assetIndexes.Add(index);
                    return true;
                }
            }
            return false;
        }
        string json = payload[AssetDragPrefix.Length..].Trim();
        if (json.Length < 2 || json[0] != '[' || json[^1] != ']')
            return false;
        string values = json[1..^1].Trim();
        if (values.Length == 0)
            return false;
        HashSet<int> unique = [];
        foreach (string value in values.Split(','))
        {
            if (!int.TryParse(value.Trim(), NumberStyles.Integer, CultureInfo.InvariantCulture, out int index)
                || index < 0 || index >= assets.Count)
            {
                assetIndexes.Clear();
                return false;
            }
            unique.Add(index);
        }
        assetIndexes.AddRange(unique.Order());
        return assetIndexes.Count > 0;
    }

    private void insertAssetSegments(int track, List<JsonObject> segments)
    {
        JsonArray tracks = lines();
        while (tracks.Count <= track)
            tracks.Add(new JsonObject { ["timeSegments"] = new JsonArray() });
        JsonObject trackData = (JsonObject)tracks[track]!;
        if (trackData["timeSegments"] is not JsonArray target)
        {
            target = new JsonArray();
            trackData["timeSegments"] = target;
        }
        foreach (JsonObject segment in segments)
            target.Add(segment);
        SetSegmentSelection(track, target.Count - 1);
        SelectedTimeTag = -1;
        SegmentSelected?.Invoke(SelectedTrack, SelectedSegment);
        SegmentChanged?.Invoke();
        Refresh();
    }

    private static JsonObject createFrame(double time) => new()
    {
        ["time"] = time,
        ["position"] = new JsonArray(0.0, 0.0),
        ["rotation"] = 0.0,
        ["scale"] = new JsonArray(1.0, 1.0),
    };

    private void updateCanvasSize()
    {
        double contentEnd = 5.0;
        int trackCount = lines().Count;
        foreach (JsonNode? lineNode in lines())
        {
            if (lineNode is not JsonObject line || line["timeSegments"] is not JsonArray segments)
                continue;
            foreach (JsonNode? segmentNode in segments)
                if (segmentNode is JsonObject segment)
                    contentEnd = Math.Max(contentEnd, AnimationEditor.number((segment["endFrame"] as JsonObject)?["time"]));
        }
        foreach (JsonNode? timeTagNode in timeTags())
        {
            if (timeTagNode is JsonObject timeTag)
                contentEnd = Math.Max(contentEnd, AnimationEditor.number(timeTag["time"]));
        }
        Width = (contentEnd + 1) * PixelsPerSecond;
        Height = HeaderHeight + (Math.Max(5, trackCount) + 1) * TrackHeight;
    }

    private JsonObject? segmentAt(int track, int segment)
    {
        return track >= 0 && track < lines().Count && lines()[track] is JsonObject line && line["timeSegments"] is JsonArray segments
            && segment >= 0 && segment < segments.Count ? segments[segment] as JsonObject : null;
    }

    private JsonArray lines()
    {
        JsonObject data = getData();
        if (data["timeLines"] is JsonArray lines)
            return lines;
        lines = new JsonArray();
        data["timeLines"] = lines;
        return lines;
    }

    private JsonArray timeTags()
    {
        JsonObject data = getData();
        if (data["timeTags"] is JsonArray tags)
            return tags;
        tags = new JsonArray();
        data["timeTags"] = tags;
        return tags;
    }

    private int sortTimeTags(JsonObject? selected)
    {
        JsonArray tags = timeTags();
        List<(JsonNode? Node, double Time, int Order)> ordered = tags
            .Select((node, index) => (
                Node: node,
                Time: node is JsonObject timeTag ? AnimationEditor.number(timeTag["time"]) : double.PositiveInfinity,
                Order: index
            ))
            .OrderBy(item => item.Time)
            .ThenBy(item => item.Order)
            .ToList();
        tags.Clear();
        int selectedIndex = -1;
        for (int index = 0; index < ordered.Count; index += 1)
        {
            JsonNode? node = ordered[index].Node;
            tags.Add(node);
            if (ReferenceEquals(node, selected))
                selectedIndex = index;
        }
        return selectedIndex;
    }

    private double timeTagDisplayX(int index)
    {
        if (GetTimeTag(index) is not JsonObject timeTag)
            return double.NegativeInfinity;
        double time = AnimationEditor.number(timeTag["time"]);
        int duplicate = 0;
        for (int previous = 0; previous < index; previous += 1)
        {
            if (GetTimeTag(previous) is JsonObject previousTag
                && Math.Abs(AnimationEditor.number(previousTag["time"]) - time) < 0.000001)
            {
                duplicate += 1;
            }
        }
        return time * PixelsPerSecond + duplicate * 14;
    }

    private int frameRate() => Math.Max(1, (int)AnimationEditor.number(getData()["frameRate"], 30));
    private double snap(double time) => Math.Round(time * frameRate()) / frameRate();
    private readonly record struct TimelineZoomAnchor(
        double Time,
        double ContentY,
        Point ViewportPoint);
}
