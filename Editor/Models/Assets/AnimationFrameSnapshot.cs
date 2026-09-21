using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class AnimationFrameSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public double Time => Number("time");
    public IReadOnlyList<double> Position => Numbers("position");
    public double Rotation => Number("rotation");
    public IReadOnlyList<double> Scale => Numbers("scale");
}
