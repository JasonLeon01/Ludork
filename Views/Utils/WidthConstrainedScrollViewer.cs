using Avalonia;
using Avalonia.Controls;
using System;

namespace Ludork.Views.Utils;

public sealed class WidthConstrainedScrollViewer : ScrollViewer
{
    protected override Type StyleKeyOverride => typeof(ScrollViewer);

    protected override Size MeasureOverride(Size availableSize)
    {
        Size measured = base.MeasureOverride(availableSize);
        double width = double.IsInfinity(availableSize.Width)
            ? Math.Max(MinWidth, 0)
            : Math.Min(measured.Width, availableSize.Width);
        double height = double.IsInfinity(availableSize.Height)
            ? measured.Height
            : Math.Min(measured.Height, availableSize.Height);
        return new Size(width, height);
    }
}
