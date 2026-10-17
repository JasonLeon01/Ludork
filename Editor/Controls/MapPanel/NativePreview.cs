using Avalonia;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Ludork.Services;
using System;
using System.Diagnostics;
using System.Globalization;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class MapPanel
{
    private MapPreviewLease? nativePreview;
    private JsonObject? nativeScene;
    private long nativeSceneRevision;
    private long nativeContentRevision = -1;

    private void configureNativePreview()
    {
        nativePreview?.Dispose();
        nativePreview = previewService?.MapPreviews.Acquire();
        if (nativePreview is not null)
        {
            nativePreview.FrameChanged += onNativeFrameChanged;
            nativePreview.Visible = () => MapPreviewService.IsVisible(this);
        }
        nativeScene = null;
    }

    private void onNativeFrameChanged(object? sender, EventArgs args) => InvalidateVisual();
    private void onNativePreviewInvalidated(object? sender, EventArgs args)
    {
        nativeScene = null;
        InvalidateVisual();
    }

    private void drawNativeMap(DrawingContext context, Rect viewport)
    {
        if (previewService is null || CurrentMapDocument is null || CurrentMapKey is null)
            return;
        if (nativePreview is null)
            configureNativePreview();
        MapPreviewService service = previewService.MapPreviews;
        if (nativeScene is null || nativeContentRevision != service.ContentRevision)
        {
            long started = Stopwatch.GetTimestamp();
            nativeScene = previewService.MapScenes.Build(CurrentMapKey, CurrentMapDocument.Data);
            nativePreview!.RecordSceneUpdate(started);
            nativeContentRevision = service.ContentRevision;
            nativeSceneRevision++;
        }
        double scale = tileSize / (double)SourceTileSize;
        JsonArray? renderOrigin = CurrentMapDocument.Data["renderOrigin"] as JsonArray;
        Vector origin = renderOrigin is { Count: 2 }
            ? new Vector(JsonScalar.Number(renderOrigin[0]) * SourceTileSize, JsonScalar.Number(renderOrigin[1]) * SourceTileSize)
            : default;
        Rect worldView = new(viewport.X / scale + origin.X, viewport.Y / scale + origin.Y, viewport.Width / scale, viewport.Height / scale);
        double renderScale = getRenderScaling();
        PixelSize size = new(Math.Clamp((int)Math.Ceiling(viewport.Width * renderScale), 1, 4096),
            Math.Clamp((int)Math.Ceiling(viewport.Height * renderScale), 1, 4096));
        nativePreview!.IsInteracting = actorPropertyDrag is not null || actorMoveIndex is not null || actorPointerGesture?.Dragging == true || propertyWheelTarget is not null || tileBrushDragging || rectangleStart is not null;
        nativePreview.Request(nativeScene, nativeSceneRevision, worldView, size, service.Options, selectedLayerName, CurrentMapKey);
        if (nativePreview.Frame is Bitmap bitmap)
        {
            Rect frame = nativePreview.FrameView;
            context.DrawImage(bitmap, new Rect(0, 0, bitmap.PixelSize.Width, bitmap.PixelSize.Height),
                new Rect((frame.X - origin.X) * scale, (frame.Y - origin.Y) * scale, frame.Width * scale, frame.Height * scale));
            MapPreviewOverlays.Draw(context, nativeScene, new Point(-origin.X * scale, -origin.Y * scale), scale, service.Options, true);
        }
        else if (nativePreview.Error.Length != 0)
        {
            FormattedText text = new(nativePreview.Error, CultureInfo.CurrentCulture, FlowDirection.LeftToRight,
                new Typeface("Arial"), 13, EditorTheme.Brush("Text")) { MaxTextWidth = Math.Max(40, viewport.Width - 24) };
            context.DrawText(text, viewport.TopLeft + new Vector(12, 44));
        }
    }
}
