using System.Collections.Generic;

namespace Ludork.EditorTutorial.Models;

public sealed class TutorialElement
{
    public string Id { get; init; } = string.Empty;
    public string Type { get; init; } = string.Empty;
    public List<TutorialElement> Children { get; init; } = [];
    public TutorialValue X { get; init; }
    public TutorialValue Y { get; init; }
    public TutorialValue Width { get; init; }
    public TutorialValue Height { get; init; }
    public double Opacity { get; init; } = 1;
    public double ScaleX { get; init; } = 1;
    public double ScaleY { get; init; } = 1;
    public double Rotation { get; init; }
    public string? TextKey { get; init; }
    public List<string> TextArguments { get; init; } = [];
    public string? LayoutKey { get; init; }
    public string? Asset { get; init; }
    public string? ImageKey { get; init; }
    public string Stretch { get; init; } = "fill";
    public string ImageVerticalAlignment { get; init; } = "center";
    public string? Fill { get; init; }
    public string? Stroke { get; init; }
    public double FontSize { get; init; } = 16;
    public double StrokeThickness { get; init; } = 1;
}
