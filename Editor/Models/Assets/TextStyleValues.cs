using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed record TextStyleFlags(bool Bold, bool Italic, bool Underlined, bool StrikeThrough);
public sealed record TextOutline(MapColour Colour, double Thickness);
public sealed record TextGlow(bool Enabled, MapColour Colour, double Radius, double Intensity);
public sealed record TextGradient(bool Enabled, string Direction, string Curve);

internal static class TextStyleValues
{
    internal static TextGlow ReadGlow(JsonObject value) => new(value["enabled"]?.GetValue<bool?>() ?? false,
        MapSnapshotValues.Colour(value["color"], new(255,255,255,0)), MapSnapshotValues.Number(value["radius"]), MapSnapshotValues.Number(value["intensity"]));
    internal static TextGradient ReadGradient(JsonObject value) => new(value["enabled"]?.GetValue<bool?>() ?? false,
        MapSnapshotValues.Text(value["direction"]), MapSnapshotValues.Text(value["curve"]));
}
