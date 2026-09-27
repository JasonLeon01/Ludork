using Avalonia;
using Avalonia.Threading;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using Ludork.Models;
using System;
using System.IO;
using System.Threading;

namespace Ludork.Controls;

public sealed partial class MapPanel
{
    private CancellationTokenSource? pendingActorRequest;
    private EditorThumbnailLease? pendingActorImage;

    private void ensurePendingActorRenderState()
    {
        if (!pendingActorRenderStateDirty)
            return;
        pendingActorRenderStateDirty = false;
        if (string.IsNullOrWhiteSpace(pendingActor) || gameData is null || previewService is null
            || CurrentMapDocument is null || IsRuntimeEditing || VisualRoot is null)
            return;
        pendingActorRequest = new CancellationTokenSource();
        string reference = pendingActor;
        string? mapKey = CurrentMapKey;
        ProjectDataStore data = gameData;
        BlueprintPreviewService previews = previewService;
        CancellationToken cancellationToken = pendingActorRequest.Token;
        Dispatcher.UIThread.Post(() => preparePendingActor(reference, mapKey, data, previews, cancellationToken),
            DispatcherPriority.Background);
    }

    private async void preparePendingActor(string reference, string? mapKey, ProjectDataStore data,
        BlueprintPreviewService previews, CancellationToken cancellationToken)
    {
        EditorThumbnailLease? loaded = null;
        try
        {
            cancellationToken.ThrowIfCancellationRequested();
            (EditorThumbnailLease? thumbnail, ActorVisualDescriptor? descriptor) =
                await previews.LoadPreviewAsync(reference, 80, cancellationToken);
            thumbnail?.Dispose();
            cancellationToken.ThrowIfCancellationRequested();
            if (descriptor is null || !GameAssetPath.TryToProjectFile(data.ProjectPath, descriptor.TexturePath, out string path))
                return;
            loaded = await data.Thumbnails.AcquireAsync(path, 0, cancellationToken);
            if (loaded is null)
                return;
            if (!EditorBitmapEffects.IsNeutralHue(descriptor.Hue))
            {
                EditorThumbnailLease? colored = await data.Thumbnails.AcquireDerivedAsync(path, 0,
                    FormattableString.Invariant($"map-ghost-hue:{EditorBitmapEffects.NormalizeHue(descriptor.Hue):R}"), loaded,
                    source => EditorBitmapEffects.CreateHueShiftedBitmap(source, descriptor.Hue), cancellationToken);
                loaded.Dispose();
                loaded = colored;
                if (loaded is null)
                    return;
            }
            cancellationToken.ThrowIfCancellationRequested();
            if (VisualRoot is null || IsRuntimeEditing || !ReferenceEquals(gameData, data)
                || !ReferenceEquals(previewService, previews) || CurrentMapKey != mapKey || pendingActor != reference)
                return;
            PixelRect rect = descriptor.BaseTextureRect;
            if (rect.X < 0 || rect.Y < 0 || rect.Width <= 0 || rect.Height <= 0
                || rect.Right > loaded.Bitmap.PixelSize.Width || rect.Bottom > loaded.Bitmap.PixelSize.Height)
                return;
            ActorPreviewLease? previewLease = null;
            if (descriptor.RequiresNativePreview)
            {
                previewLease = previews.ActorPreviews.Acquire(descriptor, 0, false);
                previewLease.FrameChanged += onActorPreviewFrameChanged;
            }
            pendingActorImage = loaded;
            loaded = null;
            pendingActorRenderState = new ActorRenderState(MapDocumentCodec.CreateActor(reference),
                pendingActorImage.Bitmap, new Rect(rect.X, rect.Y, rect.Width, rect.Height),
                descriptor.Translation, descriptor.Scale, descriptor.Origin, descriptor.Rotation,
                descriptor.MapPreviewOpacity, descriptor.Animated, descriptor.SwitchInterval,
                descriptor.FrameCount, previewLease);
            animationStateDirty = true;
            scheduleActorPreviewActivityUpdate();
            InvalidateVisual();
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
        }
        finally
        {
            loaded?.Dispose();
        }
    }
}
