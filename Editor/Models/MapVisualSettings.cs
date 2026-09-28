using System.Text.Json.Nodes;

namespace Ludork.Models;

internal readonly record struct MapVisualSettings(
    string Fog, int FogPower, double FogOx, double FogOy, int FogDistort, string Panorama)
{
    public static MapVisualSettings From(MapInfo value)
        => new(value.Fog, value.FogPower, value.FogOx, value.FogOy, value.FogDistort, value.Panorama);

    public static MapVisualSettings From(WorldMapInfo value)
        => new(value.Fog, value.FogPower, value.FogOx, value.FogOy, value.FogDistort, value.Panorama);

    public void ApplyDelta(JsonObject candidate, MapVisualSettings baseline)
    {
        bool fogChanged = Fog.Trim() != baseline.Fog.Trim();
        bool clearFog = fogChanged && string.IsNullOrWhiteSpace(Fog);
        if (fogChanged)
            candidate["fog"] = Fog.Trim();
        if (clearFog || FogPower != baseline.FogPower)
            candidate["fogPower"] = clearFog ? 0 : FogPower;
        if (clearFog || FogOx != baseline.FogOx)
            candidate["fogOx"] = clearFog ? 0.0 : FogOx;
        if (clearFog || FogOy != baseline.FogOy)
            candidate["fogOy"] = clearFog ? 0.0 : FogOy;
        if (clearFog || FogDistort != baseline.FogDistort)
            candidate["fogDistort"] = clearFog ? 0 : FogDistort;
        if (Panorama.Trim() != baseline.Panorama.Trim())
            candidate["panorama"] = Panorama.Trim();
    }
}
