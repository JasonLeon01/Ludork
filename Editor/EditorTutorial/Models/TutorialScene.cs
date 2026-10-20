using System.Collections.Generic;

namespace Ludork.EditorTutorial.Models;

public sealed class TutorialScene
{
    public string Id { get; init; } = string.Empty;
    public string Trigger { get; init; } = string.Empty;
    public string Anchor { get; init; } = string.Empty;
    public double Duration { get; init; }
    public bool Loop { get; init; }
    public List<TutorialElement> Elements { get; init; } = [];
    public List<TutorialTrack> Tracks { get; init; } = [];
}
