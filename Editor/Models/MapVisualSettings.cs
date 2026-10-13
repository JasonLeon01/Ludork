using System.Text.Json.Nodes;

namespace Ludork.Models;

internal readonly record struct MapVisualSettings(JsonObject Fog, string Panorama)
{
    public static MapVisualSettings From(MapInfo value)
        => new((JsonObject)value.Fog.DeepClone(), value.Panorama);

    public static MapVisualSettings From(WorldMapInfo value)
        => new((JsonObject)value.Fog.DeepClone(), value.Panorama);

    public void ApplyDelta(JsonObject candidate, MapVisualSettings baseline)
    {
        if (!JsonNode.DeepEquals(Fog, baseline.Fog))
            candidate["fog"] = Fog.DeepClone();
        if (Panorama.Trim() != baseline.Panorama.Trim())
            candidate["panorama"] = Panorama.Trim();
    }
}
