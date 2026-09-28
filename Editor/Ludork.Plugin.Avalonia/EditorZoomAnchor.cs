using Avalonia;
using Avalonia.Controls;

namespace Ludork.Plugin.Avalonia;

public sealed class EditorZoomAnchor
{
    private Anchor? pending;

    public bool IsPending => pending is not null;

    public void Capture(Point contentPoint, Point viewportPoint, Point origin, double scale)
    {
        pending = new Anchor(
            new Point((contentPoint.X - origin.X) / scale, (contentPoint.Y - origin.Y) / scale),
            viewportPoint);
    }

    public void Apply(ScrollViewer? scrollViewer, Point origin, double scale)
    {
        Anchor? anchor = pending;
        pending = null;
        if (anchor is not Anchor value || scrollViewer is null)
            return;
        Point contentAnchor = new(
            origin.X + value.Position.X * scale,
            origin.Y + value.Position.Y * scale);
        scrollViewer.Offset = EditorZoomInput.GetAnchoredOffset(
            contentAnchor, value.ViewportPoint, scrollViewer.Extent, scrollViewer.Viewport);
    }

    public void Clear()
    {
        pending = null;
    }

    private readonly record struct Anchor(Point Position, Point ViewportPoint);
}
