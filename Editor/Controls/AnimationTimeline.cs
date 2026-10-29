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

public sealed class AnimationTimeline : Panel
{
    internal const string AssetDragPrefix = "ludork-animation-assets:";
    private const double SegmentMarqueeThreshold = 4;
    private readonly ProjectAssetPaths assetPaths;
    private readonly Func<JsonObject> getData;
    private readonly AnimationTimelineIndex index;
    private readonly TimelineDrawingLayer contentLayer;
    private readonly TimelineDrawingLayer selectionLayer;
    private readonly TimelinePlayhead playhead;
    private readonly AnimationTimeTagCache tagCache = new();
    private readonly List<AnimationTimelineSegment> visibleSegments = [];
    private readonly List<AnimationTimeTagLayout> visibleTags = [];
    private readonly Dictionary<string, double?> dragAudioDurations = new(StringComparer.Ordinal);
    private readonly SolidColorBrush frameBrush = new(Color.Parse("#4f89b8"));
    private readonly SolidColorBrush soundBrush = new(Color.Parse("#76573b"));
    private readonly SolidColorBrush selectedBrush = new(Color.Parse("#8ab4f8"));
    private readonly SolidColorBrush tagBrush = new(Color.Parse("#5ad1c4"));
    private readonly SolidColorBrush selectedTagBrush = new(Color.Parse("#ffd166"));
    private readonly Pen segmentPen = new(new SolidColorBrush(Color.Parse("#a9cce8")), 1);
    private readonly Pen selectedSegmentPen = new(Brushes.White, 2);
    private readonly Pen selectedTagPen = new(Brushes.White, 1);
    private readonly Pen selectedTagLinePen = new(new SolidColorBrush(Color.Parse("#66ffd166")), 1);
    private readonly SolidColorBrush marqueeBrush = new(Color.Parse("#338ab4f8"));
    private readonly Pen marqueePen = new(new SolidColorBrush(Color.Parse("#8ab4f8")), 1);
    private readonly StreamGeometry tagGeometry = createTagGeometry();
    private Rect viewport;
    private string? dragAudioPayload;
    private double gestureLeft;
    private double gestureRight;
    private bool segmentMoved;
    private bool tagsDirty = true;
    private readonly Pen tagConnectorPen = new(new SolidColorBrush(Color.Parse("#5ad1c4")), 1);
    private readonly Pen selectedTagConnectorPen = new(new SolidColorBrush(Color.Parse("#ffd166")), 1);
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

