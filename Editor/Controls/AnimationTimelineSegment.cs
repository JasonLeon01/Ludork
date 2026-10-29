using Ludork.Services;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class AnimationTimelineSegment(int track, int index, JsonObject data)
{
    public int Track { get; } = track;
    public int Index { get; } = index;
    public JsonObject Data { get; } = data;
    public double Start => JsonScalar.Number((Data["startFrame"] as JsonObject)?["time"]);
    public double End => JsonScalar.Number((Data["endFrame"] as JsonObject)?["time"]);
    public bool IsSound => Data["type"]?.GetValue<string>() == "sound";
}
