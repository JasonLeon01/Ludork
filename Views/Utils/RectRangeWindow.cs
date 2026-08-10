using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Ludork.Services;
using System;
using System.IO;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public readonly record struct RectRangeSelection(int X, int Y, int Width, int Height);

public sealed class RectRangeWindow : Window
{
    private readonly Bitmap? bitmap;
    private readonly RectRangeCanvas canvas;

    private RectRangeWindow(
        Bitmap? source,
        RectRangeSelection initial,
        int step,
        double minimumWidth,
        double minimumHeight)
    {
        bitmap = source;
        Title = LocaleService.Get("RECT_VIEWER_TITLE");
        Width = minimumWidth;
        Height = minimumHeight;
        MinWidth = minimumWidth;
        MinHeight = minimumHeight;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);

        canvas = new RectRangeCanvas(source, initial, Math.Max(1, step));
        ScrollViewer scroll = new()
        {
            Content = canvas,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
            HorizontalContentAlignment = HorizontalAlignment.Left,
            VerticalContentAlignment = VerticalAlignment.Top,
        };
        Border imageArea = new()
        {
            Background = new SolidColorBrush(Color.Parse("#1e1e1e")),
            BorderBrush = new SolidColorBrush(Color.Parse("#464646")),
            BorderThickness = new Thickness(1),
            ClipToBounds = true,
            Child = scroll,
        };

        Button confirm = new() { Content = LocaleService.Get("CONFIRM") };
        confirm.Click += (_, _) => Close(canvas.Selection);
        Button cancel = new() { Content = LocaleService.Get("CANCEL") };
        cancel.Click += (_, _) => Close(null);
        StackPanel actions = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { confirm, cancel },
        };

        Grid layout = new()
        {
            Margin = new Thickness(5),
            RowDefinitions = new RowDefinitions("*,Auto"),
            RowSpacing = 8,
        };
        layout.Children.Add(imageArea);
        Grid.SetRow(actions, 1);
        layout.Children.Add(actions);
        Content = layout;
        KeyDown += onKeyDown;
        Closed += (_, _) => bitmap?.Dispose();
    }

    public static Task<RectRangeSelection?> ShowAsync(
        Window owner,
        string imagePath,
        RectRangeSelection initial,
        int step)
    {
        Bitmap? source = loadBitmap(imagePath);
        int imageWidth = source?.PixelSize.Width ?? 0;
        int imageHeight = source?.PixelSize.Height ?? 0;
        RectRangeSelection normalized = Normalize(initial, imageWidth, imageHeight);
        double minimumWidth = Math.Max(480, owner.Bounds.Width / 2);
        double minimumHeight = Math.Max(320, owner.Bounds.Height / 2);
        RectRangeWindow window = new(source, normalized, step, minimumWidth, minimumHeight);
        return window.ShowDialog<RectRangeSelection?>(owner);
    }

    public static RectRangeSelection Normalize(RectRangeSelection value, int imageWidth, int imageHeight)
    {
        int width = Math.Max(0, value.Width);
        int height = Math.Max(0, value.Height);
        if (imageWidth > 0)
            width = Math.Min(width, imageWidth);
        if (imageHeight > 0)
            height = Math.Min(height, imageHeight);
        int maximumX = imageWidth > 0 ? Math.Max(0, imageWidth - width) : int.MaxValue;
        int maximumY = imageHeight > 0 ? Math.Max(0, imageHeight - height) : int.MaxValue;
        int x = Math.Clamp(value.X, 0, maximumX);
        int y = Math.Clamp(value.Y, 0, maximumY);
        return new RectRangeSelection(x, y, width, height);
    }

    private static Bitmap? loadBitmap(string imagePath)
    {
        if (string.IsNullOrWhiteSpace(imagePath) || !File.Exists(imagePath))
            return null;
        try
        {
            return new Bitmap(imagePath);
        }
        catch (Exception)
        {
            return null;
        }
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.Escape)
        {
            Close(null);
            args.Handled = true;
            return;
        }
        if (args.Key is Key.Enter or Key.Return)
        {
            Close(canvas.Selection);
            args.Handled = true;
        }
    }
}

internal sealed class RectRangeCanvas : Control
{
    private const double HandleSize = 8;
    private readonly Bitmap? bitmap;
    private readonly int imageWidth;
    private readonly int imageHeight;
    private readonly int step;
    private RectRangeSelection selection;
    private RectRangeDragMode dragMode;
    private Point dragStart;
    private RectRangeSelection dragStartSelection;

    public RectRangeCanvas(Bitmap? source, RectRangeSelection initial, int snapStep)
    {
        bitmap = source;
        imageWidth = source?.PixelSize.Width ?? 0;
        imageHeight = source?.PixelSize.Height ?? 0;
        step = Math.Max(1, snapStep);
        selection = RectRangeWindow.Normalize(initial, imageWidth, imageHeight);
        Cursor = new Cursor(StandardCursorType.Cross);
        ClipToBounds = true;
        Focusable = true;
    }

