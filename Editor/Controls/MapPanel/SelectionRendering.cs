using Avalonia;
using Avalonia.Media;
using Ludork.Models;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Controls;

public sealed partial class MapPanel
{
    private static readonly Pen SelectedActorBackdropPen = new(Brushes.Black, 4);

    private void drawSelectedActors(DrawingContext context)
    {
        HashSet<(string Layer, string Tag)> selectedActorsByTag = selectedActors
            .Select(actor => (actor.LayerName, actor.Tag)).ToHashSet();
        foreach (KeyValuePair<string, List<ActorRenderState>> group in actorRenderStates)
        {
            if (getLayer(group.Key) is not MapLayerSnapshot layer || !isLayerVisible(layer))
                continue;
            for (int index = 0; index < group.Value.Count; index++)
            {
                ActorRenderState actor = group.Value[index];
                bool selected = IsRuntimeEditing
                    ? group.Key == selectedActorLayer && index == selectedActorIndex
                    : selectedActorsByTag.Contains((group.Key, actor.Actor.Tag));
                if (!selected || !tryGetActorPosition(actor.Actor, out int x, out int y))
                    continue;
                Rect cell = getLocalTileRect(x, y);
                Point[] corners = [cell.TopLeft, cell.TopRight, cell.BottomRight, cell.BottomLeft];
                if (actor.Image is not null && actor.Scale.X != 0 && actor.Scale.Y != 0)
                {
                    Rect source = getActorRenderSource(actor);
                    Rect bounds = new((actor.PreviewLease?.FrameOffset.X ?? 0) - actor.Origin.X,
                        (actor.PreviewLease?.FrameOffset.Y ?? 0) - actor.Origin.Y, source.Width, source.Height);
                    if (bounds.Width > 0 && bounds.Height > 0)
                    {
                        Matrix transform = createActorTransform(x, y, actor.Translation, actor.Scale, actor.Rotation);
                        corners = [transform.Transform(bounds.TopLeft), transform.Transform(bounds.TopRight),
                            transform.Transform(bounds.BottomRight), transform.Transform(bounds.BottomLeft)];
                    }
                }
                foreach (Pen pen in new[] { SelectedActorBackdropPen, SelectedActorPen })
                    for (int edge = 0; edge < corners.Length; edge++)
                        context.DrawLine(pen, corners[edge], corners[(edge + 1) % corners.Length]);
            }
        }
    }
}
