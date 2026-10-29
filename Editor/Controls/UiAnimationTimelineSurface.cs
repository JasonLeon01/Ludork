using Ludork.Services;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class UiAnimationTimelineSurface : Panel
{
    private const double HeaderHeight = 26;
    private const double TrackHeight = 28;
    private const double LabelWidth = 104;
    private const double PixelsPerSecond = 240;
    private static readonly string[] TrackNames = ["translation", "rotation", "scale", "colour"];
    private static readonly IBrush KeyBrush = new SolidColorBrush(Color.Parse("#8ab4f8"));
    private static readonly IBrush SelectedKeyBrush = new SolidColorBrush(Color.Parse("#ffd166"));
    private static readonly Pen SelectedKeyPen = new(Brushes.White, 1);
    private static readonly StreamGeometry Diamond = createDiamond();
    private readonly TimelinePlayhead playhead = new(new SolidColorBrush(Color.Parse("#ff5c5c")), 1, true, 10, 10, 16);
    private readonly Dictionary<long, FormattedText> rulerLabels = [];
    private readonly List<long> obsoleteTicks = [];
    private readonly FormattedText?[] trackLabels = new FormattedText[TrackNames.Length];
    private readonly TimelineDrawingLayer contentLayer;
    private Rect viewport;
    private IBrush? labelBrush;
    private IBrush? borderBrush;
    private IBrush? wholeBrush;
    private Pen? borderPen;
    private Pen? wholePen;
    private string? culture;
    private JsonObject? animation;
    private string selectedTrack = "translation";
    private int selectedKey = -1;
    private double currentTime;
    private bool editable;
    private bool draggingKey;

    public UiAnimationTimelineSurface()
    {
        contentLayer = new TimelineDrawingLayer(drawContent) { IsHitTestVisible = false };
        Children.Add(contentLayer);
        Background = Brushes.Transparent;
        Height = HeaderHeight + TrackNames.Length * TrackHeight;
        MinWidth = 720;
        Focusable = true;
        Children.Add(playhead);
        SizeChanged += (_, _) => contentLayer.InvalidateVisual();
        DetachedFromVisualTree += (_, _) => finishKeyGesture();
        ActualThemeVariantChanged += (_, _) => contentLayer.InvalidateVisual();
        ResourcesChanged += (_, _) => contentLayer.InvalidateVisual();
        EffectiveViewportChanged += (_, args) =>
        {
            if (viewport == args.EffectiveViewport)
                return;
            viewport = args.EffectiveViewport;
            contentLayer.InvalidateVisual();
        };
    }

    public event EventHandler<double>? TimeChanged;
    public event EventHandler<UiAnimationKeySelectionEventArgs>? SelectionChanged;
    public event EventHandler<UiAnimationKeyMoveEventArgs>? KeyMoved;
    public event EventHandler? GestureStarted;
    public event EventHandler? GestureEnded;

    internal bool IsDraggingKey => draggingKey;

    public void SetContent(JsonObject? animation, bool editable)
    {
        this.animation = animation;
        this.editable = editable;
        Width = Math.Max(720, LabelWidth + duration() * PixelsPerSecond + 40);
        contentLayer.InvalidateVisual();
    }

    public void SetSelection(string track, int key)
    {
        if (selectedTrack == track && selectedKey == key)
            return;
        selectedTrack = track;
        selectedKey = key;
        contentLayer.InvalidateVisual();
    }

    public void SetCurrentTime(double time)
    {
        currentTime = time;
        playhead.SetPosition(LabelWidth + currentTime * PixelsPerSecond);
    }

    public void RefreshLabels()
    {
        culture = null;
        contentLayer.InvalidateVisual();
    }

    private void drawContent(DrawingContext context)
    {
        Rect bounds = new(Bounds.Size);
        Rect visible = bounds.Intersect(viewport);
        if (visible.Width <= 0 || visible.Height <= 0)
            return;
        ensureDrawingResources();
        using IDisposable clip = context.PushClip(visible);
        context.FillRectangle(EditorTheme.Brush("Surface"), bounds);
        context.FillRectangle(
            EditorTheme.Brush("Surface"),
            new Rect(LabelWidth, 0, Math.Max(0, bounds.Width - LabelWidth), HeaderHeight));
        context.FillRectangle(
            EditorTheme.Brush("Surface"),
            new Rect(0, 0, LabelWidth, bounds.Height));
        drawRuler(context, bounds, visible);
        for (int trackIndex = 0; trackIndex < TrackNames.Length; trackIndex++)
            drawTrack(context, bounds, visible, trackIndex);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        if (!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        Focus();
        Point position = e.GetPosition(this);
        if (position.X < LabelWidth || position.Y < HeaderHeight)
        {
            if (position.X >= LabelWidth)
                setTime(position.X);
            return;
        }
        int trackIndex = Math.Clamp(
            (int)((position.Y - HeaderHeight) / TrackHeight),
            0,
            TrackNames.Length - 1);
        string track = TrackNames[trackIndex];
        int keyIndex = hitKey(track, position.X);
        SetSelection(track, keyIndex);
        SelectionChanged?.Invoke(this, new UiAnimationKeySelectionEventArgs(track, keyIndex));
        if (keyIndex >= 0)
        {
            JsonObject? key = keyAt(track, keyIndex);
            if (key is not null)
                TimeChanged?.Invoke(this, JsonScalar.Number(key["time"]));
            if (editable)
            {
                draggingKey = true;
                GestureStarted?.Invoke(this, EventArgs.Empty);
                e.Pointer.Capture(this);
            }
        }
        else
        {
            setTime(position.X);
        }
        e.Handled = true;
    }

    protected override void OnPointerMoved(PointerEventArgs e)
    {
        if (!draggingKey || !e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        double time = timeAt(e.GetPosition(this).X);
        KeyMoved?.Invoke(this, new UiAnimationKeyMoveEventArgs(selectedTrack, selectedKey, time));
        e.Handled = true;
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        if (!draggingKey)
            return;
        finishKeyGesture();
        e.Pointer.Capture(null);
        e.Handled = true;
    }

    protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs e)
    {
        finishKeyGesture();
        base.OnPointerCaptureLost(e);
    }

    private void finishKeyGesture()
    {
        if (!draggingKey)
            return;
        draggingKey = false;
        GestureEnded?.Invoke(this, EventArgs.Empty);
    }

    private void drawRuler(DrawingContext context, Rect bounds, Rect visible)
    {
        double end = Math.Max(duration(), (bounds.Width - LabelWidth) / PixelsPerSecond);
        long first = Math.Max(0, (long)Math.Floor((visible.Left - LabelWidth - 64) / PixelsPerSecond * 4));
        long last = Math.Min((long)Math.Floor(end * 4), (long)Math.Ceiling((visible.Right - LabelWidth) / PixelsPerSecond * 4));
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
            double time = tick / 4.0;
            double x = LabelWidth + time * PixelsPerSecond;
            bool whole = tick % 4 == 0;
            context.DrawLine(whole ? wholePen! : borderPen!, new Point(x, whole ? 0 : 14), new Point(x, bounds.Height));
            if (!whole)
                continue;
            if (!rulerLabels.TryGetValue(tick, out FormattedText? label))
            {
                label = new FormattedText(time.ToString("0", CultureInfo.InvariantCulture) + "s",
                    CultureInfo.CurrentUICulture, FlowDirection.LeftToRight, Typeface.Default, 10, labelBrush);
                rulerLabels.Add(tick, label);
            }
            context.DrawText(label, new Point(x + 3, 2));
        }
    }

    private void drawTrack(DrawingContext context, Rect bounds, Rect visible, int trackIndex)
    {
        string track = TrackNames[trackIndex];
        double y = HeaderHeight + trackIndex * TrackHeight;
        IBrush fill = EditorTheme.Brush(string.Equals(track, selectedTrack, StringComparison.Ordinal)
            ? "AccentMuted"
            : trackIndex % 2 == 0 ? "Surface" : "Background");
        context.FillRectangle(
            fill,
            new Rect(0, y, bounds.Width, TrackHeight));
        if (visible.Left < LabelWidth)
            context.DrawText(trackLabels[trackIndex]!, new Point(8, y + 6));
        JsonArray? keys = (animation?["tracks"] as JsonObject)?[track] as JsonArray;
        if (keys is null)
            return;
        int first = lowerBound(keys, (visible.Left - LabelWidth - 7) / PixelsPerSecond);
        double lastTime = (visible.Right - LabelWidth + 7) / PixelsPerSecond;
        for (int keyIndex = first; keyIndex < keys.Count; keyIndex++)
        {
            if (keys[keyIndex] is not JsonObject key)
                continue;
            double time = JsonScalar.Number(key["time"]);
            if (time > lastTime)
                break;
            double x = LabelWidth + time * PixelsPerSecond;
            double centerY = y + TrackHeight / 2.0;
            bool selected = string.Equals(track, selectedTrack, StringComparison.Ordinal)
                && keyIndex == selectedKey;
            using IDisposable transform = context.PushTransform(Matrix.CreateTranslation(x, centerY));
            context.DrawGeometry(selected ? SelectedKeyBrush : KeyBrush, selected ? SelectedKeyPen : null, Diamond);
        }
    }

    private int hitKey(string track, double x)
    {
        JsonArray? keys = (animation?["tracks"] as JsonObject)?[track] as JsonArray;
        if (keys is null)
            return -1;
        int result = -1;
        double distance = 8.0;
        int first = lowerBound(keys, (x - LabelWidth - distance) / PixelsPerSecond);
        double lastTime = (x - LabelWidth + distance) / PixelsPerSecond;
        for (int index = first; index < keys.Count; index++)
        {
            if (keys[index] is not JsonObject key)
                continue;
            if (JsonScalar.Number(key["time"]) > lastTime)
                break;
            double current = Math.Abs(LabelWidth + JsonScalar.Number(key["time"]) * PixelsPerSecond - x);
            if (current <= distance)
            {
                result = index;
                distance = current;
            }
        }
        return result;
    }

    private static int lowerBound(JsonArray keys, double time)
    {
        int left = 0;
        int right = keys.Count;
        while (left < right)
        {
            int middle = left + (right - left) / 2;
            if (JsonScalar.Number((keys[middle] as JsonObject)?["time"]) < time)
                left = middle + 1;
            else
                right = middle;
        }
        return left;
    }

    private void ensureDrawingResources()
    {
        IBrush nextLabel = EditorTheme.Brush("TextMuted");
        if (!ReferenceEquals(labelBrush, nextLabel) || culture != CultureInfo.CurrentUICulture.Name)
        {
            labelBrush = nextLabel;
            culture = CultureInfo.CurrentUICulture.Name;
            rulerLabels.Clear();
            for (int index = 0; index < TrackNames.Length; index++)
                trackLabels[index] = new FormattedText(TrackNames[index], CultureInfo.CurrentUICulture,
                    FlowDirection.LeftToRight, Typeface.Default, 11, Brushes.White);
        }
        IBrush nextBorder = EditorTheme.Brush("Border");
        if (!ReferenceEquals(borderBrush, nextBorder))
        {
            borderBrush = nextBorder;
            borderPen = new Pen(nextBorder, 1);
        }
        IBrush nextWhole = EditorTheme.Brush("TextDisabled");
        if (!ReferenceEquals(wholeBrush, nextWhole))
        {
            wholeBrush = nextWhole;
            wholePen = new Pen(nextWhole, 1);
        }
    }

    private static StreamGeometry createDiamond()
    {
        StreamGeometry result = new();
        using StreamGeometryContext geometry = result.Open();
        geometry.BeginFigure(new Point(0, -6), true);
        geometry.LineTo(new Point(6, 0));
        geometry.LineTo(new Point(0, 6));
        geometry.LineTo(new Point(-6, 0));
        geometry.EndFigure(true);
        return result;
    }

    private JsonObject? keyAt(string track, int index)
    {
        JsonArray? keys = (animation?["tracks"] as JsonObject)?[track] as JsonArray;
        return keys is not null && index >= 0 && index < keys.Count
            ? keys[index] as JsonObject
            : null;
    }

    private void setTime(double x)
    {
        TimeChanged?.Invoke(this, timeAt(x));
    }

    private double timeAt(double x)
    {
        return Math.Clamp((x - LabelWidth) / PixelsPerSecond, 0.0, duration());
    }

    private double duration()
    {
        return Math.Max(0.01, JsonScalar.Number(animation?["duration"], 0.5));
    }

}
