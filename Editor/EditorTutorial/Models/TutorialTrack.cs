using System.Collections.Generic;

namespace Ludork.EditorTutorial.Models;

public sealed class TutorialTrack
{
    public string Target { get; init; } = string.Empty;
    public string Property { get; init; } = string.Empty;
    public List<TutorialKeyframe> Keyframes { get; init; } = [];
}
