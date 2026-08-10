using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using System;

namespace Ludork.Controls;

public sealed class MiddleEllipsisTextBlock : TextBlock
{
    public static readonly StyledProperty<string?> SourceTextProperty =
        AvaloniaProperty.Register<MiddleEllipsisTextBlock, string?>(nameof(SourceText));

    private readonly TextBlock measureTextBlock = new();

    public MiddleEllipsisTextBlock()
    {
        SizeChanged += (_, _) => updateText();
    }

    public string? SourceText
    {
        get => GetValue(SourceTextProperty);
        set => SetValue(SourceTextProperty, value);
    }

    protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change)
    {
        base.OnPropertyChanged(change);
        if (change.Property == SourceTextProperty)
            updateText();
    }

    private void updateText()
    {
        string sourceText = SourceText ?? string.Empty;
        if (Bounds.Width <= 0 || measureText(sourceText) <= Bounds.Width)
        {
            Text = sourceText;
            return;
        }

        const string ellipsis = "…";
        int visibleCount = sourceText.Length;
        int minimumVisibleCount = Math.Min(2, visibleCount);
        while (visibleCount > minimumVisibleCount)
        {
            int startLength = (visibleCount + 1) / 2;
            int endLength = visibleCount - startLength;
            string candidate = sourceText[..startLength] + ellipsis + sourceText[^endLength..];
            if (measureText(candidate) <= Bounds.Width)
            {
                Text = candidate;
                return;
            }
            visibleCount--;
        }

        Text = ellipsis;
    }

    private double measureText(string text)
    {
        measureTextBlock.FontFamily = FontFamily;
        measureTextBlock.FontSize = FontSize;
        measureTextBlock.FontStyle = FontStyle;
        measureTextBlock.FontWeight = FontWeight;
        measureTextBlock.FontStretch = FontStretch;
        measureTextBlock.LetterSpacing = LetterSpacing;
        measureTextBlock.Text = text;
        measureTextBlock.Measure(Size.Infinity);
        return measureTextBlock.DesiredSize.Width;
    }
}
