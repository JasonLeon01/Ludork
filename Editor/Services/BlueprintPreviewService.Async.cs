using Avalonia;
using Avalonia.Media.Imaging;
using Ludork.Models;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class BlueprintPreviewService
{
    public Task<(EditorThumbnailLease? Thumbnail, ActorVisualDescriptor? Visual)> LoadPreviewAsync(
        string blueprintReference,
        int size,
        CancellationToken cancellationToken = default)
    {
        ResolvedBlueprintClass resolved = classResolver.Resolve(blueprintReference);
        return LoadPreviewAsync(resolved, blueprintReference, size, cancellationToken);
    }

    public async Task<(EditorThumbnailLease? Thumbnail, ActorVisualDescriptor? Visual)> LoadPreviewAsync(
        ResolvedBlueprintClass resolved,
        string blueprintReference,
        int size,
        CancellationToken cancellationToken = default)
    {
        string? texture = getResolvedValue(resolved, "texturePath")?.ToString();
        if (string.IsNullOrWhiteSpace(texture))
            return (null, null);
        string path = await Task.Run(() => resolveTextureFilePath(texture), cancellationToken);
        using EditorThumbnailLease? source = await gameData.Thumbnails.AcquireAsync(path, 0, cancellationToken);
        if (source is null)
            return (null, null);
        cancellationToken.ThrowIfCancellationRequested();
        ActorVisualDescriptor? visual;
        (int sx, int sy, int w, int h)? rect;
        (double x, double y) origin;
        (double x, double y) scale;
        float hue;
        using (classResolver.BeginBatch())
        {
            visual = createActorVisual(resolved, blueprintReference, source.Bitmap.PixelSize);
            rect = parseRect(getResolvedValue(resolved, "defaultRect"))
                ?? defaultRect(classResolver.IsDerivedFrom(resolved, "Engine.Character"),
                    source.Bitmap.PixelSize.Width, source.Bitmap.PixelSize.Height);
            origin = parseVec2(getResolvedValue(resolved, "defaultOrigin"), 0, 0);
            scale = parseVec2(getResolvedValue(resolved, "defaultScale"), 1, 1);
            hue = parseHue(getResolvedValue(resolved, "hue"));
        }
        if (rect is not { } crop)
            return (null, visual);
        string variant = FormattableString.Invariant($"actor:{crop.sx},{crop.sy},{crop.w},{crop.h}:{origin.x},{origin.y}:{scale.x},{scale.y}:{hue}");
        EditorThumbnailLease? thumbnail = await gameData.Thumbnails.AcquireDerivedAsync(path, size, variant, source, bitmap =>
        {
            int width = Math.Max(1, (int)(crop.w * scale.x));
            int height = Math.Max(1, (int)(crop.h * scale.y));
            Bitmap? preview = renderCrop(bitmap, crop.sx, crop.sy, crop.w, crop.h,
                origin.x, origin.y, scale.x, scale.y, width, height);
            if (preview is null)
                return null;
            if (!isNeutralHue(hue))
            {
                Bitmap colored = applyHue(preview, hue);
                preview.Dispose();
                preview = colored;
            }
            Bitmap result = scaleToFit(preview, size);
            if (!ReferenceEquals(result, preview))
                preview.Dispose();
            return result;
        }, cancellationToken);
        return (thumbnail, visual);
    }
}
