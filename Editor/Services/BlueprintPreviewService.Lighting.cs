using Avalonia;
using Avalonia.Media;
using Ludork.Models;
using System;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class BlueprintPreviewService
{
    public ActorLightDescriptor? tryResolveActorLight(string blueprintReference, JsonObject? overrides = null)
    {
        const string prefix = "Data.Blueprints.";
        if (blueprintReference.StartsWith(prefix, StringComparison.Ordinal)
            && !gameData.Blueprints.BlueprintsData.ContainsKey(blueprintReference[prefix.Length..].Replace('.', '/')))
            return null;
        ResolvedBlueprintClass resolved = classResolver.Resolve(blueprintReference, overrides);
        ResolvedBlueprintField? field = resolved.GetField("lightComp");
        if (field?.Value is not JsonObject light || !getBool(getResolvedValue(resolved, "visible"), true))
            return null;
        JsonObject? defaults = field.BlueprintDefaultValue as JsonObject;
        double radius = getDouble(light["lightRadius"] ?? defaults?["lightRadius"], 16);
        if (!double.IsFinite(radius) || radius <= 0)
            return null;
        (double x, double y) offset = parseVec2(light["lightOffset"] ?? defaults?["lightOffset"], 0, 0);
        (double x, double y) origin = parseVec2(getResolvedValue(resolved, "defaultOrigin"), 0, 0);
        (double x, double y) translation = parseVec2(getResolvedValue(resolved, "defaultTranslation"), 0, 0);
        (double x, double y) scale = parseVec2(getResolvedValue(resolved, "defaultScale"), 1, 1);
        Size bounds = getActorLightBounds(resolved);
        JsonArray? colour = (light["lightColour"] ?? defaults?["lightColour"]) as JsonArray;
        return new ActorLightDescriptor(
            new Point(bounds.Width * 0.5 + offset.x - origin.x, bounds.Height * 0.5 + offset.y - origin.y),
            new Vector(translation.x, translation.y),
            new Vector(scale.x, scale.y),
            getDouble(getResolvedValue(resolved, "defaultRotation"), 0),
            radius,
            Color.FromArgb(getLightChannel(colour, 3), getLightChannel(colour, 0),
                getLightChannel(colour, 1), getLightChannel(colour, 2)));
    }

    private Size getActorLightBounds(ResolvedBlueprintClass resolved)
    {
        bool isCharacter = classResolver.IsDerivedFrom(resolved, "Engine.Character");
        if (!isCharacter && getResolvedValue(resolved, "defaultRect") is JsonArray rect
            && rect.Count > 0 && rect[0] is JsonArray arguments && arguments.Count >= 4)
            return new Size(Math.Abs(getDouble(arguments[2], 0)), Math.Abs(getDouble(arguments[3], 0)));
        string texturePath = getResolvedValue(resolved, "texturePath")?.ToString() ?? string.Empty;
        string filePath = resolveTextureFilePath(texturePath);
        PixelSize? size = File.Exists(filePath) ? getSourceImageSize(filePath) : null;
        if (size is null)
            return default;
        return isCharacter
            ? new Size(size.Value.Width / 4, size.Value.Height / 4)
            : new Size(size.Value.Width, size.Value.Height);
    }

    private static byte getLightChannel(JsonArray? colour, int index)
    {
        return colour is not null && index < colour.Count
            ? (byte)Math.Clamp(getDouble(colour[index], 255), 0, 255)
            : (byte)255;
    }
}
