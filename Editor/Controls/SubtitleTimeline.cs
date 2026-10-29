using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Ludork.Services;
using System;
using System.Globalization;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class SubtitleTimeline : Panel
{
    private readonly Func<JsonArray> getSections;
    private readonly TimelinePlayhead playhead = new(Brushes.OrangeRed, 2);
    private readonly TimelineIntervalIndex<SectionEntry> intervals = new([], item => item.Start, item => item.End);
    private readonly List<SectionEntry> candidates = [];
    private readonly Dictionary<long, FormattedText> rulerLabels = [];
    private readonly Dictionary<int, FormattedText> sectionLabels = [];
    private readonly List<long> obsoleteTicks = [];
    private readonly List<int> obsoleteSections = [];
    private static readonly Pen SelectedPen = new(Brushes.White);
    private static readonly Pen SectionPen = new(Brushes.Gray);
    private SectionEntry?[] sectionEntries = [];
    private JsonArray? indexedSections;
    private readonly TimelineDrawingLayer contentLayer;
    private Rect viewport;
    private IBrush? labelBrush;
    private IBrush? borderBrush;
    private Pen? borderPen;
    private int selectedIndex = -1;
    private double currentTime;
    private double duration;
    private double rulerStep;
    private double rulerLabelPadding = 100;
    private int dragMode;
    private Point dragStart;
    private double originalStart;
    private double originalEnd;
    private double pixelsPerSecond = 100;
    private double requestedPixelsPerSecond = 100;

    public SubtitleTimeline(Func<JsonArray> getSections)
    {
        contentLayer = new TimelineDrawingLayer(drawContent) { IsHitTestVisible = false };
        Children.Add(contentLayer);
        Background = Brushes.Transparent;
        this.getSections = getSections;
        Height = 108;
        HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Left;
        VerticalAlignment = Avalonia.Layout.VerticalAlignment.Top;
        Focusable = true;
        ClipToBounds = true;
        Children.Add(playhead);
        SizeChanged += (_, _) => contentLayer.InvalidateVisual();
        ActualThemeVariantChanged += (_, _) => contentLayer.InvalidateVisual();
        ResourcesChanged += (_, _) => contentLayer.InvalidateVisual();
        EffectiveViewportChanged += (_, args) =>
        {
            if (viewport == args.EffectiveViewport)
                return;
            viewport = args.EffectiveViewport;
            contentLayer.InvalidateVisual();
        };
        RefreshContent();
        Refresh();
    }

    public event Action<int>? SelectionChanged;
    public event Action<double, bool>? Scrub;
    public event Action<int, double, double>? SegmentChanged;
    public event Action? GestureStarted;
    public event Action? GestureEnded;
    public int SelectedIndex
    {
        get => selectedIndex;
        set
        {
            if (selectedIndex == value)
                return;
            selectedIndex = value;
            contentLayer.InvalidateVisual();
        }
    }
    public double CurrentTime
    {
        get => currentTime;
        set
        {
            currentTime = value;
            playhead.SetPosition(12 + value * pixelsPerSecond);
        }
    }
    public double Duration
    {
        get => duration;
        set
        {
            if (duration == value)
                return;
            duration = value;
            Refresh();
        }
    }
    public double ContentDuration => Math.Max(0, intervals.MaximumEnd);

    public void RefreshContent(int? changedIndex = null)
    {
        JsonArray sections = getSections();
        if (changedIndex is int index && ReferenceEquals(indexedSections, sections)
            && sectionEntries.Length == sections.Count && index >= 0 && index < sectionEntries.Length
            && sectionEntries[index] is SectionEntry entry && ReferenceEquals(entry.Section, sections[index]))
        {
            double start = Number(entry.Section["startTime"]);
            double end = Number(entry.Section["endTime"]);
            if (entry.Start == start && entry.End == end)
                return;
            entry.Start = start;
            entry.End = end;
            intervals.Update(entry);
        }
        else
        {
            indexedSections = sections;
            sectionEntries = new SectionEntry?[sections.Count];
            for (int position = 0; position < sections.Count; position++)
            {
                if (sections[position] is JsonObject section)
                    sectionEntries[position] = new SectionEntry(section, position,
                        Number(section["startTime"]), Number(section["endTime"]));
            }
            intervals.Reset(sectionEntries.OfType<SectionEntry>());
            sectionLabels.Clear();
        }
        contentLayer.InvalidateVisual();
    }

    public JsonObject? FindActiveSection(double time)
    {
        intervals.Query(time, time, candidates);
        foreach (SectionEntry entry in candidates)
        {
            if (entry.Start <= time && time < entry.End)
                return entry.Section;
        }
        return null;
    }

    public void SetZoom(double value)
    {
        double next = 100 * Math.Clamp(value, 0.05, 20);
        if (requestedPixelsPerSecond == next)
            return;
        requestedPixelsPerSecond = next;
        Refresh();
    }

    public void Refresh()
    {
        pixelsPerSecond = Math.Min(requestedPixelsPerSecond, 100000 / Math.Max(5, Duration));
        Width = Math.Max(600, (Math.Max(5, Duration) + 1) * pixelsPerSecond + 24);
        rulerLabelPadding = Math.Max(5, Duration).ToString("0.#", CultureInfo.InvariantCulture).Length * 12 + 24;
        playhead.SetPosition(12 + CurrentTime * pixelsPerSecond);
        contentLayer.InvalidateVisual();
    }

    private void drawContent(DrawingContext context)
    {
        Rect visible = new Rect(Bounds.Size).Intersect(viewport);
        if (visible.Width <= 0 || visible.Height <= 0)
            return;
        using IDisposable clip = context.PushClip(visible);
        context.FillRectangle(EditorTheme.Brush("Surface"), visible);
        double step = Math.Pow(10, Math.Ceiling(Math.Log10(Math.Max(1, 80 / pixelsPerSecond))));
        IBrush nextLabel = EditorTheme.Brush("TextMuted");
        if (!ReferenceEquals(labelBrush, nextLabel) || rulerStep != step)
        {
            labelBrush = nextLabel;
            rulerStep = step;
            rulerLabels.Clear();
        }
        IBrush nextBorder = EditorTheme.Brush("Border");
        if (!ReferenceEquals(borderBrush, nextBorder))
        {
            borderBrush = nextBorder;
            borderPen = new Pen(nextBorder);
        }
        long first = Math.Max(0, (long)Math.Floor((visible.Left - 12 - rulerLabelPadding) / pixelsPerSecond / step));
        long last = (long)Math.Ceiling((visible.Right - 12) / pixelsPerSecond / step);
        obsoleteTicks.Clear();
        foreach (long tick in rulerLabels.Keys)
        {
            if (tick < first || tick > last)
                obsoleteTicks.Add(tick);
        }
        foreach (long tick in obsoleteTicks)
            rulerLabels.Remove(tick);
        for (long tick = first; tick <= last; tick++)
        {
            double time = tick * step;
            if (time * pixelsPerSecond >= Bounds.Width)
                break;
            double x = 12 + time * pixelsPerSecond;
            context.DrawLine(borderPen!, new Point(x, 25), new Point(x, 98));
            if (!rulerLabels.TryGetValue(tick, out FormattedText? label))
            {
                label = new FormattedText(time.ToString("0.#", CultureInfo.InvariantCulture) + "s",
                    CultureInfo.InvariantCulture, FlowDirection.LeftToRight, Typeface.Default, 12, labelBrush);
                rulerLabels.Add(tick, label);
            }
            context.DrawText(label, new Point(x + 3, 4));
        }
        intervals.Query((visible.Left - 12 - 5) / pixelsPerSecond, (visible.Right - 12 + 1) / pixelsPerSecond, candidates);
        obsoleteSections.Clear();
        foreach (int index in sectionLabels.Keys)
        {
            SectionEntry? entry = index < sectionEntries.Length ? sectionEntries[index] : null;
            if (entry is null || !sectionRectangle(entry).Intersects(visible))
                obsoleteSections.Add(index);
        }
        foreach (int index in obsoleteSections)
            sectionLabels.Remove(index);
        foreach (SectionEntry entry in candidates)
        {
            Rect rectangle = sectionRectangle(entry);
            bool selected = entry.Index == SelectedIndex;
            context.FillRectangle(selected ? Brushes.SteelBlue : Brushes.DarkSlateGray, rectangle, 4);
            context.DrawRectangle(selected ? SelectedPen : SectionPen, rectangle, 4);
            using (context.PushClip(rectangle))
            {
                if (!sectionLabels.TryGetValue(entry.Index, out FormattedText? label))
                {
                    label = new FormattedText((entry.Index + 1).ToString(CultureInfo.InvariantCulture), CultureInfo.InvariantCulture,
                        FlowDirection.LeftToRight, Typeface.Default, 14, Brushes.White);
                    sectionLabels.Add(entry.Index, label);
                }
                context.DrawText(label, rectangle.TopLeft + new Vector(8, 12));
            }
        }
    }

    private Rect sectionRectangle(SectionEntry entry) =>
        new(12 + entry.Start * pixelsPerSecond, 38, Math.Max(4, (entry.End - entry.Start) * pixelsPerSecond), 45);

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        if (!args.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        Focus();
        Point point = args.GetPosition(this);
        dragStart = point;
        dragMode = 4;
        if (point.Y >= 38 && point.Y <= 83)
        {
            intervals.Query((point.X - 12 - 4) / pixelsPerSecond, (point.X - 12 + 4) / pixelsPerSecond, candidates);
            for (int candidate = candidates.Count - 1; candidate >= 0; candidate--)
            {
                SectionEntry entry = candidates[candidate];
                int index = entry.Index;
                double start = entry.Start;
                double end = entry.End;
                double left = 12 + start * pixelsPerSecond;
                double right = 12 + end * pixelsPerSecond;
                if (point.X < left - 4 || point.X > right + 4)
                    continue;
                SelectedIndex = index;
                originalStart = start;
                originalEnd = end;
                dragMode = Math.Abs(point.X - left) <= 7 ? 1 : Math.Abs(point.X - right) <= 7 ? 2 : 3;
                SelectionChanged?.Invoke(index);
                GestureStarted?.Invoke();
                break;
            }
        }
        args.Pointer.Capture(this);
        if (dragMode == 4)
            Scrub?.Invoke(timeAt(point), false);
        args.Handled = true;
    }

    protected override void OnPointerMoved(PointerEventArgs args)
    {
        base.OnPointerMoved(args);
        if (dragMode == 0)
            return;
        Point point = args.GetPosition(this);
        if (dragMode == 4)
            Scrub?.Invoke(timeAt(point), false);
        else
        {
            double delta = Math.Round((point.X - dragStart.X) / pixelsPerSecond, 3);
            double start = originalStart;
            double end = originalEnd;
            if (dragMode == 1)
                start = Math.Clamp(start + delta, 0, Math.Max(0, end - 0.001));
            else if (dragMode == 2)
                end = Math.Max(start + 0.001, end + delta);
            else
            {
                start = Math.Max(0, start + delta);
                end = start + originalEnd - originalStart;
            }
            SegmentChanged?.Invoke(SelectedIndex, start, end);
        }
        args.Handled = true;
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs args)
    {
        if (dragMode == 4)
            Scrub?.Invoke(timeAt(args.GetPosition(this)), true);
        else if (dragMode != 0)
            GestureEnded?.Invoke();
        dragMode = 0;
        args.Pointer.Capture(null);
        base.OnPointerReleased(args);
    }

    protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs args)
    {
        if (dragMode == 4)
            Scrub?.Invoke(CurrentTime, true);
        else if (dragMode != 0)
            GestureEnded?.Invoke();
        dragMode = 0;
        base.OnPointerCaptureLost(args);
    }

    private sealed class SectionEntry(JsonObject section, int index, double start, double end)
    {
        public JsonObject Section { get; } = section;
        public int Index { get; } = index;
        public double Start { get; set; } = start;
        public double End { get; set; } = end;
    }

    private double timeAt(Point point) => Math.Max(0, Math.Round((point.X - 12) / pixelsPerSecond, 3));
    private static double Number(JsonNode? value) => value is JsonValue number && number.TryGetValue(out double result) && double.IsFinite(result) ? result : 0;
}
