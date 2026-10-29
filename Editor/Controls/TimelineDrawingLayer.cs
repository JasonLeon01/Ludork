using Avalonia.Controls;
using Avalonia.Media;
using System;

namespace Ludork.Controls;

internal sealed class TimelineDrawingLayer(Action<DrawingContext> draw) : Control
{
    public override void Render(DrawingContext context)
    {
        base.Render(context);
        draw(context);
    }
}
