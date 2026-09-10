using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using System;

namespace Ludork.Controls;

public sealed class AspectRatioPanel : Panel
{
    public static readonly StyledProperty<double> AspectRatioProperty = AvaloniaProperty.Register<AspectRatioPanel, double>(
        nameof(AspectRatio),
        4.0 / 3.0);

    public double AspectRatio
    {
        get => GetValue(AspectRatioProperty);
        set => SetValue(AspectRatioProperty, value);
    }

    protected override Size MeasureOverride(Size availableSize)
    {
        foreach (Control child in Children)
            child.Measure(availableSize);
        return availableSize;
    }

    protected override Size ArrangeOverride(Size finalSize)
    {
        double ratio = double.IsFinite(AspectRatio) && AspectRatio > 0
            ? AspectRatio
            : 1.0;
        double width = finalSize.Width;
        double height = finalSize.Height;
        if (width > 0 && height > 0)
        {
            if (width / height > ratio)
                width = height * ratio;
            else
                height = width / ratio;
        }
        Rect bounds = new(
            Math.Max(0, (finalSize.Width - width) / 2.0),
            Math.Max(0, (finalSize.Height - height) / 2.0),
            Math.Max(0, width),
            Math.Max(0, height));
        foreach (Control child in Children)
            child.Arrange(bounds);
        return finalSize;
    }
}