    public RectRangeSelection Selection => selection;

    protected override Size MeasureOverride(Size availableSize)
    {
        return new Size(Math.Max(1, imageWidth), Math.Max(1, imageHeight));
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        if (!args.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        Point point = args.GetPosition(this);
        dragStart = point;
        dragStartSelection = selection;
        if (isOnHandle(point))
        {
            dragMode = RectRangeDragMode.Resize;
        }
        else if (isInsideSelection(point))
        {
            dragMode = args.KeyModifiers.HasFlag(KeyModifiers.Shift)
                ? RectRangeDragMode.Resize
                : RectRangeDragMode.Move;
        }
        else
        {
            int width = selection.Width > 0 ? selection.Width : step;
            int height = selection.Height > 0 ? selection.Height : step;
            setSelection(
                snap(point.X) - width / 2,
                snap(point.Y) - height / 2,
                width,
                height);
            dragMode = RectRangeDragMode.None;
        }
        if (dragMode != RectRangeDragMode.None)
            args.Pointer.Capture(this);
        updateCursor(point);
        Focus();
        args.Handled = true;
    }

    protected override void OnPointerMoved(PointerEventArgs args)
    {
        base.OnPointerMoved(args);
        Point point = args.GetPosition(this);
        if (dragMode == RectRangeDragMode.None)
        {
            updateCursor(point);
            return;
        }
        double dx = point.X - dragStart.X;
        double dy = point.Y - dragStart.Y;
        if (dragMode == RectRangeDragMode.Move)
        {
            setSelection(
                snap(dragStartSelection.X + dx),
                snap(dragStartSelection.Y + dy),
                dragStartSelection.Width,
                dragStartSelection.Height);
        }
        else
        {
            setSelection(
                dragStartSelection.X,
                dragStartSelection.Y,
                Math.Max(step, snap(dragStartSelection.Width + dx)),
                Math.Max(step, snap(dragStartSelection.Height + dy)));
        }
        args.Handled = true;
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs args)
    {
        base.OnPointerReleased(args);
        if (dragMode == RectRangeDragMode.None)
            return;
        dragMode = RectRangeDragMode.None;
        args.Pointer.Capture(null);
        updateCursor(args.GetPosition(this));
        args.Handled = true;
    }

    protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs args)
    {
        base.OnPointerCaptureLost(args);
        dragMode = RectRangeDragMode.None;
    }

    public override void Render(DrawingContext context)
    {
        Rect bounds = new(Bounds.Size);
        context.FillRectangle(new SolidColorBrush(Color.Parse("#1e1e1e")), bounds);
        if (bitmap is not null)
        {
            Rect imageRect = new(0, 0, imageWidth, imageHeight);
            context.DrawImage(bitmap, imageRect, imageRect);
        }
        if (selection.Width <= 0 || selection.Height <= 0)
            return;
        Rect selected = new(selection.X, selection.Y, selection.Width, selection.Height);
        context.FillRectangle(new SolidColorBrush(Color.Parse("#3c00c8ff")), selected);
        context.DrawRectangle(new Pen(new SolidColorBrush(Color.Parse("#00c8ff")), 2), selected);
    }

    private void setSelection(double x, double y, double width, double height)
    {
        RectRangeSelection next = new(
            (int)Math.Round(x),
            (int)Math.Round(y),
            Math.Max(0, (int)Math.Round(width)),
            Math.Max(0, (int)Math.Round(height)));
        selection = RectRangeWindow.Normalize(next, imageWidth, imageHeight);
        InvalidateVisual();
    }

    private int snap(double value)
    {
        return (int)Math.Round(value / step) * step;
    }

    private bool isInsideSelection(Point point)
    {
        return selection.Width > 0
            && selection.Height > 0
            && point.X >= selection.X
            && point.X <= selection.X + selection.Width
            && point.Y >= selection.Y
            && point.Y <= selection.Y + selection.Height;
    }

    private bool isOnHandle(Point point)
    {
        return selection.Width > 0
            && selection.Height > 0
            && point.X >= selection.X + selection.Width - HandleSize
            && point.X <= selection.X + selection.Width
            && point.Y >= selection.Y + selection.Height - HandleSize
            && point.Y <= selection.Y + selection.Height;
    }

    private void updateCursor(Point point)
    {
        if (dragMode == RectRangeDragMode.Resize || isOnHandle(point))
            Cursor = new Cursor(StandardCursorType.BottomRightCorner);
        else if (dragMode == RectRangeDragMode.Move || isInsideSelection(point))
            Cursor = new Cursor(StandardCursorType.SizeAll);
        else
            Cursor = new Cursor(StandardCursorType.Cross);
    }
}

internal enum RectRangeDragMode
{
    None,
    Move,
    Resize,
}
