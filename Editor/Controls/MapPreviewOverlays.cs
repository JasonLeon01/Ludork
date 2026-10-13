using Avalonia;
using Avalonia.Media;
using Ludork.Services;
using System;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal static class MapPreviewOverlays
{
    private static readonly Pen LightOutlinePen = new(Brushes.Black, 4);
    private static readonly Pen LightPen = new(Brushes.White, 2, DashStyle.Dash);
    private static readonly IBrush OriginBrush = new SolidColorBrush(Color.FromArgb(220, 0, 255, 0));

    public static void Draw(DrawingContext context, JsonObject scene, Point origin, double scale, MapPreviewOptions options, bool origins)
    {
        if (origins && options.ShowActorOrigins && scene["sprites"] is JsonArray sprites)
        {
            double halfCell = JsonScalar.Number(scene["cellSize"], 32) * 0.5;
            foreach (JsonNode? sprite in sprites)
            {
                if (sprite?["position"] is not JsonArray { Count: 2 } position)
                    continue;
                Point center = origin + new Vector((JsonScalar.Number(position[0]) + halfCell) * scale,
                    (JsonScalar.Number(position[1]) + halfCell) * scale);
                context.DrawEllipse(OriginBrush, null, center, 3, 3);
            }
        }
        if (options.Lit || !options.ShowLightBounds || scene["lights"] is not JsonArray lights)
            return;
        foreach (JsonNode? light in lights)
        {
            if (light?["position"] is not JsonArray { Count: 2 } position)
                continue;
            Point center = origin + new Vector(JsonScalar.Number(position[0]) * scale, JsonScalar.Number(position[1]) * scale);
            double radius = JsonScalar.Number(light["radius"]) * scale;
            if (radius <= 0)
                continue;
            JsonArray? colour = light["colour"] as JsonArray;
            IBrush fill = new SolidColorBrush(Color.FromArgb(28,
                (byte)Math.Clamp(JsonScalar.Number(colour?[0], 255), 0, 255),
                (byte)Math.Clamp(JsonScalar.Number(colour?[1], 255), 0, 255),
                (byte)Math.Clamp(JsonScalar.Number(colour?[2], 255), 0, 255)));
            context.DrawEllipse(fill, LightOutlinePen, center, radius, radius);
            context.DrawEllipse(null, LightPen, center, radius, radius);
            context.DrawEllipse(Brushes.Black, null, center, 5, 5);
            context.DrawEllipse(Brushes.White, null, center, 3, 3);
        }
    }
}