    internal AnimationTimeline(ProjectDataStore gameData, Func<JsonObject> getData, AnimationTimelineIndex index)
    {
        assetPaths = gameData.AssetPaths;
        this.getData = getData;
        this.index = index;
        contentLayer = new TimelineDrawingLayer(drawContent) { IsHitTestVisible = false };
        selectionLayer = new TimelineDrawingLayer(drawSelection) { IsHitTestVisible = false };
        playhead = new TimelinePlayhead(new SolidColorBrush(Color.Parse("#ff5c5c")), 1, true);
        playhead.SetPosition(0);
        Children.Add(contentLayer);
        Children.Add(selectionLayer);
        Children.Add(playhead);
        Background = Brushes.Transparent;
        EffectiveViewportChanged += (_, args) =>
        {
            viewport = args.EffectiveViewport;
            invalidateContent();
        };
        SizeChanged += (_, _) => invalidateContent();
        ActualThemeVariantChanged += onTextStyleChanged;
        Focusable = true;
        DragDrop.SetAllowDrop(this, true);
        AddHandler(DragDrop.DragOverEvent, onDragOver);
        AddHandler(DragDrop.DropEvent, onDrop);
        AddHandler(DragDrop.DragLeaveEvent, (_, _) => clearDragAudioDurations());
        AddHandler(
            PointerCaptureLostEvent,
            onPointerCaptureLost,
            RoutingStrategies.Tunnel
        );
        PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
        ContextRequested += onContextRequested;
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

    public void Refresh(bool resetTags = false)
    {
        if (resetTags || tagsDirty)
        {
            JsonObject? selectedTimeTag = GetTimeTag(SelectedTimeTag);
            SelectedTimeTag = sortTimeTags(selectedTimeTag);
            tagCache.Reset(timeTags(), PixelsPerSecond);
            tagsDirty = false;
        }
        selectedSegments.IntersectWith(validSelectedSegments());
        updatePrimarySegmentSelection();
        updateCanvasSize();
        invalidateContent();
    }

    public void RefreshSelection() => invalidateContent();

    public void SetZoom(double value)
    {
        setZoom(value, null);
    }

    public void AdjustZoom(double delta) => SetZoom(zoom + delta);

    public void ClearTimeTagSelection()
    {
        SelectedTimeTag = -1;
        invalidateContent();
    }

    public void SetSegmentSelection(int track, int segment)
    {
        selectedSegments.Clear();
        if (segmentAt(track, segment) is not null)
            selectedSegments.Add((track, segment));
        updatePrimarySegmentSelection();
        invalidateContent();
    }

    public void AddTimeTag(string tag)
    {
        JsonObject timeTag = new()
        {
            ["tag"] = tag,
            ["time"] = CurrentTime,
        };
        timeTags(true).Add(timeTag);
        SelectedTimeTag = sortTimeTags(timeTag);
        tagsDirty = true;
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
        tagsDirty = true;
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
        tagsDirty = true;
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
            index.RefreshTrack(getData(), group.Key);
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
        JsonArray tracks = lines(true);
        while (tracks.Count <= track)
            tracks.Add(new JsonObject { ["timeSegments"] = new JsonArray() });
        JsonArray segments = ((JsonObject)tracks[track]!)["timeSegments"]!.AsArray();
        JsonObject startFrame = segment["startFrame"] as JsonObject ?? new JsonObject();
        JsonObject endFrame = segment["endFrame"] as JsonObject ?? new JsonObject();
        double duration = Math.Max(minimumDuration(segment), JsonScalar.Number(endFrame["time"]) - JsonScalar.Number(startFrame["time"]));
        double start = clampInsertStart(track, -1, Math.Max(0, snap(startTime)), duration);
        if (overlaps(track, start, start + duration, -1))
            return false;
        startFrame["time"] = start;
        endFrame["time"] = start + duration;
        segment["startFrame"] = startFrame;
        segment["endFrame"] = endFrame;
        segments.Add(segment);
        index.RefreshTrack(getData(), track);
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
        double end = JsonScalar.Number((source["endFrame"] as JsonObject)?["time"]);
        return InsertSegmentAt(SelectedTrack, (JsonObject)source.DeepClone(), end);
    }

    public bool NudgeSelectedSegment(int frameDelta)
    {
        if (selectedSegments.Count != 1
            || segmentAt(SelectedTrack, SelectedSegment) is not JsonObject segment)
            return false;
        JsonObject startFrame = segment["startFrame"] as JsonObject ?? new JsonObject();
        JsonObject endFrame = segment["endFrame"] as JsonObject ?? new JsonObject();
        double start = JsonScalar.Number(startFrame["time"]);
        double end = JsonScalar.Number(endFrame["time"]);
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
        index.Update(SelectedTrack, SelectedSegment);
        SegmentChanged?.Invoke();
        Refresh();
        return true;
    }

    private void drawContent(DrawingContext context)
    {
        Rect bounds = new(Bounds.Size);
        Rect visible = bounds.Intersect(viewport);
        if (visible.Width <= 0 || visible.Height <= 0)
            return;
        using IDisposable clip = context.PushClip(visible);
        context.FillRectangle(EditorTheme.Brush("Background"), visible);
        Rect header = new Rect(0, 0, bounds.Width, HeaderHeight).Intersect(visible);
        if (header.Height > 0)
            context.FillRectangle(EditorTheme.Brush("Surface"), header);
        Pen gridPen = new(EditorTheme.Brush("Border"), 1);
        double firstSecond = Math.Max(0, Math.Floor(visible.Left / PixelsPerSecond));
        for (double second = firstSecond; second * PixelsPerSecond < visible.Right; second++)
            context.DrawLine(gridPen, new Point(second * PixelsPerSecond, visible.Top), new Point(second * PixelsPerSecond, visible.Bottom));
        int firstTrack = Math.Max(0, (int)Math.Floor((visible.Top - HeaderHeight) / TrackHeight));
        int lastTrack = Math.Min(Math.Max(5, index.TrackCount) - 1, (int)Math.Floor((visible.Bottom - HeaderHeight) / TrackHeight));
        for (int track = firstTrack; track <= lastTrack; track++)
        {
            double y = HeaderHeight + track * TrackHeight;
            context.FillRectangle(EditorTheme.Brush(track % 2 == 0 ? "Surface" : "Background"), new Rect(visible.Left, y, visible.Width, TrackHeight));
            index.QueryTrack(track, (visible.Left - 4) / PixelsPerSecond, (visible.Right + 1) / PixelsPerSecond, visibleSegments);
            foreach (AnimationTimelineSegment segment in visibleSegments)
            {
                Rect rect = segmentRect(track, segment.Data);
                bool selected = selectedSegments.Contains((track, segment.Index));
                context.FillRectangle(selected ? selectedBrush : segment.IsSound ? soundBrush : frameBrush, rect, 4);
                context.DrawRectangle(selected ? selectedSegmentPen : segmentPen, rect, 4);
            }
        }
        if (header.Height <= 0)
            return;
        using IDisposable headerClip = context.PushClip(header);
        tagCache.Query(visible.Left - 1, visible.Right + 1, visibleTags);
        foreach (AnimationTimeTagLayout tag in visibleTags)
        {
            bool selected = tag.Index == SelectedTimeTag;
            IBrush brush = selected ? selectedTagBrush : tagBrush;
            if (Math.Abs(tag.MarkerX - tag.ActualX) > 0.01)
                context.DrawLine(selected ? selectedTagConnectorPen : tagConnectorPen, new Point(tag.ActualX, HeaderHeight - 1), new Point(tag.MarkerX, HeaderHeight - 1));
            using (context.PushTransform(Matrix.CreateTranslation(tag.MarkerX, 0)))
                context.DrawGeometry(brush, selected ? selectedTagPen : null, tagGeometry);
            context.DrawText(selected ? tag.SelectedLabel : tag.Label, new Point(tag.MarkerX + 7, 2));
        }
    }

    private void drawSelection(DrawingContext context)
    {
        Rect visible = new Rect(Bounds.Size).Intersect(viewport);
        if (visible.Width <= 0 || visible.Height <= 0)
            return;
        using IDisposable clip = context.PushClip(visible);
        if (segmentMarqueeActive)
        {
            context.FillRectangle(marqueeBrush, segmentMarqueeRect);
            context.DrawRectangle(marqueePen, segmentMarqueeRect);
        }
        if (SelectedTimeTag >= 0 && GetTimeTag(SelectedTimeTag) is JsonObject selectedTimeTag)
        {
            double x = JsonScalar.Number(selectedTimeTag["time"]) * PixelsPerSecond;
            context.DrawLine(selectedTagLinePen, new Point(x, HeaderHeight), new Point(x, Bounds.Height));
        }
    }

    private void invalidateContent()
    {
        contentLayer.InvalidateVisual();
        selectionLayer.InvalidateVisual();
    }

    private static StreamGeometry createTagGeometry()
    {
        StreamGeometry geometry = new();
        using StreamGeometryContext path = geometry.Open();
        path.BeginFigure(new Point(0, HeaderHeight), true);
        path.LineTo(new Point(-6, HeaderHeight - 9));
        path.LineTo(new Point(6, HeaderHeight - 9));
        path.EndFigure(true);
        return geometry;
    }

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        Point position = e.GetPosition(this);
        PointerPointProperties properties = e.GetCurrentPoint(this).Properties;
        if (!properties.IsLeftButtonPressed || properties.IsRightButtonPressed)
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
                invalidateContent();
                return;
            }
            draggedTimeTag = GetTimeTag(selectedTimeTag);
            double tagTime = JsonScalar.Number(draggedTimeTag?["time"]);
            timeTagPointerOffset = position.X - tagTime * PixelsPerSecond;
            timeTagMoved = false;
            dragMode = TimeTagDragMode;
            dragStart = position;
            SetTime(tagTime);
            e.Pointer.Capture(this);
            invalidateContent();
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
                originalStart = JsonScalar.Number((current["startFrame"] as JsonObject)?["time"]);
                originalEnd = JsonScalar.Number((current["endFrame"] as JsonObject)?["time"]);
                (gestureLeft, gestureRight) = findBounds(SelectedTrack, SelectedSegment);
                segmentMoved = false;
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
        invalidateContent();
    }

    private void onContextRequested(object? sender, ContextRequestedEventArgs args)
    {
        int timeTag = args.TryGetPosition(this, out Point position) ? hitTimeTag(position) : SelectedTimeTag;
        if (timeTag < 0)
            return;
        Focus();
        SelectedTimeTag = timeTag;
        selectedSegments.Clear();
        updatePrimarySegmentSelection();
        SegmentSelected?.Invoke(-1, -1);
        showTimeTagContextMenu();
        invalidateContent();
        args.Handled = true;
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
                double current = JsonScalar.Number(draggedTimeTag["time"]);
                if (Math.Abs(next - current) > 0.000001)
                {
                    draggedTimeTag["time"] = next;
                    timeTagMoved = true;
                    tagCache.UpdateTime(SelectedTimeTag);
                    invalidateContent();
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
            ? JsonScalar.Number(segment["originalDuration"], double.PositiveInfinity)
            : double.PositiveInfinity;
        if (dragMode == 1)
        {
            double duration = originalEnd - originalStart;
            double potentialStart = Math.Max(0, snap(originalStart + delta));
            start = Math.Max(Math.Max(0, gestureLeft), Math.Min(potentialStart, gestureRight - duration));
            end = start + duration;
        }
        else if (dragMode == 2)
        {
            start = Math.Max(Math.Max(gestureLeft, end - maximum), Math.Min(snap(originalStart + delta), end - minimum));
        }
        else
        {
            end = Math.Min(Math.Min(gestureRight, start + maximum), Math.Max(start + minimum, snap(originalEnd + delta)));
        }
        if (JsonScalar.Number((segment["startFrame"] as JsonObject)?["time"]) == start
            && JsonScalar.Number((segment["endFrame"] as JsonObject)?["time"]) == end)
            return;
        (segment["startFrame"] as JsonObject)!["time"] = start;
        (segment["endFrame"] as JsonObject)!["time"] = end;
        index.Update(SelectedTrack, SelectedSegment);
        segmentMoved = true;
        invalidateContent();
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        completePointerGesture(true, e.GetPosition(this));
        e.Pointer.Capture(null);
    }

    private void completePointerGesture(bool completeMarquee, Point position)
    {
        if (dragMode == SegmentMarqueeDragMode)
        {
            if (completeMarquee && segmentMarqueeActive)
                updateSegmentMarquee(position);
            completeSegmentMarquee(completeMarquee);
        }
        else if (dragMode == TimeTagDragMode)
        {
            if (timeTagMoved && draggedTimeTag is not null)
            {
                SelectedTimeTag = sortTimeTags(draggedTimeTag);
                tagsDirty = true;
                TimeTagChanged?.Invoke();
                Refresh();
            }
            draggedTimeTag = null;
            timeTagMoved = false;
        }
        else if (dragMode != 0 && dragMode != ScrubDragMode && segmentMoved)
            SegmentChanged?.Invoke();
        segmentMoved = false;
        dragMode = 0;
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
        segmentMarqueeRect = MarqueeSelection.CreateRect(
            segmentMarqueeStart,
            position,
            trackBounds
        );
        List<(int Track, int Segment)> hits = [];
        int firstTrack = Math.Max(0, trackAt(segmentMarqueeRect.Top));
        int lastTrack = Math.Min(index.TrackCount - 1, trackAt(segmentMarqueeRect.Bottom));
        for (int track = firstTrack; track <= lastTrack; track++)
        {
            index.QueryTrack(track, (segmentMarqueeRect.Left - 3) / PixelsPerSecond,
                segmentMarqueeRect.Right / PixelsPerSecond, visibleSegments);
            foreach (AnimationTimelineSegment segment in visibleSegments)
            {
                if (MarqueeSelection.Overlaps(segmentMarqueeRect, segmentRect(track, segment.Data)))
                    hits.Add((track, segment.Index));
            }
        }
        HashSet<(int Track, int Segment)> selection = MarqueeSelection.Build(
            segmentSelectionBeforeMarquee,
            hits,
            segmentMarqueeAdditive,
            segmentMarqueeToggle
        );
        selectedSegments.Clear();
        selectedSegments.UnionWith(selection);
        updatePrimarySegmentSelection();
        invalidateContent();
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
        invalidateContent();
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
        double start = JsonScalar.Number(
            (segment["startFrame"] as JsonObject)?["time"]
        );
        double end = JsonScalar.Number(
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
        completePointerGesture(false, default);
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
        LocaleService.LanguageChanged += onTextStyleChanged;
        hostScrollViewer = this.FindAncestorOfType<ScrollViewer>();
    }

    protected override void OnDetachedFromVisualTree(
        VisualTreeAttachmentEventArgs args)
    {
        LayoutUpdated -= onLayoutUpdated;
        LocaleService.LanguageChanged -= onTextStyleChanged;
        pendingZoomAnchor = null;
        hostScrollViewer = null;
        clearDragAudioDurations();
        base.OnDetachedFromVisualTree(args);
    }

    private void onTextStyleChanged(object? sender, EventArgs args)
    {
        tagCache.RefreshText();
        invalidateContent();
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
        tagCache.SetScale(PixelsPerSecond);
        updateCanvasSize();
        playhead.SetPosition(CurrentTime * PixelsPerSecond);
        invalidateContent();
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
        if (!string.Equals(payload, dragAudioPayload, StringComparison.Ordinal))
        {
            clearDragAudioDurations();
            dragAudioPayload = payload;
        }
        Point position = e.GetPosition(this);
        int track = trackAt(position.Y);
        e.DragEffects = payload is not null && tryBuildAssetSegments(payload, track, position.X / PixelsPerSecond, out _)
            ? DragDropEffects.Copy : DragDropEffects.None;
        e.Handled = true;
    }

    private void onDrop(object? sender, DragEventArgs e)
    {
        if (e.DataTransfer.TryGetText() is not string payload)
        {
            clearDragAudioDurations();
            return;
        }
        clearDragAudioDurations();
        Point position = e.GetPosition(this);
        int track = trackAt(position.Y);
        if (tryBuildAssetSegments(payload, track, position.X / PixelsPerSecond, out List<JsonObject> segments))
            insertAssetSegments(track, segments);
        clearDragAudioDurations();
        e.Handled = true;
    }

    public void SetTime(double time, bool snapToFrame = true)
    {
        CurrentTime = Math.Max(0, snapToFrame ? snap(time) : time);
        playhead.SetPosition(CurrentTime * PixelsPerSecond);
        TimeChanged?.Invoke(CurrentTime);
    }

    private (int track, int segment, int mode) hitTest(Point position)
    {
        int track = trackAt(position.Y);
        double time = position.X / PixelsPerSecond;
        double handleWidth = 5.0 / PixelsPerSecond;
        index.QueryTrack(track, time - handleWidth, time + handleWidth, visibleSegments);
        foreach (AnimationTimelineSegment segment in visibleSegments)
        {
            double start = segment.Start;
            double end = segment.End;
            if (time < start - handleWidth || time > end + handleWidth)
                continue;
            int mode = Math.Abs(time - start) <= handleWidth ? 2 : Math.Abs(time - end) <= handleWidth ? 3 : 1;
            return (track, segment.Index, mode);
        }
        return (-1, -1, 0);
    }

    private int hitTimeTag(Point position)
    {
        if (position.Y < 0 || position.Y >= HeaderHeight)
            return -1;
        int result = -1;
        double distance = 8;
        tagCache.Query(position.X - distance, position.X + distance, visibleTags);
        foreach (AnimationTimeTagLayout tag in visibleTags)
        {
            double currentDistance = Math.Abs(position.X - tag.MarkerX);
            if (currentDistance <= distance)
            {
                result = tag.Index;
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
        index.QueryTrack(track, start, end, visibleSegments);
        foreach (AnimationTimelineSegment segment in visibleSegments)
        {
            if (segment.Index != ignored && start < segment.End && end > segment.Start)
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
            double start = JsonScalar.Number((segment["startFrame"] as JsonObject)?["time"]);
            double end = JsonScalar.Number((segment["endFrame"] as JsonObject)?["time"]);
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
        double duration = audioDuration(asset) ?? 0.1;
        return duration > 0 ? duration : 0.1;
    }

    private bool tryGetExactAudioDuration(string asset, out double duration)
    {
        duration = audioDuration(asset) ?? 0;
        return duration > 0;
    }

    private double? audioDuration(string asset)
    {
        if (!assetPaths.TryResolveAsset(asset, out string path))
            return null;
        if (!dragAudioDurations.TryGetValue(path, out double? duration))
        {
            duration = AnimationAudioPlayback.GetDuration(path);
            dragAudioDurations[path] = duration;
        }
        return duration;
    }

    private void clearDragAudioDurations()
    {
        dragAudioDurations.Clear();
        dragAudioPayload = null;
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
                    double otherStart = JsonScalar.Number((other["startFrame"] as JsonObject)?["time"]);
                    if (otherStart > nextStart)
                        duration = Math.Min(duration, otherStart - nextStart);
                }
                originalAudioDuration = duration;
            }
            JsonObject segment = new()
            {
                ["type"] = sound ? "sound" : "frame",
                ["asset"] = assetIndex,
                ["startFrame"] = AnimationFrameData.Create(nextStart),
                ["endFrame"] = AnimationFrameData.Create(nextStart + duration),
            };
            if (sound)
                segment["originalDuration"] = originalAudioDuration;
            else
                segment["flipX"] = false;
            segments.Add(segment);
            nextStart += duration;
        }
        if (segments.Count == 0 || overlaps(track, start, nextStart, -1))
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
        JsonArray tracks = lines(true);
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
        index.RefreshTrack(getData(), track);
        SetSegmentSelection(track, target.Count - 1);
        SelectedTimeTag = -1;
        SegmentSelected?.Invoke(SelectedTrack, SelectedSegment);
        SegmentChanged?.Invoke();
        Refresh();
    }

    private void updateCanvasSize()
    {
        double contentEnd = Math.Max(5.0, Math.Max(index.Duration, tagCache.MaximumTime));
        Width = (contentEnd + 1) * PixelsPerSecond;
        Height = HeaderHeight + (Math.Max(5, index.TrackCount) + 1) * TrackHeight;
    }

    private JsonObject? segmentAt(int track, int segment)
    {
        return track >= 0 && track < lines().Count && lines()[track] is JsonObject line && line["timeSegments"] is JsonArray segments
            && segment >= 0 && segment < segments.Count ? segments[segment] as JsonObject : null;
    }

    private JsonArray lines(bool create = false)
    {
        JsonObject data = getData();
        if (data["timeLines"] is JsonArray lines)
            return lines;
        lines = new JsonArray();
        if (create)
            data["timeLines"] = lines;
        return lines;
    }

    private JsonArray timeTags(bool create = false)
    {
        JsonObject data = getData();
        if (data["timeTags"] is JsonArray tags)
            return tags;
        tags = new JsonArray();
        if (create)
            data["timeTags"] = tags;
        return tags;
    }

    private int sortTimeTags(JsonObject? selected)
    {
        JsonArray tags = timeTags();
        List<(JsonNode? Node, double Time, int Order)> ordered = tags
            .Select((node, index) => (
                Node: node,
                Time: node is JsonObject timeTag ? JsonScalar.Number(timeTag["time"]) : double.PositiveInfinity,
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

    private int frameRate() => Math.Max(1, (int)JsonScalar.Number(getData()["frameRate"], 30));
    private double snap(double time) => Math.Round(time * frameRate()) / frameRate();
    private readonly record struct TimelineZoomAnchor(
        double Time,
        double ContentY,
        Point ViewportPoint);
}
