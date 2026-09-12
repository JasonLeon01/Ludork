using Avalonia;
using Avalonia.Media.Imaging;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class MapPanel
{
    private void reconcileRuntimeActorSelection()
    {
        if (movingRuntimeActorId is not null)
        {
            (string Layer, int Index)? moving = findRuntimeActor(movingRuntimeActorId);
            if (moving is { } candidate && getActorList(candidate.Layer)?[candidate.Index]?["parentRuntimeId"] is not null)
            {
                moving = null;
                capturedPointer?.Capture(null);
            }
            actorMoveLayer = moving?.Layer;
            actorMoveIndex = moving?.Index;
            if (moving is null)
                movingRuntimeActorId = null;
        }
        if (selectedRuntimeActorId is null)
            return;
        (string Layer, int Index)? selected = findRuntimeActor(selectedRuntimeActorId);
        setSelectedActor(selected?.Layer, selected?.Index, true, true);
    }

    private (string Layer, int Index)? findRuntimeActor(string actorId)
    {
        if (CurrentMapData?["actors"] is not JsonObject groups)
            return null;
        foreach (KeyValuePair<string, JsonNode?> group in groups)
        {
            if (group.Value is not JsonArray actors)
                continue;
            for (int index = 0; index < actors.Count; index++)
                if (string.Equals(actors[index]?["runtimeId"]?.GetValue<string>(), actorId, StringComparison.Ordinal))
                    return (group.Key, index);
        }
        return null;
    }

    private (string Layer, int Index)? resolveMapActorHit(string layerName, int? index)
    {
        if (index is not int actorIndex)
            return null;
        (string Layer, int Index) target = (layerName, actorIndex);
        if (!IsRuntimeEditing)
            return target;
        HashSet<string> ancestors = new(StringComparer.Ordinal);
        while (getActorList(target.Layer)?[target.Index]?["parentRuntimeId"]?.GetValue<string>() is string parentId)
        {
            if (!ancestors.Add(parentId) || findRuntimeActor(parentId) is not { } parent)
                return null;
            target = parent;
        }
        return target;
    }

    private ActorVisualDescriptor? resolveRuntimeActorVisual(JsonObject actor)
    {
        if (actor["visual"] is not JsonObject visual
            || visual["texturePath"]?.GetValue<string>() is not string path
            || visual["textureRect"] is not JsonArray rectangle || rectangle.Count != 4
            || getActorBitmap(path) is not Bitmap texture)
            return null;
        PixelRect rect = new(readRuntimeInt(rectangle[0]), readRuntimeInt(rectangle[1]),
            readRuntimeInt(rectangle[2]), readRuntimeInt(rectangle[3]));
        return new ActorVisualDescriptor(
            actor["bp"]?.GetValue<string>() ?? actor["type"]?.GetValue<string>() ?? string.Empty,
            path,
            texture.PixelSize,
            rect,
            visual["shaderPath"]?.GetValue<string>() ?? string.Empty,
            getDouble(visual["hue"], 0),
            readRuntimeVector(visual["translation"], default),
            readRuntimeVector(visual["scale"], new Vector(1, 1)),
            readRuntimeVector(visual["origin"], default),
            getDouble(visual["rotation"], 0),
            visual["visible"]?.GetValue<bool>() ?? true,
            false,
            false,
            0.2,
            1);
    }

    private static Vector readRuntimeVector(JsonNode? value, Vector fallback)
        => value is JsonArray entries && entries.Count == 2
            ? new Vector(getDouble(entries[0], fallback.X), getDouble(entries[1], fallback.Y))
            : fallback;

    private static int readRuntimeInt(JsonNode? value) => tryGetInt(value, out int result) ? result : 0;
}
