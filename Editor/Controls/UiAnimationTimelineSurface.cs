using Ludork.Services;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using System;
using System.Globalization;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class UiAnimationTimelineSurface : Control
{
    private const double HeaderHeight = 26;
    private const double TrackHeight = 28;
    private const double LabelWidth = 104;
    private const double PixelsPerSecond = 240;
    private static readonly string[] TrackNames = ["translation", "rotation", "scale", "colour"];
    private JsonObject? animation;
    private string selectedTrack = "translation";
    private int selectedKey = -1;
    private double currentTime;
    private bool editable;
    private bool draggingKey;

    public UiAnimationTimelineSurface()
    {
        Height = HeaderHeight + TrackNames.Length * TrackHeight;
        MinWidth = 720;
        Focusable = true;
    }

    public event EventHandler<double>? TimeChanged;
    public event EventHandler<UiAnimationKeySelectionEventArgs>? SelectionChanged;
    public event EventHandler<UiAnimationKeyMoveEventArgs>? KeyMoved;

    public void SetState(
        JsonObject? animation,
        string selectedTrack,
        int selectedKey,
        double currentTime,
        bool editable)
    {
        this.animation = animation;
        this.selectedTrack = selectedTrack;
        this.selectedKey = selectedKey;
        this.currentTime = currentTime;
        this.editable = editable;
        Width = Math.Max(720, LabelWidth + duration() * PixelsPerSecond + 40);
        InvalidateVisual();
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);
        Rect bounds = new(Bounds.Size);
        context.FillRectangle(EditorTheme.Brush("Surface"), bounds);
        context.FillRectangle(
            EditorTheme.Brush("Surface"),
            new Rect(LabelWidth, 0, Math.Max(0, bounds.Width - LabelWidth), HeaderHeight));
        context.FillRectangle(
            EditorTheme.Brush("Surface"),
            new Rect(0, 0, LabelWidth, bounds.Height));
        drawRuler(context, bounds);
        for (int trackIndex = 0; trackIndex < TrackNames.Length; trackIndex++)
            drawTrack(context, bounds, trackIndex);
        double playheadX = LabelWidth + currentTime * PixelsPerSecond;
        Pen playheadPen = new(new SolidColorBrush(Color.Parse("#ff5c5c")), 1);
        context.DrawLine(playheadPen, new Point(playheadX, 0), new Point(playheadX, bounds.Height));
        StreamGeometry head = new();
        using (StreamGeometryContext geometry = head.Open())
        {
            geometry.BeginFigure(new Point(playheadX, 16), true);
            geometry.LineTo(new Point(playheadX - 5, 10));
            geometry.LineTo(new Point(playheadX - 5, 0));
            geometry.LineTo(new Point(playheadX + 5, 0));
            geometry.LineTo(new Point(playheadX + 5, 10));
            geometry.EndFigure(true);
        }
        context.DrawGeometry(new SolidColorBrush(Color.Parse("#ff5c5c")), null, head);
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
        selectedTrack = track;
        selectedKey = keyIndex;
        SelectionChanged?.Invoke(this, new UiAnimationKeySelectionEventArgs(track, keyIndex));
        if (keyIndex >= 0)
        {
            JsonObject? key = keyAt(track, keyIndex);
            if (key is not null)
                TimeChanged?.Invoke(this, number(key["time"]));
            if (editable)
            {
                draggingKey = true;
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
        draggingKey = false;
        e.Pointer.Capture(null);
        e.Handled = true;
    }

    protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs e)
    {
        draggingKey = false;
        base.OnPointerCaptureLost(e);
    }

    private void drawRuler(DrawingContext context, Rect bounds)
    {
        double end = Math.Max(duration(), (bounds.Width - LabelWidth) / PixelsPerSecond);
        for (double time = 0.0; time <= end + 0.0001; time += 0.25)
        {
            double x = LabelWidth + time * PixelsPerSecond;
            bool whole = Math.Abs(time - Math.Round(time)) < 0.0001;
            Pen pen = new(
                EditorTheme.Brush(whole ? "TextDisabled" : "Border"),
                1);
            context.DrawLine(pen, new Point(x, whole ? 0 : 14), new Point(x, bounds.Height));
            if (!whole)
                continue;
            FormattedText label = new(
                time.ToString("0", CultureInfo.InvariantCulture) + "s",
                CultureInfo.CurrentUICulture,
                FlowDirection.LeftToRight,
                Typeface.Default,
                10,
                EditorTheme.Brush("TextMuted"));
            context.DrawText(label, new Point(x + 3, 2));
        }
    }

    private void drawTrack(DrawingContext context, Rect bounds, int trackIndex)
    {
        string track = TrackNames[trackIndex];
        double y = HeaderHeight + trackIndex * TrackHeight;
        IBrush fill = EditorTheme.Brush(string.Equals(track, selectedTrack, StringComparison.Ordinal)
            ? "AccentMuted"
            : trackIndex % 2 == 0 ? "Surface" : "Background");
        context.FillRectangle(
            fill,
            new Rect(0, y, bounds.Width, TrackHeight));
        FormattedText label = new(
            track,
            CultureInfo.CurrentUICulture,
            FlowDirection.LeftToRight,
            Typeface.Default,
            11,
            Brushes.White);
        context.DrawText(label, new Point(8, y + 6));
        JsonArray? keys = (animation?["tracks"] as JsonObject)?[track] as JsonArray;
        if (keys is null)
            return;
        for (int keyIndex = 0; keyIndex < keys.Count; keyIndex++)
        {
            if (keys[keyIndex] is not JsonObject key)
                continue;
            double x = LabelWidth + number(key["time"]) * PixelsPerSecond;
            double centerY = y + TrackHeight / 2.0;
            StreamGeometry diamond = new();
            using (StreamGeometryContext geometry = diamond.Open())
            {
                geometry.BeginFigure(new Point(x, centerY - 6), true);
                geometry.LineTo(new Point(x + 6, centerY));
                geometry.LineTo(new Point(x, centerY + 6));
                geometry.LineTo(new Point(x - 6, centerY));
                geometry.EndFigure(true);
            }
            bool selected = string.Equals(track, selectedTrack, StringComparison.Ordinal)
                && keyIndex == selectedKey;
            context.DrawGeometry(
                new SolidColorBrush(Color.Parse(selected ? "#ffd166" : "#8ab4f8")),
                selected ? new Pen(Brushes.White, 1) : null,
                diamond);
        }
    }

    private int hitKey(string track, double x)
    {
        JsonArray? keys = (animation?["tracks"] as JsonObject)?[track] as JsonArray;
        if (keys is null)
            return -1;
        int result = -1;
        double distance = 8.0;
        for (int index = 0; index < keys.Count; index++)
        {
            if (keys[index] is not JsonObject key)
                continue;
            double current = Math.Abs(LabelWidth + number(key["time"]) * PixelsPerSecond - x);
            if (current <= distance)
            {
                result = index;
                distance = current;
            }
        }
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
        return Math.Max(0.01, number(animation?["duration"], 0.5));
    }

    private static double number(JsonNode? value, double fallback = 0.0)
    {
        if (value is not JsonValue json)
            return fallback;
        if (json.TryGetValue(out double doubleValue))
            return doubleValue;
        if (json.TryGetValue(out long integerValue))
            return integerValue;
        if (json.TryGetValue(out decimal decimalValue))
            return decimal.ToDouble(decimalValue);
        return fallback;
    }
}
