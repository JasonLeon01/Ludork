using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class BlueprintPreviewService : IDisposable
{
    private const int DefaultTileSize = 32;
    private readonly string projectPath;
    private readonly GameDataService gameData;
    private readonly BlueprintClassResolver classResolver;
    private readonly Dictionary<string, SourceImageInfo> sourceImageInfoCache = new(StringComparer.OrdinalIgnoreCase);

    public BlueprintPreviewService(
        string projectPath,
        GameDataService gameData,
        BlueprintClassResolver classResolver)
    {
        this.projectPath = Path.GetFullPath(projectPath);
        this.gameData = gameData;
        this.classResolver = classResolver;
        ActorPreviews = new ActorPreviewService(this.projectPath);
        gameData.DataReloaded += onVisualSourceDataChanged;
        gameData.DataRestored += onVisualSourceDataChanged;
    }

    public ActorPreviewService ActorPreviews { get; }
    public event EventHandler? VisualsInvalidated;
    public event EventHandler? LiveVisualsInvalidated;

    public IDisposable BeginResolutionBatch()
    {
        return classResolver.BeginBatch();
    }

    public void InvalidateVisuals()
    {
        sourceImageInfoCache.Clear();
        VisualsInvalidated?.Invoke(this, EventArgs.Empty);
        LiveVisualsInvalidated?.Invoke(this, EventArgs.Empty);
    }

    public ActorVisualDescriptor? tryResolveActorVisual(
        string blueprintReference,
        JsonObject? overrides = null)
    {
        const string prefix = "Data.Blueprints.";
        if (!blueprintReference.StartsWith(prefix, StringComparison.Ordinal))
            return createActorVisual(classResolver.Resolve(blueprintReference, overrides), blueprintReference);
        string key = blueprintReference[prefix.Length..].Replace('.', '/');
        return gameData.BlueprintsData.ContainsKey(key)
            ? tryResolveActorVisual(classResolver.Resolve(blueprintReference, overrides), blueprintReference)
            : null;
    }

    public ActorVisualDescriptor? tryResolveActorVisual(
        JsonObject blueprint,
        string? blueprintKey = null,
        JsonObject? overrides = null)
    {
        ResolvedBlueprintClass resolved = classResolver.ResolveBlueprint(blueprint, blueprintKey, overrides);
        string reference = string.IsNullOrWhiteSpace(blueprintKey)
            ? resolved.ClassReference
            : blueprintKey.StartsWith("Data.Blueprints.", StringComparison.Ordinal)
                ? blueprintKey
                : "Data.Blueprints." + blueprintKey.Replace('/', '.').Replace('\\', '.');
        return tryResolveActorVisual(resolved, reference);
    }

    public ActorVisualDescriptor? tryResolveActorVisual(
        ResolvedBlueprintClass resolved,
        string blueprintReference)
    {
        return createActorVisual(resolved, blueprintReference);
    }

    public ActorVisualDescriptor? tryResolveMapActorVisual(JsonObject map, JsonObject actor)
    {
        string reference = actor["bp"]?.GetValue<string>() ?? string.Empty;
        string tag = actor["tag"]?.GetValue<string>() ?? string.Empty;
        JsonObject? overrides = tag.Length == 0 ? null : map["BPClassVarChanged"]?[tag] as JsonObject;
        return tryResolveActorVisual(reference, overrides);
    }

    public void Dispose()
    {
        gameData.DataReloaded -= onVisualSourceDataChanged;
        gameData.DataRestored -= onVisualSourceDataChanged;
        ActorPreviews.Dispose();
        sourceImageInfoCache.Clear();
    }

    private void onVisualSourceDataChanged(object? sender, EventArgs args)
    {
        sourceImageInfoCache.Clear();
        VisualsInvalidated?.Invoke(this, EventArgs.Empty);
    }

    public Bitmap? tryLoadPreview(JsonObject blueprint, int size = 80, string? blueprintKey = null)
    {
        ResolvedBlueprintClass resolved = classResolver.ResolveBlueprint(blueprint, blueprintKey);
        return tryLoadPreview(resolved, size);
    }

    public Bitmap? tryLoadPreview(ResolvedBlueprintClass resolved, int size = 80)
    {
        string? texturePath = getResolvedValue(resolved, "texturePath")?.ToString();
        if (string.IsNullOrWhiteSpace(texturePath))
            return null;

        string filePath = resolveTexturePath(texturePath);
        if (!File.Exists(filePath))
            return null;

        using Bitmap source = new Bitmap(filePath);
        (int sx, int sy, int w, int h)? rect = parseRect(getResolvedValue(resolved, "defaultRect"));
        (double x, double y) origin = parseVec2(getResolvedValue(resolved, "defaultOrigin"), 0, 0);
        (double x, double y) scale = parseVec2(getResolvedValue(resolved, "defaultScale"), 1, 1);
        float hue = parseHue(getResolvedValue(resolved, "hue"));
        if (rect is null)
        {
            bool isCharacter = classResolver.IsDerivedFrom(resolved, "Engine.Character");
            rect = defaultRect(isCharacter, source.PixelSize.Width, source.PixelSize.Height);
            if (rect is null)
                return null;
        }
        (int sx, int sy, int w, int h) rectValue = rect.Value;
        int dw = Math.Max(1, (int)(rectValue.w * scale.x));
        int dh = Math.Max(1, (int)(rectValue.h * scale.y));
        Bitmap? preview = renderCrop(source, rectValue.sx, rectValue.sy, rectValue.w, rectValue.h, origin.x, origin.y, scale.x, scale.y, dw, dh);
        if (preview is null)
            return null;
        if (!isNeutralHue(hue))
        {
            Bitmap huePreview = applyHue(preview, hue);
            preview.Dispose();
            preview = huePreview;
        }
        Bitmap result = scaleToFit(preview, size);
        if (!ReferenceEquals(result, preview))
            preview.Dispose();
        return result;
    }

    public Bitmap? tryLoadPreview(string blueprintReference, int size = 80)
    {
        const string prefix = "Data.Blueprints.";
        if (!blueprintReference.StartsWith(prefix, StringComparison.Ordinal))
            return null;
        string key = blueprintReference[prefix.Length..].Replace('.', '/');
        return gameData.BlueprintsData.ContainsKey(key)
            ? tryLoadPreview(classResolver.Resolve(blueprintReference), size)
            : null;
    }

    public JsonNode? getBlueprintAttr(string blueprintReference, string attrName)
    {
        const string prefix = "Data.Blueprints.";
        if (!blueprintReference.StartsWith(prefix, StringComparison.Ordinal))
            return null;
        string key = blueprintReference[prefix.Length..].Replace('.', '/');
        return gameData.BlueprintsData.ContainsKey(key)
            ? classResolver.Resolve(blueprintReference).GetValue(attrName)
            : null;
    }

    public bool isCharacterBlueprint(string blueprintReference)
    {
        const string characterType = "Engine.Character";
        return classResolver.IsDerivedFrom(blueprintReference, characterType);
    }

    private ActorVisualDescriptor? createActorVisual(
        ResolvedBlueprintClass resolved,
        string blueprintReference)
    {
        string texturePath = getResolvedValue(resolved, "texturePath")?.ToString() ?? string.Empty;
        if (string.IsNullOrWhiteSpace(texturePath))
            return null;
        string filePath = resolveTexturePath(texturePath);
        if (!File.Exists(filePath))
            return null;
        PixelSize? sourceSize = getSourceImageSize(filePath);
        if (sourceSize is null)
            return null;
        PixelSize textureSize = sourceSize.Value;
        bool isCharacter = blueprintReference.Length != 0
            && classResolver.IsDerivedFrom(resolved, "Engine.Character");
        (int sx, int sy, int w, int h)? parsedRect = parseRect(getResolvedValue(resolved, "defaultRect"));
        PixelRect rect;
        bool animated = getBool(getResolvedValue(resolved, "animatable"), false);
        if (isCharacter && textureSize.Width >= 4 && textureSize.Height >= 4)
        {
            int width = Math.Max(1, textureSize.Width / 4);
            int height = Math.Max(1, textureSize.Height / 4);
            int direction = Math.Clamp((int)getDouble(getResolvedValue(resolved, "direction"), 0), 0, 3);
            rect = new PixelRect(0, direction * height, width, height);
            animated = animated && getBool(getResolvedValue(resolved, "animateWithoutMoving"), false);
        }
        else
        {
            (int sx, int sy, int w, int h)? fallback = parsedRect
                ?? defaultRect(false, textureSize.Width, textureSize.Height);
            if (fallback is null)
                return null;
            (int sx, int sy, int w, int h) value = fallback.Value;
            rect = new PixelRect(value.sx, value.sy, value.w, value.h);
        }
        if (rect.X < 0 || rect.Y < 0 || rect.Width <= 0 || rect.Height <= 0
            || rect.Right > textureSize.Width || rect.Bottom > textureSize.Height)
        {
            return null;
        }
        (double x, double y) translation = parseVec2(getResolvedValue(resolved, "defaultTranslation"), 0, 0);
        (double x, double y) scale = parseVec2(getResolvedValue(resolved, "defaultScale"), 1, 1);
        (double x, double y) origin = parseVec2(getResolvedValue(resolved, "defaultOrigin"), 0, 0);
        double interval = Math.Max(0.001, getDouble(getResolvedValue(resolved, "switchInterval"), 0.2));
        int frameCount = Math.Max(1, textureSize.Width / rect.Width);
        return new ActorVisualDescriptor(
            blueprintReference,
            filePath,
            textureSize,
            rect,
            getResolvedValue(resolved, "shaderPath")?.ToString() ?? string.Empty,
            parseHue(getResolvedValue(resolved, "hue")),
            new Vector(translation.x, translation.y),
            new Vector(scale.x, scale.y),
            new Vector(origin.x, origin.y),
            getDouble(getResolvedValue(resolved, "defaultRotation"), 0),
            isCharacter,
            animated,
            interval,
            frameCount);
    }

    private static JsonNode? getResolvedValue(ResolvedBlueprintClass resolved, string attrName)
    {
        return resolved.GetField(attrName)?.Value;
    }

    private static (int sx, int sy, int w, int h)? parseRect(JsonNode? value)
    {
        if (value is not JsonArray values || values.Count == 0
            || values[0] is not JsonArray arguments || arguments.Count < 4)
            return null;
        if (int.TryParse(arguments[0]?.ToString(), out int sx)
            && int.TryParse(arguments[1]?.ToString(), out int sy)
            && int.TryParse(arguments[2]?.ToString(), out int w)
            && int.TryParse(arguments[3]?.ToString(), out int h))
            return (sx, sy, Math.Max(1, w), Math.Max(1, h));
        return null;
    }

    private static (double x, double y) parseVec2(JsonNode? value, double defaultX, double defaultY)
    {
        if (value is JsonArray array && array.Count >= 2
            && double.TryParse(array[0]?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out double x)
            && double.TryParse(array[1]?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out double y))
            return (x, y);
        return (defaultX, defaultY);
    }

    private static float parseHue(JsonNode? value)
    {
        if (value is null)
            return 0;
        return float.TryParse(value.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out float hue)
            ? hue % 360f
            : 0;
    }

    private static double getDouble(JsonNode? value, double fallback)
    {
        return value is not null
            && double.TryParse(value.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out double number)
                ? number
                : fallback;
    }

    private static bool getBool(JsonNode? value, bool fallback)
    {
        if (value is JsonValue scalar && scalar.TryGetValue(out bool boolean))
            return boolean;
        return fallback;
    }

    private static (int sx, int sy, int w, int h)? defaultRect(
        bool isCharacter,
        int imageWidth,
        int imageHeight)
    {
        if (imageWidth <= 0 || imageHeight <= 0)
            return null;
        if (isCharacter)
            return (0, 0, Math.Max(1, imageWidth / 4), Math.Max(1, imageHeight / 4));
        int tile = DefaultTileSize;
        return (0, 0, Math.Min(tile, imageWidth), Math.Min(tile, imageHeight));
    }

    private PixelSize? getSourceImageSize(string filePath)
    {
        FileInfo file = new(filePath);
        if (!file.Exists)
        {
            sourceImageInfoCache.Remove(filePath);
            return null;
        }
        if (sourceImageInfoCache.TryGetValue(filePath, out SourceImageInfo cached)
            && cached.ModifiedAt == file.LastWriteTimeUtc
            && cached.Length == file.Length)
        {
            return cached.Size;
        }
        using Bitmap source = new(filePath);
        PixelSize size = source.PixelSize;
        sourceImageInfoCache[filePath] = new SourceImageInfo(file.LastWriteTimeUtc, file.Length, size);
        return size;
    }

    private static Bitmap? renderCrop(
        Bitmap source,
        int sx,
        int sy,
        int w,
        int h,
        double originX,
        double originY,
        double scaleX,
        double scaleY,
        int dw,
        int dh
    )
    {
        (byte[] pixels, int width, int height, int stride, PixelChannels channels) sourceBuffer =
            readPixelBuffer(source);
        WriteableBitmap result = createBitmap(dw, dh);
        using ILockedFramebuffer frame = result.Lock();
        PixelChannels targetChannels = getPixelChannels(frame.Format);
        byte[] target = new byte[frame.RowBytes * dh];
        for (int py = 0; py < dh; py++)
        {
            for (int px = 0; px < dw; px++)
            {
                int srcX = (int)Math.Floor(sx + (px + originX * scaleX) / scaleX);
                int srcY = (int)Math.Floor(sy + (py + originY * scaleY) / scaleY);
                if (srcX < 0 || srcY < 0 || srcX >= sourceBuffer.width || srcY >= sourceBuffer.height)
                    continue;
                copyPixel(
                    sourceBuffer.pixels,
                    sourceBuffer.stride,
                    sourceBuffer.channels,
                    srcX,
                    srcY,
                    target,
                    frame.RowBytes,
                    targetChannels,
                    px,
                    py);
            }
        }
        Marshal.Copy(target, 0, frame.Address, target.Length);
        return result;
    }

    private static Bitmap scaleToFit(Bitmap source, int size)
    {
        int width = source.PixelSize.Width;
        int height = source.PixelSize.Height;
        int maxDim = Math.Max(width, height);
        if (maxDim <= size)
            return source;

        double scale = size / (double)maxDim;
        int targetWidth = Math.Max(1, (int)Math.Round(width * scale));
        int targetHeight = Math.Max(1, (int)Math.Round(height * scale));
        (byte[] pixels, int width, int height, int stride, PixelChannels channels) sourceBuffer =
            readPixelBuffer(source);
        WriteableBitmap result = createBitmap(targetWidth, targetHeight);
        using ILockedFramebuffer frame = result.Lock();
        PixelChannels targetChannels = getPixelChannels(frame.Format);
        byte[] target = new byte[frame.RowBytes * targetHeight];
        for (int py = 0; py < targetHeight; py++)
        {
            for (int px = 0; px < targetWidth; px++)
            {
                int srcX = Math.Clamp((int)Math.Floor(px / scale), 0, sourceBuffer.width - 1);
                int srcY = Math.Clamp((int)Math.Floor(py / scale), 0, sourceBuffer.height - 1);
                copyPixel(
                    sourceBuffer.pixels,
                    sourceBuffer.stride,
                    sourceBuffer.channels,
                    srcX,
                    srcY,
                    target,
                    frame.RowBytes,
                    targetChannels,
                    px,
                    py);
            }
        }
        Marshal.Copy(target, 0, frame.Address, target.Length);
        return result;
    }

    private static WriteableBitmap createBitmap(int width, int height)
    {
        return new WriteableBitmap(
            new PixelSize(width, height),
            new Vector(96, 96),
            PixelFormat.Bgra8888,
            AlphaFormat.Unpremul
        );
    }

    private static (byte[] pixels, int width, int height, int stride, PixelChannels channels) readPixelBuffer(
        Bitmap bitmap)
    {
        int width = bitmap.PixelSize.Width;
        int height = bitmap.PixelSize.Height;
        using WriteableBitmap buffer = createBitmap(width, height);
        using ILockedFramebuffer frame = buffer.Lock();
        int bufferSize = frame.RowBytes * height;
        bitmap.CopyPixels(frame);
        byte[] pixels = new byte[bufferSize];
        Marshal.Copy(frame.Address, pixels, 0, bufferSize);
        return (pixels, width, height, frame.RowBytes, getPixelChannels(frame.Format));
    }

    private static void copyPixel(
        byte[] source,
        int sourceStride,
        PixelChannels sourceChannels,
        int sourceX,
        int sourceY,
        byte[] target,
        int targetStride,
        PixelChannels targetChannels,
        int targetX,
        int targetY
    )
    {
        int sourceIndex = sourceY * sourceStride + sourceX * 4;
        int targetIndex = targetY * targetStride + targetX * 4;
        target[targetIndex + targetChannels.Red] = source[sourceIndex + sourceChannels.Red];
        target[targetIndex + targetChannels.Green] = source[sourceIndex + sourceChannels.Green];
        target[targetIndex + targetChannels.Blue] = source[sourceIndex + sourceChannels.Blue];
        target[targetIndex + targetChannels.Alpha] = source[sourceIndex + sourceChannels.Alpha];
    }

    private static bool isNeutralHue(float hue) => hue <= 0.0001f || Math.Abs(hue - 360f) <= 0.0001f;

    private static Bitmap applyHue(Bitmap source, float hue)
    {
        WriteableBitmap writeable = new WriteableBitmap(source.PixelSize, source.Dpi, PixelFormat.Bgra8888, AlphaFormat.Unpremul);
        using (ILockedFramebuffer frame = writeable.Lock())
        {
            source.CopyPixels(frame);
            PixelChannels channels = getPixelChannels(frame.Format);
            float hueOffset = hue / 360f;
            byte[] pixels = new byte[frame.RowBytes * frame.Size.Height];
            Marshal.Copy(frame.Address, pixels, 0, pixels.Length);
            for (int y = 0; y < frame.Size.Height; y++)
            {
                for (int x = 0; x < frame.Size.Width; x++)
                {
                    int index = y * frame.RowBytes + x * 4;
                    byte alpha = pixels[index + channels.Alpha];
                    if (alpha == 0)
                        continue;
                    rgbToHsv(
                        pixels[index + channels.Red],
                        pixels[index + channels.Green],
                        pixels[index + channels.Blue],
                        out float h,
                        out float s,
                        out float v);
                    hsvToRgb((h + hueOffset) % 1f, s, v, out byte r, out byte g, out byte b);
                    pixels[index + channels.Red] = r;
                    pixels[index + channels.Green] = g;
                    pixels[index + channels.Blue] = b;
                }
            }
            Marshal.Copy(pixels, 0, frame.Address, pixels.Length);
        }
        return writeable;
    }

    private static void rgbToHsv(byte r, byte g, byte b, out float h, out float s, out float v)
    {
        float rf = r / 255f;
        float gf = g / 255f;
        float bf = b / 255f;
        float max = Math.Max(rf, Math.Max(gf, bf));
        float min = Math.Min(rf, Math.Min(gf, bf));
        float delta = max - min;
        v = max;
        if (delta <= 0.00001f)
        {
            h = 0;
            s = 0;
            return;
        }
        s = delta / max;
        if (rf >= max)
            h = (gf - bf) / delta % 6f;
        else if (gf >= max)
            h = (bf - rf) / delta + 2f;
        else
            h = (rf - gf) / delta + 4f;
        h /= 6f;
        if (h < 0)
            h += 1f;
    }

    private static void hsvToRgb(float h, float s, float v, out byte r, out byte g, out byte b)
    {
        float c = v * s;
        float x = c * (1 - Math.Abs(h * 6f % 2f - 1));
        float m = v - c;
        float rf, gf, bf;
        int sector = (int)(h * 6f) % 6;
        switch (sector)
        {
            case 0: rf = c; gf = x; bf = 0; break;
            case 1: rf = x; gf = c; bf = 0; break;
            case 2: rf = 0; gf = c; bf = x; break;
            case 3: rf = 0; gf = x; bf = c; break;
            case 4: rf = x; gf = 0; bf = c; break;
            default: rf = c; gf = 0; bf = x; break;
        }
        r = (byte)Math.Clamp((rf + m) * 255f, 0, 255);
        g = (byte)Math.Clamp((gf + m) * 255f, 0, 255);
        b = (byte)Math.Clamp((bf + m) * 255f, 0, 255);
    }

    private static PixelChannels getPixelChannels(PixelFormat format)
    {
        if (format == PixelFormat.Bgra8888)
            return new PixelChannels(2, 1, 0, 3);
        if (format == PixelFormat.Rgba8888)
            return new PixelChannels(0, 1, 2, 3);
        throw new NotSupportedException($"Unsupported blueprint preview pixel format: {format}");
    }

    internal string resolveTexturePath(string texturePath)
    {
        string path;
        if (Path.IsPathRooted(texturePath))
            path = texturePath;
        else if (texturePath.StartsWith("Assets/", StringComparison.OrdinalIgnoreCase)
            || texturePath.StartsWith("Assets\\", StringComparison.OrdinalIgnoreCase))
            path = Path.Combine(projectPath, texturePath);
        else
            path = Path.Combine(projectPath, "Assets", "Characters", texturePath);
        return Path.GetFullPath(path);
    }

    private readonly record struct PixelChannels(int Red, int Green, int Blue, int Alpha);
    private readonly record struct SourceImageInfo(DateTime ModifiedAt, long Length, PixelSize Size);
}
