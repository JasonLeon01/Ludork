using Avalonia.Media;
using Ludork.Services;
using System;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils;

internal static class TextConfigValues
{
    public static Color Colour(JsonNode? node, Color fallback)
    {
        if (node is not JsonArray values || values.Count < 4)
            return fallback;
        return Color.FromArgb(
            component(values[3], fallback.A), component(values[0], fallback.R),
            component(values[1], fallback.G), component(values[2], fallback.B));
    }

    private static byte component(JsonNode? node, byte fallback)
    {
        return JsonScalar.TryGetFiniteNumber(node, out double number) && number == Math.Truncate(number)
            && number >= int.MinValue && number <= int.MaxValue
                ? (byte)Math.Clamp(number, 0, 255) : fallback;
    }
}
