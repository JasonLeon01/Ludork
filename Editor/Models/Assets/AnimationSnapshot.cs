using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class AnimationSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public string Name => Text("name");
    public double FrameRate => Number("frameRate", 30);
    public IReadOnlyList<string> Assets => Strings("assets");
    public IReadOnlyList<AnimationTimelineSnapshot> Timelines => Objects("timeLines", value => new AnimationTimelineSnapshot(value));
}
