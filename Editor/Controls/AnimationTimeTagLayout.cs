using Avalonia.Media;
using System;
using System.Globalization;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class AnimationTimeTagLayout(int index, JsonObject data, double time)
{
    public int Index { get; } = index;
    public JsonObject Data { get; } = data;
    public double Time { get; set; } = time;
    public int DuplicateCount { get; set; }
    public double ActualX { get; private set; }
    public double MarkerX { get; private set; }
    public double Left => Math.Min(ActualX, MarkerX - 6);
    public double Right => MarkerX + Math.Max(6, 7 + Label.Width);
    public FormattedText Label { get; private set; } = createLabel(data, new SolidColorBrush(Color.Parse("#5ad1c4")));
    public FormattedText SelectedLabel { get; private set; } = createLabel(data, new SolidColorBrush(Color.Parse("#ffd166")));

    public void RefreshText()
    {
        Label = createLabel(Data, new SolidColorBrush(Color.Parse("#5ad1c4")));
        SelectedLabel = createLabel(Data, new SolidColorBrush(Color.Parse("#ffd166")));
    }

    public void SetScale(double pixelsPerSecond)
    {
        ActualX = Time * pixelsPerSecond;
        MarkerX = ActualX + DuplicateCount * 14;
    }

    private static FormattedText createLabel(JsonObject data, IBrush brush)
    {
        return new FormattedText(data["tag"]?.GetValue<string>() ?? string.Empty,
            CultureInfo.CurrentUICulture, Avalonia.Media.FlowDirection.LeftToRight,
            Typeface.Default, 10, brush);
    }
}
