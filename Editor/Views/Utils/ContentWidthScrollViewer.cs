using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using System;

namespace Ludork.Views.Utils;

public sealed class ContentWidthScrollViewer : ScrollViewer
{
    private ScrollBar? verticalScrollBar;

    public double RequiredWidth { get; private set; }

    public event EventHandler? RequiredWidthChanged;

    protected override Type StyleKeyOverride => typeof(ScrollViewer);

    protected override void OnApplyTemplate(TemplateAppliedEventArgs args)
    {
        base.OnApplyTemplate(args);
        verticalScrollBar = args.NameScope.Find<ScrollBar>("PART_VerticalScrollBar");
    }

    protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change)
    {
        base.OnPropertyChanged(change);
        if (change.Property == ExtentProperty)
            InvalidateMeasure();
    }

    protected override Size MeasureOverride(Size availableSize)
    {
        Size measured = base.MeasureOverride(availableSize);
        if (Content is not Control content)
            return measured;
        content.Measure(Size.Infinity);
        double width = Math.Ceiling(content.DesiredSize.Width + Padding.Left + Padding.Right
            + BorderThickness.Left + BorderThickness.Right + (verticalScrollBar?.DesiredSize.Width ?? 0));
        if (RequiredWidth != width)
        {
            RequiredWidth = width;
            RequiredWidthChanged?.Invoke(this, EventArgs.Empty);
        }
        content.InvalidateMeasure();
        measured = base.MeasureOverride(availableSize);
        return new Size(Math.Min(width, availableSize.Width), measured.Height);
    }
}
