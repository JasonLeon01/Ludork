using Avalonia;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Ludork.Services;
using Ludork.Models;
using System;
using System.Diagnostics;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class WorldMapPreviewRenderer : IDisposable
{
    private readonly ProjectDataStore gameData;
    private readonly BlueprintPreviewService? preview;
    private readonly Dictionary<string, CachedMap> thumbnails = new(StringComparer.Ordinal);
    private readonly MapPreviewLease? world;
    private JsonObject? worldScene;
    private string worldSignature = string.Empty;
    private long worldRevision;
    private long contentRevision = -1;
    private long revision;

    public WorldMapPreviewRenderer(ProjectDataStore gameData, BlueprintPreviewService? previewService = null)
    {
        this.gameData = gameData;
        gameData.Maps.MapPreviewChanged += onMapChanged;
        preview = previewService ?? gameData.BlueprintPreviews;
        if (preview is not null)
        {
            world = preview.MapPreviews.Acquire();
            world.FrameChanged += onFrameChanged;
            preview.MapPreviews.Invalidated += onInvalidated;
            preview.MapPreviews.OptionsChanged += onFrameChanged;
        }
    }

    public event EventHandler? PreviewChanged;
    public bool IsLit => preview?.MapPreviews.Options.Lit == true;
    public int SourceCellSize => gameData.Configs.getCellSize();
    public void SetVisibility(Func<bool> visible)
    {
        if (world is not null)
            world.Visible = visible;
    }

    public void DrawMap(DrawingContext context, string key, JsonObject map, Point origin, double cellSize, Rect clip, double renderScale, Func<bool> visible)
    {
        if (preview is null)
            return;
        if (!thumbnails.TryGetValue(key, out CachedMap? cached))
        {
            cached = new CachedMap(preview.MapPreviews.Acquire(true));
            cached.Lease.FrameChanged += onFrameChanged;
            thumbnails.Add(key, cached);
        }
        cached.Lease.Visible = visible;
        if (cached.Scene is null || cached.Source != map || cached.ContentRevision != preview.MapPreviews.ContentRevision)
        {
            long started = Stopwatch.GetTimestamp();
            cached.Scene = preview.MapScenes.Build(key, map);
            cached.Lease.RecordSceneUpdate(started);
            cached.Source = map;
            cached.ContentRevision = preview.MapPreviews.ContentRevision;
            cached.Revision = ++revision;
        }
        Rect mapRect = new(origin, new Size(number(map["width"]) * cellSize, number(map["height"]) * cellSize));
        draw(context, cached.Lease, cached.Scene, cached.Revision, origin, cellSize, mapRect.Intersect(clip), renderScale);
    }

    public void DrawWorld(DrawingContext context, string worldKey, JsonObject manifest,
        IReadOnlyList<(string Key, int X, int Y, int Priority, JsonObject Map)> maps,
        Point origin, double cellSize, Rect clip, double renderScale)
    {
        if (preview is null || world is null)
            return;
        string signature = string.Join(";", maps.Select(map => FormattableString.Invariant($"{map.Key}:{map.X}:{map.Y}:{map.Priority}")));
        if (worldScene is null || worldSignature != signature || contentRevision != preview.MapPreviews.ContentRevision)
        {
            long started = Stopwatch.GetTimestamp();
            worldScene = preview.MapScenes.Build(manifest, maps);
            world.RecordSceneUpdate(started);
            worldSignature = signature;
            contentRevision = preview.MapPreviews.ContentRevision;
            worldRevision = ++revision;
        }
        draw(context, world, worldScene, worldRevision, origin, cellSize, clip, renderScale, worldKey);
    }

    private void draw(DrawingContext context, MapPreviewLease lease, JsonObject scene, long sceneRevision,
        Point origin, double cellSize, Rect clip, double renderScale, string? sceneIdentity = null)
    {
        if (clip.Width <= 0 || clip.Height <= 0 || preview is null)
            return;
        double scale = cellSize / gameData.Configs.getCellSize();
        Rect view = new((clip.X - origin.X) / scale, (clip.Y - origin.Y) / scale, clip.Width / scale, clip.Height / scale);
        PixelSize size = new(Math.Clamp((int)Math.Ceiling(clip.Width * renderScale), 1, 4096),
            Math.Clamp((int)Math.Ceiling(clip.Height * renderScale), 1, 4096));
        lease.Request(scene, sceneRevision, view, size, preview.MapPreviews.Options, sceneIdentity: sceneIdentity);
        if (lease.Frame is Bitmap bitmap)
        {
            Rect frame = lease.FrameView;
            context.DrawImage(bitmap, new Rect(0, 0, bitmap.PixelSize.Width, bitmap.PixelSize.Height),
                new Rect(origin.X + frame.X * scale, origin.Y + frame.Y * scale, frame.Width * scale, frame.Height * scale));
            if (!lease.IsThumbnail)
                MapPreviewOverlays.Draw(context, scene, origin, scale, preview.MapPreviews.Options, true);
        }
        else if (lease.Error.Length != 0)
        {
            FormattedText text = new(lease.Error, CultureInfo.CurrentCulture, FlowDirection.LeftToRight,
                new Typeface("Arial"), 12, EditorTheme.Brush("Text")) { MaxTextWidth = Math.Max(40, clip.Width - 16) };
            context.DrawText(text, clip.TopLeft + new Vector(8, 40));
        }
    }

    public void ReleaseMap(string key)
    {
        if (thumbnails.Remove(key, out CachedMap? cached))
            cached.Lease.Dispose();
    }

    public void SetActive(bool active)
    {
        if (world is not null)
            world.IsActive = active;
    }

    public void SetInteracting(bool interacting)
    {
        if (world is not null)
            world.IsInteracting = interacting;
    }

    public void TrimMapCache(IReadOnlyCollection<string> pinnedMapKeys) => gameData.Maps.TrimWorldChildCache(pinnedMapKeys);
    public void ClearPendingWork() => SetActive(false);
    public void ResetView()
    {
        worldScene = null;
        world?.Invalidate();
        SetActive(false);
    }

    private void onMapChanged(object? sender, MapPreviewChangedEventArgs args)
    {
        if (preview is null || args.MapKey is not string key || args.Edit is null
            || !MapPreviewSceneBuilder.IsActorEdit(args.Edit))
            return;
        JsonObject? map = gameData.Maps.getMap(key);
        if (map is null)
            return;
        long started = Stopwatch.GetTimestamp();
        if (worldScene is not null && preview.MapScenes.UpdateActors(worldScene, key, map, args.Edit))
        {
            worldRevision = ++revision;
            world?.RecordSceneUpdate(started);
        }
        if (thumbnails.TryGetValue(key, out CachedMap? cached) && cached.Scene is not null
            && preview.MapScenes.UpdateActors(cached.Scene, key, map, args.Edit))
        {
            cached.Revision = ++revision;
            cached.Lease.RecordSceneUpdate(started);
        }
        PreviewChanged?.Invoke(this, EventArgs.Empty);
    }

    private void onInvalidated(object? sender, EventArgs args)
    {
        worldScene = null;
        PreviewChanged?.Invoke(this, EventArgs.Empty);
    }

    private void onFrameChanged(object? sender, EventArgs args) => PreviewChanged?.Invoke(this, EventArgs.Empty);
    private static int number(JsonNode? value) => value?.GetValue<int>() ?? 0;

    public void Dispose()
    {
        gameData.Maps.MapPreviewChanged -= onMapChanged;
        world?.Dispose();
        foreach (CachedMap cached in thumbnails.Values)
            cached.Lease.Dispose();
        thumbnails.Clear();
        if (preview is not null)
        {
            preview.MapPreviews.Invalidated -= onInvalidated;
            preview.MapPreviews.OptionsChanged -= onFrameChanged;
        }
    }

    private sealed class CachedMap(MapPreviewLease lease)
    {
        public MapPreviewLease Lease { get; } = lease;
        public JsonObject? Scene { get; set; }
        public JsonObject? Source { get; set; }
        public long Revision { get; set; }
        public long ContentRevision { get; set; }
    }
}
