using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Ludork.Services;
using System;
using System.Globalization;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class SubtitleTimeline : Control
{
    private readonly Func<JsonArray> getSections;
    private int dragMode;
    private Point dragStart;
    private double originalStart;
    private double originalEnd;
    private double pixelsPerSecond = 100;
    private double requestedPixelsPerSecond = 100;

    public SubtitleTimeline(Func<JsonArray> getSections)
    {
        this.getSections = getSections;
        Height = 108;
        HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Left;
        VerticalAlignment = Avalonia.Layout.VerticalAlignment.Top;
        Focusable = true;
        ClipToBounds = true;
    }

    public event Action<int>? SelectionChanged;
    public event Action<double, bool>? Scrub;
    public event Action<int, double, double>? SegmentChanged;
    public event Action? GestureStarted;
    public event Action? GestureEnded;
    public int SelectedIndex { get; set; } = -1;
    public double CurrentTime { get; set; }
    public double Duration { get; set; }

    public void SetZoom(double value)
    {
        requestedPixelsPerSecond = 100 * Math.Clamp(value, 0.05, 20);
        Refresh();
    }

    public void Refresh()
    {
        pixelsPerSecond = Math.Min(requestedPixelsPerSecond, 100000 / Math.Max(5, Duration));
        Width = Math.Max(600, (Math.Max(5, Duration) + 1) * pixelsPerSecond + 24);
        InvalidateVisual();
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);
        context.FillRectangle(EditorTheme.Brush("Surface"), new Rect(Bounds.Size));
        double step = Math.Pow(10, Math.Ceiling(Math.Log10(Math.Max(1, 80 / pixelsPerSecond))));
        for (double time = 0; time * pixelsPerSecond < Bounds.Width; time += step)
        {
            double x = 12 + time * pixelsPerSecond;
            context.DrawLine(new Pen(EditorTheme.Brush("Border")), new Point(x, 25), new Point(x, 98));
            FormattedText label = new(time.ToString("0.#", CultureInfo.InvariantCulture) + "s",
                CultureInfo.InvariantCulture, FlowDirection.LeftToRight, Typeface.Default, 12, EditorTheme.Brush("TextMuted"));
            context.DrawText(label, new Point(x + 3, 4));
        }
        JsonArray sections = getSections();
        for (int index = 0; index < sections.Count; index++)
        {
            if (sections[index] is not JsonObject section)
                continue;
            double start = Number(section["startTime"]);
            double end = Number(section["endTime"]);
            Rect rectangle = new(12 + start * pixelsPerSecond, 38, Math.Max(4, (end - start) * pixelsPerSecond), 45);
            IBrush fill = index == SelectedIndex ? Brushes.SteelBlue : Brushes.DarkSlateGray;
            context.FillRectangle(fill, rectangle, 4);
            context.DrawRectangle(new Pen(index == SelectedIndex ? Brushes.White : Brushes.Gray), rectangle, 4);
            using (context.PushClip(rectangle))
            {
                FormattedText label = new((index + 1).ToString(CultureInfo.InvariantCulture), CultureInfo.InvariantCulture,
                    FlowDirection.LeftToRight, Typeface.Default, 14, Brushes.White);
                context.DrawText(label, rectangle.TopLeft + new Vector(8, 12));
            }
        }
        double playhead = 12 + CurrentTime * pixelsPerSecond;
        context.DrawLine(new Pen(Brushes.OrangeRed, 2), new Point(playhead, 0), new Point(playhead, Bounds.Height));
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        if (!args.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        Focus();
        Point point = args.GetPosition(this);
        dragStart = point;
        dragMode = 4;
        JsonArray sections = getSections();
        if (point.Y >= 38 && point.Y <= 83)
        {
            for (int index = sections.Count - 1; index >= 0; index--)
            {
                if (sections[index] is not JsonObject section)
                    continue;
                double start = Number(section["startTime"]);
                double end = Number(section["endTime"]);
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
        InvalidateVisual();
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

    private double timeAt(Point point) => Math.Max(0, Math.Round((point.X - 12) / pixelsPerSecond, 3));
    private static double Number(JsonNode? value) => value is JsonValue number && number.TryGetValue(out double result) && double.IsFinite(result) ? result : 0;
}
