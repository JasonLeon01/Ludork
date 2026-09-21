using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Controls;

public sealed class AnimationPreview : Control, IDisposable
{
    private readonly ProjectDataStore gameData;
    private readonly Func<JsonObject> getData;
    private readonly Dictionary<string, EditorThumbnailLease> cache = new(StringComparer.Ordinal);
    private CancellationTokenSource? imageRequest;
    private string[] preparedAssets = [];
    private Point dragStart;
    private double dragStartX;
    private double dragStartY;
    private bool dragging;

    public AnimationPreview(ProjectDataStore gameData, Func<JsonObject> getData)
    {
        this.gameData = gameData;
        this.getData = getData;
        Focusable = true;
    }

    public event Action<int, int>? SegmentSelected;
    public event Action? SegmentChanged;
    public double CurrentTime { get; set; }
    public int SelectedTrack { get; set; } = -1;
    public int SelectedSegment { get; set; } = -1;

    public void Refresh() => InvalidateVisual();

    public override void Render(DrawingContext context)
    {
        base.Render(context);
        Rect bounds = new(Bounds.Size);
        context.FillRectangle(EditorTheme.Brush("Background"), bounds);
        Point center = new(bounds.Width / 2, bounds.Height / 2);
        context.DrawLine(new Pen(EditorTheme.Brush("Border"), 1), new Point(center.X, 0), new Point(center.X, bounds.Height));
        context.DrawLine(new Pen(EditorTheme.Brush("Border"), 1), new Point(0, center.Y), new Point(bounds.Width, center.Y));

        JsonArray assets = getData()["assets"] as JsonArray ?? [];
        JsonArray lines = getData()["timeLines"] as JsonArray ?? [];
        for (int track = 0; track < lines.Count; track += 1)
        {
            if (lines[track] is not JsonObject line || line["timeSegments"] is not JsonArray segments)
                continue;
            for (int index = 0; index < segments.Count; index += 1)
            {
                if (segments[index] is not JsonObject segment || segment["type"]?.GetValue<string>() == "sound")
                    continue;
                JsonObject? start = segment["startFrame"] as JsonObject;
                JsonObject? end = segment["endFrame"] as JsonObject;
                if (start is null || end is null)
                    continue;
                double startTime = AnimationEditor.number(start["time"]);
                double endTime = AnimationEditor.number(end["time"]);
                if (CurrentTime < startTime || CurrentTime > endTime)
                    continue;
                int assetIndex = (int)AnimationEditor.number(segment["asset"], -1);
                if (assetIndex < 0 || assetIndex >= assets.Count || assets[assetIndex] is not JsonValue assetValue || !assetValue.TryGetValue<string>(out string? assetName) || string.IsNullOrWhiteSpace(assetName))
                    continue;
                Bitmap? bitmap = getBitmap(assetName);
                if (bitmap is null)
                    continue;
                double factor = endTime - startTime < 0.0001 ? 0 : (CurrentTime - startTime) / (endTime - startTime);
                double x = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 0, factor);
                double y = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 1, factor);
                double scaleX = interpolate(start["scale"] as JsonArray, end["scale"] as JsonArray, 0, factor, 1);
                double scaleY = interpolate(start["scale"] as JsonArray, end["scale"] as JsonArray, 1, factor, 1);
                if (segment["flipX"]?.GetValue<bool>() == true)
                    scaleX *= -1;
                double rotation = interpolateValue(start["rotation"], end["rotation"], factor);
                double radians = rotation * Math.PI / 180.0;
                double cosine = Math.Cos(radians);
                double sine = Math.Sin(radians);
                Matrix transform = new(scaleX * cosine, scaleX * sine, -scaleY * sine, scaleY * cosine, center.X + x, center.Y + y);
                Rect localRect = new(-bitmap.Size.Width / 2, -bitmap.Size.Height / 2, bitmap.Size.Width, bitmap.Size.Height);
                using (context.PushTransform(transform))
                    context.DrawImage(bitmap, new Rect(bitmap.Size), localRect);
                if (track == SelectedTrack && index == SelectedSegment)
                    context.DrawRectangle(new Pen(Brushes.White, 2), new Rect(center.X + x - bitmap.Size.Width * Math.Abs(scaleX) / 2, center.Y + y - bitmap.Size.Height * Math.Abs(scaleY) / 2, bitmap.Size.Width * Math.Abs(scaleX), bitmap.Size.Height * Math.Abs(scaleY)));
            }
        }
    }

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        if (!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        Point point = e.GetPosition(this);
        (int track, int segment) = hitTest(point);
        if (segment < 0)
        {
            SelectedTrack = -1;
            SelectedSegment = -1;
            SegmentSelected?.Invoke(-1, -1);
            return;
        }
        SelectedTrack = track;
        SelectedSegment = segment;
        SegmentSelected?.Invoke(track, segment);
        if (segmentAt(track, segment) is JsonObject selected)
        {
            JsonObject start = selected["startFrame"] as JsonObject ?? new JsonObject();
            JsonObject end = selected["endFrame"] as JsonObject ?? new JsonObject();
            double factor = timeFactor(start, end);
            dragStartX = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 0, factor);
            dragStartY = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 1, factor);
            dragStart = point;
            dragging = true;
            e.Pointer.Capture(this);
        }
        InvalidateVisual();
    }

    protected override void OnPointerMoved(PointerEventArgs e)
    {
        if (!dragging || !e.GetCurrentPoint(this).Properties.IsLeftButtonPressed || segmentAt(SelectedTrack, SelectedSegment) is not JsonObject segment)
            return;
        double x = snap(dragStartX + e.GetPosition(this).X - dragStart.X);
        double y = snap(dragStartY + e.GetPosition(this).Y - dragStart.Y);
        JsonObject start = segment["startFrame"] as JsonObject ?? new JsonObject();
        JsonObject end = segment["endFrame"] as JsonObject ?? new JsonObject();
        double factor = timeFactor(start, end);
        double originalX = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 0, factor);
        double originalY = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 1, factor);
        shiftFrame(start, x - originalX, y - originalY);
        shiftFrame(end, x - originalX, y - originalY);
        segment["startFrame"] = start;
        segment["endFrame"] = end;
        InvalidateVisual();
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        if (dragging)
            SegmentChanged?.Invoke();
        dragging = false;
        e.Pointer.Capture(null);
    }

    private (int track, int segment) hitTest(Point point)
    {
        JsonArray lines = getData()["timeLines"] as JsonArray ?? [];
        for (int track = lines.Count - 1; track >= 0; track -= 1)
        {
            if (lines[track] is not JsonObject line || line["timeSegments"] is not JsonArray segments)
                continue;
            for (int index = segments.Count - 1; index >= 0; index -= 1)
            {
                if (segments[index] is not JsonObject segment || segment["type"]?.GetValue<string>() == "sound")
                    continue;
                JsonObject? start = segment["startFrame"] as JsonObject;
                JsonObject? end = segment["endFrame"] as JsonObject;
                if (start is null || end is null || CurrentTime < AnimationEditor.number(start["time"]) || CurrentTime > AnimationEditor.number(end["time"]))
                    continue;
                double factor = timeFactor(start, end);
                double x = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 0, factor);
                double y = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 1, factor);
                if (new Rect(Bounds.Width / 2 + x - 32, Bounds.Height / 2 + y - 32, 64, 64).Contains(point))
                    return (track, index);
            }
        }
        return (-1, -1);
    }

    private JsonObject? segmentAt(int track, int segment)
    {
        JsonArray lines = getData()["timeLines"] as JsonArray ?? [];
        return track >= 0 && track < lines.Count && lines[track] is JsonObject line && line["timeSegments"] is JsonArray segments
            && segment >= 0 && segment < segments.Count ? segments[segment] as JsonObject : null;
    }

    public void PrepareAssets(string[] assets, bool force = false)
    {
        if (!force && imageRequest is not null && assets.SequenceEqual(preparedAssets, StringComparer.Ordinal))
            return;
        Dispose();
        preparedAssets = assets;
        imageRequest = new CancellationTokenSource();
        CancellationToken token = imageRequest.Token;
        foreach (string asset in assets.Distinct(StringComparer.Ordinal))
        {
            string extension = System.IO.Path.GetExtension(asset);
            if (new[] { ".png", ".jpg", ".jpeg", ".bmp", ".webp" }.Contains(extension, StringComparer.OrdinalIgnoreCase))
                loadAsset(asset, token);
        }
    }

    private async void loadAsset(string asset, CancellationToken cancellationToken)
    {
        try
        {
            string? path = await Task.Run(() =>
                GameAssetPath.TryResolveExistingFile(gameData.ProjectPath, asset, out string resolved) ? resolved : null,
                cancellationToken);
            if (path is null)
                return;
            EditorThumbnailLease? lease = await gameData.Thumbnails.AcquireAsync(path, 0, cancellationToken);
            if (cancellationToken.IsCancellationRequested)
            {
                lease?.Dispose();
                return;
            }
            if (lease is not null)
                cache[asset] = lease;
            InvalidateVisual();
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
        }
    }

    public void Dispose()
    {
        imageRequest?.Cancel();
        imageRequest?.Dispose();
        imageRequest = null;
        foreach (EditorThumbnailLease lease in cache.Values)
            lease.Dispose();
        cache.Clear();
        InvalidateVisual();
    }

    private Bitmap? getBitmap(string asset) => cache.TryGetValue(asset, out EditorThumbnailLease? lease) ? lease.Bitmap : null;

    private static double interpolate(JsonArray? start, JsonArray? end, int index, double factor, double fallback = 0)
    {
        double a = AnimationEditor.number(start?.ElementAtOrDefault(index), fallback);
        double b = AnimationEditor.number(end?.ElementAtOrDefault(index), fallback);
        return a + (b - a) * factor;
    }

    private static double interpolateValue(JsonNode? start, JsonNode? end, double factor)
    {
        double a = AnimationEditor.number(start);
        return a + (AnimationEditor.number(end) - a) * factor;
    }

    private double timeFactor(JsonObject start, JsonObject end)
    {
        double startTime = AnimationEditor.number(start["time"]);
        double endTime = AnimationEditor.number(end["time"]);
        return endTime - startTime < 0.0001 ? 0 : (CurrentTime - startTime) / (endTime - startTime);
    }

    private static void shiftFrame(JsonObject frame, double x, double y)
    {
        JsonArray position = frame["position"] as JsonArray ?? new JsonArray(0.0, 0.0);
        position[0] = AnimationEditor.number(position.ElementAtOrDefault(0)) + x;
        position[1] = AnimationEditor.number(position.ElementAtOrDefault(1)) + y;
        frame["position"] = position;
    }

    private static double snap(double value) => Math.Round(value / 16) * 16;
}
