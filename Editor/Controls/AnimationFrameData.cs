using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal static class AnimationFrameData
{
    public static JsonObject Create(double time) => new()
    {
        ["time"] = time,
        ["position"] = new JsonArray(0.0, 0.0),
        ["rotation"] = 0.0,
        ["scale"] = new JsonArray(1.0, 1.0),
    };
}
