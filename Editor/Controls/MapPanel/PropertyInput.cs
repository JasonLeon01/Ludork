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
    private (string? Layer, string? Tag, int? Light, string Property)? propertyWheelTarget;
    internal event EventHandler? ActorPropertiesChanged;

    private sealed record ActorPropertyDrag(
        string Layer, string Tag, string Property, Point Start, Vector Value, Matrix Inverse);

    private bool tryGetEditableActorClass(out MapActorSnapshot actor, out ResolvedBlueprintClass resolved)
    {
        actor = null!;
        resolved = null!;
        if (IsRuntimeEditing || !canEditMap || CurrentMapDocument is null || CurrentMapKey is null
            || selectedActorLayer is null || selectedActorLayer != selectedLayerName
            || getLayer(selectedActorLayer) is not MapLayerSnapshot layer || !isLayerVisible(layer)
            || getSelectedActor() is not MapActorSnapshot selected
            || selected.Tag is not { Length: > 0 }
            || previewService?.tryResolveMapActorClass(CurrentMapDocument, selected) is not ResolvedBlueprintClass value)
            return false;
        actor = selected;
        resolved = value;
        return true;
    }

    private bool beginActorPropertyDrag(PointerPressedEventArgs args)
    {
        if (IsRuntimeEditing || !args.KeyModifiers.HasFlag(KeyModifiers.Alt)
            && !EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
            return false;
        if (!tryGetEditableActorClass(out MapActorSnapshot actor, out ResolvedBlueprintClass resolved))
            return true;
        bool origin = args.KeyModifiers.HasFlag(KeyModifiers.Alt);
        string property = origin ? "defaultOrigin" : "defaultTranslation";
        if (!tryGetPropertyVector(resolved.GetField(property), out Vector value))
            return true;
        double displayScale = tileSize / (double)SourceTileSize;
        Matrix transform = Matrix.CreateScale(displayScale, displayScale);
        if (origin)
        {
            if (!tryGetPropertyVector(resolved.GetField("defaultScale"), out Vector scale))
                return true;
            double radians = getDouble(resolved.GetValue("defaultRotation"), 0) * Math.PI / 180;
            if (!double.IsFinite(radians))
                return true;
            transform = new Matrix(
                Math.Cos(radians) * scale.X * displayScale, Math.Sin(radians) * scale.X * displayScale,
                -Math.Sin(radians) * scale.Y * displayScale, Math.Cos(radians) * scale.Y * displayScale, 0, 0);
        }
        if (!transform.TryInvert(out Matrix inverse))
            return true;
        actorPropertyDrag = new ActorPropertyDrag(selectedActorLayer!, actor.Tag,
            property, args.GetPosition(this), value, inverse);
        capturePointer(args.Pointer);
        return true;
    }

    private void updateActorPropertyDrag(Point position)
    {
        if (actorPropertyDrag is not ActorPropertyDrag drag)
            return;
        if (!tryGetEditableActorClass(out MapActorSnapshot actor, out ResolvedBlueprintClass resolved)
            || selectedActorLayer != drag.Layer || actor.Tag != drag.Tag)
        {
            cancelMapGesture();
            return;
        }
        Vector delta = position - drag.Start;
        Point local = drag.Inverse.Transform(new Point(delta.X, delta.Y));
        Vector value = drag.Value + new Vector(local.X, local.Y);
        if (!isFinite(value) || !tryGetPropertyVector(resolved.GetField(drag.Property), out Vector current)
            || value == current)
            return;
        ensurePropertyHistoryGesture();
        setActorProperty(actor, drag.Property, new JsonArray(value.X, value.Y));
    }

    private bool tryAdjustSelectedProperty(KeyModifiers modifiers, double factor, double rotationDelta)
    {
        if (EditMode == MapEditMode.Actor && modifiers.HasFlag(KeyModifiers.Alt))
            return tryRotateSelectedActor(rotationDelta);
        if (!EditorZoomInput.HasPrimaryModifier(modifiers) || IsRuntimeEditing
            || editingContext?.IsEditable != true || gameData is null || CurrentMapKey is null)
            return false;
        if (EditMode == MapEditMode.Light && selectedActorIndex is null
            && selectedLightIndex is int index && CurrentMapDocument?.Lights is IReadOnlyList<MapLightSnapshot> lights
            && index >= 0 && index < lights.Count && lights[index] is MapLightSnapshot light)
        {
            double radius = light.Radius;
            if (!double.IsFinite(radius) || radius < 0)
                return false;
            if (!canScaleProperty(factor))
                return true;
            double nextRadius = Math.Max(1, Math.Max(1, radius) * factor);
            if (!double.IsFinite(nextRadius) || nextRadius == radius)
                return true;
            beginPropertyWheelGesture((null, null, index, "radius"));
            if (gameData.Maps.UpdateMapLight(CurrentMapKey, index, light, radius: nextRadius)
                && getLight(index) is MapLightSnapshot changedLight)
                LightDataChanged?.Invoke(this, new LightDataChangedEventArgs(CurrentMapKey, index, changedLight.ToJson()));
            return true;
        }
        if (EditMode != MapEditMode.Actor && (EditMode != MapEditMode.Light || !LightActorSelectionEnabled)
            || !tryGetEditableActorClass(out MapActorSnapshot actor, out ResolvedBlueprintClass resolved))
            return false;
        string property;
        JsonNode value;
        if (EditMode == MapEditMode.Actor)
        {
            property = "defaultScale";
            if (!tryGetPropertyVector(resolved.GetField(property), out Vector scale))
                return false;
            if (!canScaleProperty(factor))
                return true;
            Vector next = scale * factor;
            if (!isFinite(next) || next == scale)
                return true;
            value = new JsonArray(next.X, next.Y);
        }
        else
        {
            property = "lightComp";
            if (resolved.GetField(property) is not ResolvedBlueprintField field || field.Value is not JsonObject component
                || previewService?.tryResolveActorLight(actor.Blueprint,
                    CurrentMapDocument?.ReadActorOverrides(actor.Tag)) is not ActorLightDescriptor descriptor)
                return false;
            if (!canScaleProperty(factor))
                return true;
            double nextRadius = Math.Max(1, descriptor.Radius * factor);
            if (!double.IsFinite(nextRadius) || nextRadius == descriptor.Radius)
                return true;
            JsonObject next = field.BlueprintDefaultValue?.DeepClone() as JsonObject ?? [];
            foreach (KeyValuePair<string, JsonNode?> entry in component)
                next[entry.Key] = entry.Value?.DeepClone();
            next["lightRadius"] = nextRadius;
            value = next;
        }
        beginPropertyWheelGesture((selectedActorLayer, actor.Tag, null, property));
        setActorProperty(actor, property, value);
        return true;
    }

    private bool tryRotateSelectedActor(double delta)
    {
        if (!tryGetEditableActorClass(out MapActorSnapshot actor, out ResolvedBlueprintClass resolved))
            return false;
        const string property = "defaultRotation";
        double rotation = getDouble(resolved.GetValue(property), double.NaN);
        if (!double.IsFinite(rotation))
            return false;
        if (!double.IsFinite(delta) || delta == 0
            || actorPropertyDrag is not null || actorMoveIndex is not null)
            return true;
        double next = rotation + delta;
        if (!double.IsFinite(next) || next == rotation)
            return true;
        beginPropertyWheelGesture((selectedActorLayer, actor.Tag, null, property));
        setActorProperty(actor, property, JsonValue.Create(next)!);
        return true;
    }

    private bool canScaleProperty(double factor)
    {
        return double.IsFinite(factor) && factor > 0 && factor != 1
            && actorPropertyDrag is null && actorMoveIndex is null && !lightMoveDragging && !lightRadiusDragging;
    }

    private void setActorProperty(MapActorSnapshot actor, string property, JsonNode value)
    {
        if (CurrentMapKey is null || selectedActorLayer is null)
            return;
        if (editingContext?.SetActorVariable(CurrentMapKey, selectedActorLayer,
                actor.Tag, property, value) == true)
            ActorPropertiesChanged?.Invoke(this, EventArgs.Empty);
    }

    private void ensurePropertyHistoryGesture()
    {
        if (gameData is not null && !gameData.IsHistoryGestureActive(mapEditGesture))
            mapEditGesture = gameData.BeginHistoryGesture();
    }

    private void beginPropertyWheelGesture((string? Layer, string? Tag, int? Light, string Property) target)
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
