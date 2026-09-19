using Avalonia;
using Avalonia.Media;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class MapPanel
{
    private static readonly Pen ActorLightPen = new(Brushes.Gray, 2, DashStyle.Dash);
    private readonly List<ActorLightRenderState> actorLightRenderStates = [];
    private bool actorLightRenderStatesDirty = true;

    private void drawActorLightOverlay(DrawingContext context)
    {
        if (IsRuntimeEditing)
            return;
        ensureActorLightRenderStates();
        double displayScale = tileSize / (double)SourceTileSize;
        foreach (ActorLightRenderState state in actorLightRenderStates)
        {
            if (CurrentMapData?["layers"]?[state.Layer] is not JsonObject layer || !isLayerVisible(layer)
                || !tryGetActorPosition(state.Actor, out int x, out int y))
                continue;
            ActorLightDescriptor light = state.Light;
            Point center = createActorTransform(x, y, light.Translation, light.Scale, light.Rotation)
                .Transform(light.LocalPosition);
            center = new Point(snapToDevicePixel(center.X), snapToDevicePixel(center.Y));
            double radius = light.Radius * displayScale;
            context.DrawEllipse(new SolidColorBrush(getLightFill(light.Colour)), ActorLightPen, center, radius, radius);
            context.DrawEllipse(Brushes.Gray, null, center, 3, 3);
        }
    }

    private void ensureActorLightRenderStates()
    {
        if (!actorLightRenderStatesDirty)
            return;
        actorLightRenderStates.Clear();
        if (previewService is not null && CurrentMapData?["actors"] is JsonObject groups
            && CurrentMapData["layerOrder"] is JsonArray order)
        {
            Dictionary<string, ActorLightDescriptor?> sharedLights = new(StringComparer.Ordinal);
            using IDisposable resolutionBatch = previewService.BeginResolutionBatch();
            foreach (JsonNode? name in order)
            {
                if (name?.GetValue<string>() is not string layer || groups[layer] is not JsonArray actors)
                    continue;
                foreach (JsonNode? node in actors)
                {
                    if (node is not JsonObject actor)
                        continue;
                    string reference = actor["bp"]?.GetValue<string>() ?? string.Empty;
                    string tag = actor["tag"]?.GetValue<string>() ?? string.Empty;
                    JsonObject? overrides = tag.Length == 0 ? null : CurrentMapData["BPClassVarChanged"]?[tag] as JsonObject;
                    ActorLightDescriptor? light;
                    if (overrides is { Count: > 0 })
                        light = previewService.tryResolveActorLight(reference, overrides);
                    else if (!sharedLights.TryGetValue(reference, out light))
                    {
                        light = previewService.tryResolveActorLight(reference);
                        sharedLights[reference] = light;
                    }
                    if (light is not null)
                        actorLightRenderStates.Add(new ActorLightRenderState(layer, actor, light));
                }
            }
        }
        actorLightRenderStatesDirty = false;
    }

    private void invalidateActorLightRenderStates()
    {
        actorLightRenderStates.Clear();
        actorLightRenderStatesDirty = true;
    }

    private void onActorLightSourcesChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (!args.AffectsSection("Blueprints"))
            return;
        invalidateActorLightRenderStates();
        InvalidateVisual();
    }

    private sealed record ActorLightRenderState(string Layer, JsonObject Actor, ActorLightDescriptor Light);
}
