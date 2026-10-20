namespace Ludork.EditorTutorial.Models;

public sealed class TutorialKeyframe
{
    public double Time { get; init; }
    public double Value { get; init; }
    public double Offset { get; init; }
    public string Easing { get; init; } = "linear";
}
