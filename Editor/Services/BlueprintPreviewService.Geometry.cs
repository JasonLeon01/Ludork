using Avalonia;
using Ludork.Models;
using System;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class BlueprintPreviewService
{
    private Size getActorBounds(ResolvedBlueprintClass resolved)
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
}
