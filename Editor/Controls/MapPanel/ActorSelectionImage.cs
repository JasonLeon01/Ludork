using Avalonia;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Controls;

public sealed partial class MapPanel
{
    internal async Task<(RenderTargetBitmap Image, Vector FrameOffset)?> CaptureSelectedActorImageAsync(
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (!canAdjustActorProperties() || selectedActors.Count != 1)
            return null;
        ActorRenderState? actor = getSelectedActorImageState();
        if (actor is null || previewService is null
            || previewService.tryResolveMapActorClass(CurrentMapDocument!, actor.Actor) is not ResolvedBlueprintClass resolved)
            return null;
        if (previewService.tryResolveActorVisual(resolved, actor.Actor.Blueprint) is { RequiresNativePreview: true } descriptor)
        {
            MapDocumentSnapshot? document = CurrentMapDocument;
            MapActorReference reference = selectedActors[0];
            using ActorPreviewLease preview = previewService.ActorPreviews.AcquireStatic(descriptor,
                TimeSpan.FromSeconds(previewService.MapPreviews.AnimationTime));
            TaskCompletionSource ready = new(TaskCreationOptions.RunContinuationsAsynchronously);
            void onFrameChanged(object? sender, EventArgs args)
            {
                if (preview.Frame is not null && !preview.NeedsNativeRender && preview.MaterialError is null)
                    ready.TrySetResult();
            }
            preview.FrameChanged += onFrameChanged;
            try
            {
                onFrameChanged(null, EventArgs.Empty);
                await ready.Task.WaitAsync(TimeSpan.FromSeconds(5), cancellationToken);
                cancellationToken.ThrowIfCancellationRequested();
                if (!ReferenceEquals(CurrentMapDocument, document) || selectedActors.Count != 1
                    || selectedActors[0] != reference || getSelectedActorImageState() is null
                    || preview.Frame is not { } frame || preview.MaterialError is not null)
                    return null;
                PixelRect rect = preview.SourceRect;
                RenderTargetBitmap? materialImage = captureActorImage(frame,
                    new Rect(rect.X, rect.Y, rect.Width, rect.Height), actor.MapPreviewOpacity);
                return materialImage is null ? null : (materialImage, preview.FrameOffset);
            }
            catch (TimeoutException)
            {
                return null;
            }
            finally
            {
                preview.FrameChanged -= onFrameChanged;
            }
        }
        Bitmap? image = actor.PreviewLease?.Frame ?? actor.Image;
        if (image is null)
            return null;
        RenderTargetBitmap? snapshot = captureActorImage(image, getActorRenderSource(actor), actor.MapPreviewOpacity);
        return snapshot is null ? null : (snapshot, actor.PreviewLease?.FrameOffset ?? default);
    }

    private static RenderTargetBitmap? captureActorImage(Bitmap image, Rect source, double opacity)
    {
        if (source.Width <= 0 || source.Height <= 0 || source.Left < 0 || source.Top < 0
            || source.Right > image.PixelSize.Width || source.Bottom > image.PixelSize.Height)
            return null;
        RenderTargetBitmap snapshot = new(new PixelSize((int)Math.Ceiling(source.Width), (int)Math.Ceiling(source.Height)));
        try
        {
            using DrawingContext context = snapshot.CreateDrawingContext();
            using (context.PushOpacity(opacity))
                context.DrawImage(image, source, new Rect(0, 0, source.Width, source.Height));
            return snapshot;
        }
        catch
        {
            snapshot.Dispose();
            throw;
        }
    }

    internal MapActorImageLayout? GetSelectedActorImageLayout(Vector frameOffset)
    {
        ActorRenderState? actor = getSelectedActorImageState();
        if (actor is null || actor.Scale.X == 0 || actor.Scale.Y == 0
            || !tryGetActorPosition(actor.Actor, out int x, out int y)
            || !tryGetMapSize(out int width, out int height))
            return null;
        Rect map = getMapRect(width, height);
        Matrix transform = createActorTransform(x, y, actor.Translation, actor.Scale, actor.Rotation)
            * Matrix.CreateTranslation(map.X, map.Y);
        Rect cell = getLocalTileRect(x, y).Translate(new Vector(map.X, map.Y));
        return new MapActorImageLayout(
            Matrix.CreateTranslation(frameOffset.X - actor.Origin.X, frameOffset.Y - actor.Origin.Y) * transform,
            transform.Transform(default(Point)), cell);
    }

    private ActorRenderState? getSelectedActorImageState()
    {
        if (IsRuntimeEditing || !canEditMap || EditMode != MapEditMode.Actor || selectedActors.Count != 1)
            return null;
        ensureActorRenderStates();
        MapActorReference reference = selectedActors[0];
        return actorRenderStates.GetValueOrDefault(reference.LayerName)
            ?.FirstOrDefault(value => value.Actor.Tag == reference.Tag);
    }
}
