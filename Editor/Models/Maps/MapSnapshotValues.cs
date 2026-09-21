using System;
using System.Globalization;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public readonly record struct MapPoint(double X, double Y);
public readonly record struct MapColour(byte R, byte G, byte B, byte A);

internal static class MapSnapshotValues
{
    internal static string Text(JsonNode? value) => value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : string.Empty;
    internal static int Integer(JsonNode? value) => int.TryParse(value?.ToString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out int number) ? number : 0;
    internal static double Number(JsonNode? value, double fallback = 0) => double.TryParse(value?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out double number) ? number : fallback;
    internal static JsonNode? Cell(JsonNode? value, int x, int y) => value is JsonArray rows && y >= 0 && y < rows.Count
        && rows[y] is JsonArray row && x >= 0 && x < row.Count ? row[x] : null;
    internal static MapPoint Point(JsonNode? value) => value is JsonArray { Count: >= 2 } position ? new(Number(position[0]), Number(position[1])) : default;
    internal static MapColour Colour(JsonNode? value, MapColour fallback)
    {
        if (value is not JsonArray { Count: >= 3 } colour)
            return fallback;
        return new((byte)Math.Clamp((int)Number(colour[0], fallback.R), 0, 255), (byte)Math.Clamp((int)Number(colour[1], fallback.G), 0, 255),
            (byte)Math.Clamp((int)Number(colour[2], fallback.B), 0, 255), colour.Count >= 4 ? (byte)Math.Clamp((int)Number(colour[3], fallback.A), 0, 255) : fallback.A);
    }
}
