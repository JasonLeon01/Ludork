using Avalonia;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class MapPanel
{
    private const double ActorRotationWheelStep = 15;
    private const double ActorRotationMagnifyDegrees = 180;
    private readonly DispatcherTimer propertyWheelTimer = new() { Interval = TimeSpan.FromMilliseconds(300) };
    private ActorPropertyDrag? actorPropertyDrag;
    private long propertyWheelTimestamp;
    private (string Layer, string Selection, string Property)? propertyWheelTarget;
    internal event EventHandler? ActorPropertiesChanged;

    private sealed record EditableActor(MapActorReference Reference, MapActorSnapshot Actor, ResolvedBlueprintClass Class);
    private sealed record ActorPropertyDragTarget(MapActorReference Reference, Vector Value, Matrix Inverse);
    private sealed record ActorPropertyDrag(Point Start, string Property, IReadOnlyList<ActorPropertyDragTarget> Targets);

    private bool tryGetEditableActorClasses(out IReadOnlyList<EditableActor> actors)
    {
        List<EditableActor> result = [];
        actors = result;
        if (IsRuntimeEditing || !canEditMap || CurrentMapDocument is null || CurrentMapKey is null
            || selectedLayerName is null || getLayer(selectedLayerName) is not MapLayerSnapshot layer
            || !isLayerVisible(layer) || selectedActors.Count == 0)
            return false;
        foreach (MapActorReference reference in selectedActors)
        {
            if (reference.LayerName != selectedLayerName || findProjectActor(reference) is not int index
                || getActorList(reference.LayerName)?[index] is not MapActorSnapshot actor
                || previewService?.tryResolveMapActorClass(CurrentMapDocument, actor) is not ResolvedBlueprintClass resolved)
                return false;
            result.Add(new(reference, actor, resolved));
        }
        return true;
    }

    private bool beginActorPropertyDrag(Point start, KeyModifiers modifiers)
    {
        if (!tryGetEditableActorClasses(out IReadOnlyList<EditableActor> actors))
            return false;
        bool origin = modifiers.HasFlag(KeyModifiers.Alt);
        string property = origin ? "defaultOrigin" : "defaultTranslation";
        double displayScale = tileSize / (double)SourceTileSize;
        List<ActorPropertyDragTarget> targets = [];
        foreach (EditableActor actor in actors)
        {
            if (!tryGetPropertyVector(actor.Class.GetField(property), out Vector value))
                return false;
            Matrix transform = Matrix.CreateScale(displayScale, displayScale);
            if (origin)
            {
                if (!tryGetPropertyVector(actor.Class.GetField("defaultScale"), out Vector scale))
                    return false;
                double radians = getDouble(actor.Class.GetValue("defaultRotation"), 0) * Math.PI / 180;
                if (!double.IsFinite(radians))
                    return false;
                transform = new Matrix(
                    Math.Cos(radians) * scale.X * displayScale, Math.Sin(radians) * scale.X * displayScale,
                    -Math.Sin(radians) * scale.Y * displayScale, Math.Cos(radians) * scale.Y * displayScale, 0, 0);
            }
            if (!transform.TryInvert(out Matrix inverse))
                return false;
            targets.Add(new(actor.Reference, value, inverse));
        }
        actorPropertyDrag = new(start, property, targets);
        return true;
    }

    private void updateActorPropertyDrag(Point position)
    {
        if (actorPropertyDrag is not ActorPropertyDrag drag)
            return;
        if (!tryGetEditableActorClasses(out IReadOnlyList<EditableActor> actors)
            || !actors.Select(actor => actor.Reference).SequenceEqual(drag.Targets.Select(target => target.Reference)))
        {
            cancelMapGesture();
            return;
        }
        List<MapActorOverrideEdit> edits = [];
        Vector delta = position - drag.Start;
        for (int index = 0; index < actors.Count; index++)
        {
            ActorPropertyDragTarget target = drag.Targets[index];
            Point local = target.Inverse.Transform(new Point(delta.X, delta.Y));
            Vector value = target.Value + new Vector(local.X, local.Y);
            if (!isFinite(value))
                return;
            edits.Add(createActorPropertyEdit(actors[index], drag.Property, new JsonArray(value.X, value.Y)));
        }
        ensurePropertyHistoryGesture();
        applyActorPropertyEdits(edits);
    }

    private bool tryAdjustSelectedProperty(KeyModifiers modifiers, double factor, double rotationDelta)
    {
        bool rotation = modifiers.HasFlag(KeyModifiers.Alt);
        if (!rotation && !EditorZoomInput.HasPrimaryModifier(modifiers)
            || EditMode != MapEditMode.Actor
            || !tryGetEditableActorClasses(out IReadOnlyList<EditableActor> actors))
            return false;
        if (actorPointerGesture is not null || actorPropertyDrag is not null || actorMoveIndex is not null)
            return true;
        if (rotation ? !double.IsFinite(rotationDelta) || rotationDelta == 0
            : !double.IsFinite(factor) || factor <= 0 || factor == 1)
            return true;
        string property = rotation ? "defaultRotation" : "defaultScale";
        List<MapActorOverrideEdit> edits = [];
        foreach (EditableActor actor in actors)
        {
            JsonNode value;
            if (rotation)
            {
                double current = getDouble(actor.Class.GetValue(property), double.NaN);
                if (!double.IsFinite(current) || !double.IsFinite(current + rotationDelta))
                    return false;
                value = JsonValue.Create(current + rotationDelta)!;
            }
            else
            {
                if (!tryGetPropertyVector(actor.Class.GetField(property), out Vector scale) || !isFinite(scale * factor))
                    return false;
                Vector next = scale * factor;
                value = new JsonArray(next.X, next.Y);
            }
            edits.Add(createActorPropertyEdit(actor, property, value));
        }
        string selection = new JsonArray(selectedActors.OrderBy(actor => actor.Tag, StringComparer.Ordinal)
            .Select(actor => (JsonNode?)JsonValue.Create(actor.Tag)).ToArray()).ToJsonString();
        beginPropertyWheelGesture((selectedLayerName!, selection, property));
        applyActorPropertyEdits(edits);
        return true;
    }

    private static MapActorOverrideEdit createActorPropertyEdit(EditableActor actor, string property, JsonNode value)
    {
        ResolvedBlueprintField? field = actor.Class.GetField(property);
        bool remove = field?.HasBlueprintDefaultValue == true && JsonNode.DeepEquals(field.BlueprintDefaultValue, value);
        return new(actor.Reference, property, value, remove);
    }

    private void applyActorPropertyEdits(IReadOnlyList<MapActorOverrideEdit> edits)
    {
        if (gameData is not null && CurrentMapKey is not null
            && gameData.Maps.ApplyMapActorOverrides(CurrentMapKey, edits))
            ActorPropertiesChanged?.Invoke(this, EventArgs.Empty);
    }

    private void ensurePropertyHistoryGesture()
    {
        if (gameData is not null && !gameData.IsHistoryGestureActive(mapEditGesture))
            mapEditGesture = gameData.BeginHistoryGesture();
    }

    private void beginPropertyWheelGesture((string Layer, string Selection, string Property) target)
    {
        Focus();
        if (propertyWheelTarget != target
            || Stopwatch.GetElapsedTime(propertyWheelTimestamp) >= propertyWheelTimer.Interval)
            endMapGesture();
        ensurePropertyHistoryGesture();
        propertyWheelTarget = target;
        propertyWheelTimestamp = Stopwatch.GetTimestamp();
        propertyWheelTimer.Stop();
        propertyWheelTimer.Start();
    }

    private void onPropertyInputLostFocus(object? sender, RoutedEventArgs args)
    {
        if (actorPropertyDrag is not null || propertyWheelTarget is not null)
            CancelInteractions();
    }

    private static bool tryGetPropertyVector(ResolvedBlueprintField? field, out Vector value)
    {
        value = default;
        if (field?.Value is not JsonArray array || array.Count < 2)
            return false;
        value = new Vector(getDouble(array[0], double.NaN), getDouble(array[1], double.NaN));
        return isFinite(value);
    }

    private static bool isFinite(Vector value) => double.IsFinite(value.X) && double.IsFinite(value.Y);
}
