using Avalonia;
using Ludork.Models;
using System;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    public Rect? GetPreviewLightBounds(string mapKey)
    {
        return tryGetMapCatalogEntry(normaliseMapKey(mapKey), out MapCatalogEntry entry)
            ? entry.PreviewLightBounds : null;
    }

    private Rect? calculatePreviewLightBounds(JsonObject sources, BlueprintPreviewService? previews)
    {
        if (previews is null)
            return null;
        Rect? result = null;
        double cellSize = store.Configs.getCellSize();
        foreach (MapActorCollection collection in WorldDataService.enumerateActorCollections(sources))
        {
            foreach (JsonObject actor in collection.Actors.OfType<JsonObject>())
            {
                if (actor["position"] is not JsonArray { Count: >= 2 } position)
                    continue;
                ResolvedBlueprintClass? resolved = previews.tryResolveMapActorClass(sources, actor);
                ActorLightDescriptor? light = resolved is null ? null : previews.tryResolveActorLight(resolved);
                if (light is null)
                    continue;
                Point centre = light.GetWorldPosition(new Point(
                    JsonScalar.FiniteNumber(position[0]) * cellSize,
                    JsonScalar.FiniteNumber(position[1]) * cellSize));
                double diameter = light.Radius * 2;
                if (!double.IsFinite(centre.X) || !double.IsFinite(centre.Y) || !double.IsFinite(diameter))
                    continue;
                Rect bounds = new(centre.X - light.Radius, centre.Y - light.Radius, diameter, diameter);
                result = result is Rect previous ? previous.Union(bounds) : bounds;
            }
        }
        return result;
    }
}
