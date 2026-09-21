using Avalonia;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Models;
using System;
using System.Linq;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class MapPanel
{
    private static readonly Pen ActorLightPen = new(Brushes.Gray, 2, DashStyle.Dash);
    private static readonly Pen SelectedActorLightPen = new(Brushes.Gold, 2, DashStyle.Dash);
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
            if (getLayer(state.Layer) is not MapLayerSnapshot layer || !isLayerVisible(layer)
                || !tryGetActorPosition(state.Actor, out int x, out int y))
                continue;
            ActorLightDescriptor light = state.Light;
            Point center = createActorTransform(x, y, light.Translation, light.Scale, light.Rotation)
                .Transform(light.LocalPosition);
            center = new Point(snapToDevicePixel(center.X), snapToDevicePixel(center.Y));
            double radius = light.Radius * displayScale;
            bool selected = selectedActorLayer == state.Layer && ReferenceEquals(getSelectedActor(), state.Actor);
            context.DrawEllipse(new SolidColorBrush(getLightFill(light.Colour)), selected ? SelectedActorLightPen : ActorLightPen, center, radius, radius);
            context.DrawEllipse(selected ? Brushes.Gold : Brushes.Gray, null, center, 3, 3);
        }
    }

    private int? hitTestLightActor(Point position, Point basePosition, int width, int height, out int? fixedLight)
    {
        fixedLight = hitTestLight(basePosition);
        if (!LightActorSelectionEnabled || !canEditMap || selectedLayerName is null || IsRuntimeEditing)
            return null;
        ensureActorLightRenderStates();
        HashSet<MapActorSnapshot> allowedActors = [];
        foreach (ActorLightRenderState state in actorLightRenderStates)
            if (state.Layer == selectedLayerName)
                allowedActors.Add(state.Actor);
        Rect mapRect = getMapRect(width, height);
        Point displayPosition = new(position.X - mapRect.X, position.Y - mapRect.Y);
        if (hitTestActor(selectedLayerName, displayPosition, allowedActors) is int bodyHit)
        {
            fixedLight = null;
            return bodyHit;
        }
        if (selectedLightIndex is int selectedIndex
            && CurrentMapDocument?.Lights is IReadOnlyList<MapLightSnapshot> lights
            && selectedIndex >= 0 && selectedIndex < lights.Count
            && lights[selectedIndex] is MapLightSnapshot selectedLight
            && tryGetLight(selectedLight, out Point selectedCenter, out double selectedRadius)
            && Math.Abs(getDistance(basePosition, selectedCenter) - selectedRadius) <= LightEdgeTolerance)
        {
            fixedLight = selectedIndex;
            return null;
        }
        double displayScale = tileSize / (double)SourceTileSize;
        double bestDistance = double.MaxValue;
        if (fixedLight is int fixedIndex && getLight(fixedIndex) is MapLightSnapshot mapLight
            && tryGetLight(mapLight, out Point fixedCenter, out _))
            bestDistance = getDistance(basePosition, fixedCenter) * displayScale;
        int? bestActor = null;
        for (int index = actorLightRenderStates.Count - 1; index >= 0; index--)
        {
            ActorLightRenderState state = actorLightRenderStates[index];
            if (state.Layer != selectedLayerName || !tryGetActorPosition(state.Actor, out int x, out int y))
                continue;
            ActorLightDescriptor light = state.Light;
            Point center = createActorTransform(x, y, light.Translation, light.Scale, light.Rotation)
                .Transform(light.LocalPosition);
            double distance = getDistance(displayPosition, center);
            if (distance > light.Radius * displayScale || distance >= bestDistance)
                continue;
            bestDistance = distance;
            bestActor = getActorList(state.Layer)?.ToList().IndexOf(state.Actor);
        }
        if (bestActor is not null)
            fixedLight = null;
        return bestActor;
    }

    private void reconcileLightActorSelection()
    {
        if (EditMode != MapEditMode.Light || selectedActorIndex is null)
            return;
        ensureActorLightRenderStates();
        MapActorSnapshot? selected = getSelectedActor();
        foreach (ActorLightRenderState state in actorLightRenderStates)
            if (state.Layer == selectedLayerName && ReferenceEquals(state.Actor, selected))
                return;
        cancelMapGesture();
        setSelectedActor(null, null, true);
    }

    private void ensureActorLightRenderStates()
    {
        if (!actorLightRenderStatesDirty)
            return;
        actorLightRenderStates.Clear();
        if (previewService is not null && CurrentMapDocument is MapDocumentSnapshot map)
        {
            Dictionary<string, ActorLightDescriptor?> sharedLights = new(StringComparer.Ordinal);
            using IDisposable resolutionBatch = previewService.BeginResolutionBatch();
            foreach (string layer in map.LayerOrder)
            {
                if (!map.Actors.TryGetValue(layer, out IReadOnlyList<MapActorSnapshot>? actors))
                    continue;
                foreach (MapActorSnapshot actor in actors)
                {
                    string reference = actor.Blueprint;
                    string tag = actor.Tag;
                    JsonObject? overrides = tag.Length == 0 ? null : CurrentMapDocument.ReadActorOverrides(tag);
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
        reconcileLightActorSelection();
        InvalidateVisual();
    }

    private sealed record ActorLightRenderState(string Layer, MapActorSnapshot Actor, ActorLightDescriptor Light);
}
