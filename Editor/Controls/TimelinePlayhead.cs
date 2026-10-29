using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;

namespace Ludork.Controls;

internal sealed class TimelinePlayhead : Control
{
    private readonly IBrush brush;
    private readonly Pen pen;
    private readonly StreamGeometry? head;
    private readonly TranslateTransform translation = new();

    public TimelinePlayhead(IBrush brush, double strokeThickness, bool showHead = false,
        double headWidth = 12, double headShoulderHeight = 12, double headHeight = 18)
    {
        this.brush = brush;
        pen = new Pen(brush, strokeThickness);
        Width = System.Math.Max(headWidth, strokeThickness);
        HorizontalAlignment = HorizontalAlignment.Left;
        VerticalAlignment = VerticalAlignment.Stretch;
        IsHitTestVisible = false;
        RenderTransform = translation;
        if (showHead)
        {
            head = new StreamGeometry();
            using StreamGeometryContext geometry = head.Open();
            double center = Width / 2;
            geometry.BeginFigure(new Point(center, headHeight), true);
            geometry.LineTo(new Point(center - headWidth / 2, headShoulderHeight));
            geometry.LineTo(new Point(center - headWidth / 2, 0));
            geometry.LineTo(new Point(center + headWidth / 2, 0));
            geometry.LineTo(new Point(center + headWidth / 2, headShoulderHeight));
            geometry.EndFigure(true);
        }
    }

    public void SetPosition(double x)
    {
        translation.X = x - Width / 2;
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);
        double center = Bounds.Width / 2;
        context.DrawLine(pen, new Point(center, 0), new Point(center, Bounds.Height));
        if (head is not null)
            context.DrawGeometry(brush, null, head);
    }
}
