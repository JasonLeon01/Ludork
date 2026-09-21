using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Threading;
using Ludork.Services;
using Ludork.Models;
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json.Nodes;
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
            if (Math.Abs(descriptor.Hue % 360) > 0.001)
            {
                EditorThumbnailLease? colored = await data.Thumbnails.AcquireDerivedAsync(path, 0,
                    FormattableString.Invariant($"map-ghost-hue:{descriptor.Hue % 360}"), loaded,
                    source => createPendingActorHueImage(source, descriptor.Hue), cancellationToken);
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

    private static Bitmap createPendingActorHueImage(Bitmap source, double hue)
    {
        WriteableBitmap result = new(source.PixelSize, new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Unpremul);
        try
        {
            using ILockedFramebuffer frame = result.Lock();
            source.CopyPixels(frame);
            byte[] pixels = new byte[frame.RowBytes * frame.Size.Height];
            Marshal.Copy(frame.Address, pixels, 0, pixels.Length);
            double offset = (hue % 360 + 360) % 360 / 360.0;
            for (int y = 0; y < frame.Size.Height; y++)
            for (int x = 0; x < frame.Size.Width; x++)
            {
                int index = y * frame.RowBytes + x * 4;
                if (pixels[index + 3] == 0)
                    continue;
                (double h, double s, double v) = rgbToHsv(pixels[index + 2] / 255.0, pixels[index + 1] / 255.0, pixels[index] / 255.0);
                (double r, double g, double b) = hsvToRgb((h + offset) % 1.0, s, v);
                pixels[index] = (byte)Math.Round(b * 255);
                pixels[index + 1] = (byte)Math.Round(g * 255);
                pixels[index + 2] = (byte)Math.Round(r * 255);
            }
            Marshal.Copy(pixels, 0, frame.Address, pixels.Length);
            return result;
        }
        catch
        {
            result.Dispose();
            throw;
        }
    }
}
