using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class AnimationTimelineSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public IReadOnlyList<AnimationSegmentSnapshot> Segments => Objects("timeSegments", value => new AnimationSegmentSnapshot(value));
}
