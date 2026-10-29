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
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class MapPanel
{
    private const double ActorRotationWheelStep = 15;
    private const double ActorRotationMagnifyDegrees = 180;
    private readonly DispatcherTimer propertyWheelTimer = new() { Interval = TimeSpan.FromMilliseconds(300) };
    private ActorPropertyDrag? actorPropertyDrag;
    private long propertyWheelTimestamp;
    private ActorPropertySession? propertyWheelTarget;
    private long actorPropertyRevision;

    private readonly record struct ActorPropertyValue(Vector Vector, double Number);

    private sealed class ActorPropertyTarget(MapActorReference reference, ActorPropertyValue value, Matrix inverse,
        ResolvedBlueprintField field, bool hasOverride, JsonNode? overrideValue)
    {
        public MapActorReference Reference { get; } = reference;
        public ActorPropertyValue InitialValue { get; } = value;
        public ActorPropertyValue Value { get; set; } = value;
        public Matrix Inverse { get; } = inverse;
        public bool HasDefault { get; } = field.HasBlueprintDefaultValue;
        public JsonNode? DefaultValue { get; } = field.BlueprintDefaultValue?.DeepClone();
        public bool HasOverride { get; set; } = hasOverride;
        public JsonNode? OverrideValue { get; set; } = overrideValue?.DeepClone();
    }

    private sealed record ActorPropertySession(MapDocumentSnapshot Document, string MapKey, string Layer,
        string Property, long SelectionRevision, long PropertyRevision, int TileSize,
        (long Resolver, long Metadata) ResolutionVersion, IReadOnlyList<ActorPropertyTarget> Targets);

    private sealed class ActorPropertyDrag(Point start, ActorPropertySession session)
    {
        public Point Start { get; } = start;
        public ActorPropertySession Session { get; } = session;
        public Point? LastAppliedPosition { get; set; }
    }

    private bool canAdjustActorProperties()
    {
        return !IsRuntimeEditing && canEditMap && EditMode == MapEditMode.Actor && CurrentMapDocument is not null
            && CurrentMapKey is not null && selectedLayerName is not null
            && getLayer(selectedLayerName) is MapLayerSnapshot layer && isLayerVisible(layer)
            && selectedActors.Count != 0;
    }

    private ActorPropertySession? createActorPropertySession(string property)
    {
        if (!canAdjustActorProperties() || previewService is null
            || getActorList(selectedLayerName!) is not IReadOnlyList<MapActorSnapshot> actors)
            return null;
        MapDocumentSnapshot document = CurrentMapDocument!;
        Dictionary<string, MapActorSnapshot?> actorsByTag = new(actors.Count, StringComparer.Ordinal);
        foreach (MapActorSnapshot actor in actors)
            if (!actorsByTag.TryAdd(actor.Tag, actor))
                actorsByTag[actor.Tag] = null;
        List<ActorPropertyTarget> targets = new(selectedActors.Count);
        double displayScale = tileSize / (double)SourceTileSize;
        using IDisposable batch = previewService.BeginResolutionBatch();
        foreach (MapActorReference reference in selectedActors)
        {
            if (reference.LayerName != selectedLayerName || !actorsByTag.TryGetValue(reference.Tag, out MapActorSnapshot? actor)
                || actor is null || previewService.tryResolveMapActorClass(document, actor) is not ResolvedBlueprintClass resolved
                || resolved.GetField(property) is not ResolvedBlueprintField field)
                return null;
            ActorPropertyValue value;
            if (property == "defaultRotation")
            {
                double number = getDouble(field.Value, double.NaN);
                if (!double.IsFinite(number))
                    return null;
                value = new(default, number);
            }
            else
            {
                if (!tryGetPropertyVector(field, out Vector vector))
                    return null;
                value = new(vector, 0);
            }
            Matrix transform = Matrix.CreateScale(displayScale, displayScale);
            if (property == "defaultOrigin")
            {
                if (!tryGetPropertyVector(resolved.GetField("defaultScale"), out Vector scale))
                    return null;
                double radians = getDouble(resolved.GetField("defaultRotation")?.Value, 0) * Math.PI / 180;
                if (!double.IsFinite(radians))
                    return null;
                transform = new Matrix(
                    Math.Cos(radians) * scale.X * displayScale, Math.Sin(radians) * scale.X * displayScale,
                    -Math.Sin(radians) * scale.Y * displayScale, Math.Cos(radians) * scale.Y * displayScale, 0, 0);
            }
            if (!transform.TryInvert(out Matrix inverse))
                return null;
            JsonObject? overrides = document.Data["BPClassVarChanged"]?[reference.Tag] as JsonObject;
            bool hasOverride = overrides?.ContainsKey(property) == true;
            targets.Add(new(reference, value, inverse, field, hasOverride, overrides?[property]));
        }
        return new(document, CurrentMapKey!, selectedLayerName!, property, actorSelectionRevision,
            actorPropertyRevision, tileSize, previewService.ResolutionVersion, targets);
    }

    private bool isActorPropertySessionCurrent(ActorPropertySession session)
    {
        return canAdjustActorProperties() && ReferenceEquals(CurrentMapDocument, session.Document)
            && CurrentMapKey == session.MapKey && selectedLayerName == session.Layer
            && actorSelectionRevision == session.SelectionRevision && actorPropertyRevision == session.PropertyRevision
            && tileSize == session.TileSize && previewService?.ResolutionVersion == session.ResolutionVersion;
    }

    private bool beginActorPropertyDrag(Point start, KeyModifiers modifiers)
    {
        string property = modifiers.HasFlag(KeyModifiers.Alt) ? "defaultOrigin" : "defaultTranslation";
        ActorPropertySession? session = createActorPropertySession(property);
        if (session is null)
            return false;
        actorPropertyDrag = new(start, session);
        return true;
    }

    private void updateActorPropertyDrag(Point position)
    {
        if (actorPropertyDrag is not ActorPropertyDrag drag)
            return;
        if (!isActorPropertySessionCurrent(drag.Session))
        {
            cancelMapGesture();
            return;
        }
        if (drag.LastAppliedPosition == position)
            return;
        ActorPropertyValue[] values = new ActorPropertyValue[drag.Session.Targets.Count];
        Vector delta = position - drag.Start;
        for (int index = 0; index < values.Length; index++)
        {
            ActorPropertyTarget target = drag.Session.Targets[index];
            Point local = target.Inverse.Transform(new Point(delta.X, delta.Y));
            Vector value = target.InitialValue.Vector + new Vector(local.X, local.Y);
            if (!isFinite(value))
                return;
            values[index] = new(value, 0);
        }
        if (applyActorPropertyValues(drag.Session, values) && ReferenceEquals(actorPropertyDrag, drag))
            drag.LastAppliedPosition = position;
    }

    private bool tryAdjustSelectedProperty(KeyModifiers modifiers, double factor, double rotationDelta)
    {
        bool rotation = modifiers.HasFlag(KeyModifiers.Alt);
        if (!rotation && !EditorZoomInput.HasPrimaryModifier(modifiers) || !canAdjustActorProperties())
            return false;
        if (actorPointerGesture is not null || actorPropertyDrag is not null || actorMoveIndex is not null)
            return true;
        if (rotation ? !double.IsFinite(rotationDelta) || rotationDelta == 0
            : !double.IsFinite(factor) || factor <= 0 || factor == 1)
            return true;
        Focus();
        string property = rotation ? "defaultRotation" : "defaultScale";
        ActorPropertySession? session = propertyWheelTarget;
        if (session is null || session.Property != property || !isActorPropertySessionCurrent(session)
            || Stopwatch.GetElapsedTime(propertyWheelTimestamp) >= propertyWheelTimer.Interval)
        {
            endMapGesture();
            session = createActorPropertySession(property);
            if (session is null)
                return false;
        }
        ActorPropertyValue[] values = new ActorPropertyValue[session.Targets.Count];
        for (int index = 0; index < values.Length; index++)
        {
            ActorPropertyValue current = session.Targets[index].Value;
            ActorPropertyValue next = rotation
                ? new(default, current.Number + rotationDelta)
                : new(current.Vector * factor, 0);
            if (rotation ? !double.IsFinite(next.Number) : !isFinite(next.Vector))
                return false;
            values[index] = next;
        }
        beginPropertyWheelGesture(session);
        applyActorPropertyValues(session, values);
        return true;
    }

    private bool applyActorPropertyValues(ActorPropertySession session, IReadOnlyList<ActorPropertyValue> values)
    {
        MapActorOverrideEdit[] edits = new MapActorOverrideEdit[values.Count];
        bool changed = false;
        for (int index = 0; index < values.Count; index++)
        {
            ActorPropertyTarget target = session.Targets[index];
            ActorPropertyValue value = values[index];
            JsonNode node = session.Property == "defaultRotation"
                ? JsonValue.Create(value.Number)!
                : new JsonArray(value.Vector.X, value.Vector.Y);
            bool remove = target.HasDefault && JsonNode.DeepEquals(target.DefaultValue, node);
            edits[index] = new(target.Reference, session.Property, node, remove);
            changed |= remove ? target.HasOverride
                : !target.HasOverride || !JsonNode.DeepEquals(target.OverrideValue, node);
        }
        if (!changed)
            return true;
        ensurePropertyHistoryGesture();
        if (gameData is null || !gameData.Maps.ApplyMapActorOverrides(session.MapKey, edits, session)
            || !isActorPropertySessionCurrent(session))
        {
            cancelMapGesture();
            return false;
        }
        for (int index = 0; index < edits.Length; index++)
        {
            ActorPropertyTarget target = session.Targets[index];
            target.Value = values[index];
            target.HasOverride = !edits[index].Remove;
            target.OverrideValue = edits[index].Remove ? null : edits[index].Value;
        }
        return true;
    }

    private void invalidateActorPropertySession(MapDataEditedEventArgs edit)
    {
        ActorPropertySession? session = actorPropertyDrag?.Session ?? propertyWheelTarget;
        if (session is not null && ReferenceEquals(edit.Origin, session))
            return;
        actorPropertyRevision++;
        if (session is not null)
            cancelMapGesture();
    }

    private void ensurePropertyHistoryGesture()
    {
        if (gameData is not null && !gameData.IsHistoryGestureActive(mapEditGesture))
            mapEditGesture = gameData.BeginHistoryGesture();
    }

    private void beginPropertyWheelGesture(ActorPropertySession target)
    {
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
