using Avalonia;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class MapPreviewService : IDisposable
{
    private readonly MapPreviewClient foreground;
    private readonly MapPreviewClient thumbnails;
    private readonly HashSet<MapPreviewLease> leases = [];
    private readonly DispatcherTimer timer = new() { Interval = TimeSpan.FromSeconds(1.0 / 30) };
    private readonly DispatcherTimer foregroundTimer = new();
    private readonly Stopwatch clock = Stopwatch.StartNew();
    private readonly CancellationTokenSource lifetime = new();
    private readonly FileSystemWatcher assets;
    private readonly ProjectDataStore gameData;
    private readonly PreviewRuntimeService runtime;
    private readonly ProjectConfigService projectConfig;
    private MapPreviewLease? active;
    private bool foregroundBusy;
    private bool thumbnailsBusy;
    private bool disposed;
    private int assetsChanged;
    private double animationTime;
    private double lastTick;
    private double lastForegroundChange;
    private double lastThumbnail;
    private double nextForeground;
    private long requestSequence;
    private JsonObject? materialSnapshot;
    private long materialRevision;

    public MapPreviewService(ProjectDataStore gameData, PreviewRuntimeService runtime, ProjectConfigService projectConfig)
    {
        this.gameData = gameData;
        this.runtime = runtime;
        this.projectConfig = projectConfig;
        Options = projectConfig.ViewportOptions;
        foreground = new MapPreviewClient(runtime);
        thumbnails = new MapPreviewClient(runtime);
        assets = new FileSystemWatcher(gameData.ProjectPath) { IncludeSubdirectories = true, NotifyFilter = NotifyFilters.FileName | NotifyFilters.LastWrite | NotifyFilters.Size };
        assets.Changed += onAssetChanged;
        assets.Created += onAssetChanged;
        assets.Deleted += onAssetChanged;
        assets.Renamed += onAssetChanged;
        assets.EnableRaisingEvents = true;
        gameData.Documents.ContentChanged += onDocumentsChanged;
        gameData.Maps.MapPreviewChanged += onMapChanged;
        gameData.DataRestored += onDataRestored;
        gameData.DataReloaded += onDataRestored;
        runtime.Changed += onRuntimeChanged;
        timer.Tick += onTick;
        foregroundTimer.Tick += onForegroundTick;
        timer.Start();
    }

    public long ContentRevision { get; private set; }
    public long ResourceRevision { get; private set; }
    public double AnimationTime => animationTime;
    public MapPreviewOptions Options { get; private set; }
    public event EventHandler? Invalidated;
    public event EventHandler? OptionsChanged;
    internal static bool IsVisible(Control control)
        => control.IsEffectivelyVisible && TopLevel.GetTopLevel(control) is { IsVisible: true } topLevel
            && topLevel is not Window { WindowState: WindowState.Minimized };
    public MapPreviewLease Acquire(bool thumbnail = false)
    {
        MapPreviewLease lease = new(this, thumbnail);
        leases.Add(lease);
        return lease;
    }

    public void SetOptions(MapPreviewOptions options)
    {
        if (Options == options)
            return;
        projectConfig.ViewportOptions = options;
        Options = options;
        foreach (MapPreviewLease lease in leases)
        {
            if (!options.Realtime)
                lease.SuspendParticles();
            lease.Invalidate();
        }
        OptionsChanged?.Invoke(this, EventArgs.Empty);
    }

    internal void Schedule(MapPreviewLease lease, bool changed)
    {
        if (!lease.IsThumbnail)
        {
            if (!ReferenceEquals(active, lease))
                active?.SuspendParticles();
            active = lease;
            if (changed)
                lastForegroundChange = clock.Elapsed.TotalSeconds;
            scheduleForeground();
        }
    }

    private void onForegroundTick(object? sender, EventArgs args)
    {
        foregroundTimer.Stop();
        scheduleForeground();
    }

    private void scheduleForeground()
    {
        if (active is { CanRender: false } || !Options.Realtime)
            active?.SuspendParticles();
        if (disposed || foregroundBusy || active is not { CanRender: true, Scene: not null } current
            || !current.Pending && !Options.Realtime)
            return;
        double now = clock.Elapsed.TotalSeconds;
        double delay = Math.Max(current.PriorityPending ? now : nextForeground, current.RetryAfter) - now;
        if (delay > 0)
        {
            foregroundTimer.Stop();
            foregroundTimer.Interval = TimeSpan.FromSeconds(delay);
            foregroundTimer.Start();
            return;
        }
        foregroundTimer.Stop();
        nextForeground = now + 1.0 / 30;
        _ = renderAsync(current, foreground, false);
    }

    internal void Release(MapPreviewLease lease)
    {
        leases.Remove(lease);
        if (ReferenceEquals(active, lease))
            active = null;
        if (!disposed)
            _ = (lease.IsThumbnail ? thumbnails : foreground).CloseSceneAsync(lease.Id);
    }

    private void onTick(object? sender, EventArgs args)
    {
        double now = clock.Elapsed.TotalSeconds;
        double delta = now - lastTick;
        lastTick = now;
        if (Options.Realtime)
            animationTime += delta;
        if (Interlocked.Exchange(ref assetsChanged, 0) != 0)
            invalidate(true);
        if (disposed)
            return;
        scheduleForeground();
        if (!thumbnailsBusy && active is not { CanRender: true, IsInteracting: true }
            && now - lastThumbnail >= 0.1 && now - lastForegroundChange >= 0.1 && leases.FirstOrDefault(lease => lease.IsThumbnail && lease.CanRender && lease.Pending && lease.Scene is not null && now >= lease.RetryAfter) is MapPreviewLease thumbnail)
            _ = renderAsync(thumbnail, thumbnails, true);
    }

    private async Task renderAsync(MapPreviewLease lease, MapPreviewClient client, bool thumbnail)
    {
        if (thumbnail)
        {
            thumbnailsBusy = true;
            lastThumbnail = clock.Elapsed.TotalSeconds;
        }
        else
            foregroundBusy = true;
        bool contentPending = lease.Pending;
        lease.Pending = false;
        lease.PriorityPending = false;
        long generation = lease.Generation;
        long epoch = lease.Epoch;
        long sequence = ++requestSequence;
        long resourceRevision = ResourceRevision;
        long nextMaterialRevision = materialRevision;
        long? selectionStartedAt = lease.SelectionStartedAt;
        Rect view = lease.View;
        PixelSize size = lease.Size;
        MapPreviewOptions options = lease.Options;
        long changedAt = contentPending ? lease.ChangedAt : Stopwatch.GetTimestamp();
        double sceneMilliseconds = contentPending ? lease.SceneUpdateMilliseconds : 0;
        (double particleDelta, bool resetParticleMotion) = lease.ParticleFrame(clock.Elapsed.TotalSeconds, options.Realtime);
        JsonObject request = new()
        {
            ["generation"] = sequence, ["sceneKey"] = lease.SceneKey, ["revision"] = lease.Revision,
            ["resourceRevision"] = resourceRevision, ["materialRevision"] = nextMaterialRevision,
            ["width"] = size.Width, ["height"] = size.Height,
            ["view"] = new JsonObject { ["x"] = view.X, ["y"] = view.Y, ["width"] = view.Width, ["height"] = view.Height, ["rotation"] = 0 },
            ["time"] = thumbnail ? 0 : animationTime, ["lit"] = options.Lit, ["showFog"] = options.ShowFog,
            ["showPanorama"] = options.ShowPanorama, ["selectedLayer"] = lease.SelectedLayer, ["otherLayerOpacity"] = 0.5,
            ["particleDelta"] = particleDelta, ["resetParticleMotion"] = resetParticleMotion,
        };
        try
        {
            materialSnapshot ??= gameData.Assets.CaptureMaterials();
            Bitmap bitmap = await client.RenderAsync(request, lease.Scene!, materialSnapshot, lifetime.Token);
            if (disposed || !lease.CanRender || epoch != lease.Epoch || sequence <= lease.PublishedSequence
                || resourceRevision != ResourceRevision || nextMaterialRevision != materialRevision
                || thumbnail && generation != lease.Generation)
            {
                bitmap.Dispose();
                return;
            }
            JsonObject? timing = client.LastResponse;
            lease.LastFrameTiming = new(Stopwatch.GetElapsedTime(changedAt).TotalMilliseconds, sceneMilliseconds,
                milliseconds(timing, "previewSerializeMs"), milliseconds(timing, "previewSceneApplyMs"),
                milliseconds(timing, "renderMilliseconds"), milliseconds(timing, "previewReadbackMs"),
                milliseconds(timing, "previewTransferMs"), milliseconds(timing, "previewPixelReadMs"), milliseconds(timing, "previewBitmapMs"),
                selectionStartedAt is long selectedAt ? Stopwatch.GetElapsedTime(selectedAt).TotalMilliseconds : null);
            if (selectionStartedAt == lease.SelectionStartedAt)
                lease.SelectionStartedAt = null;
            lease.PublishedSequence = sequence;
            lease.Publish(bitmap, view, string.Empty);
        }
        catch (Exception exception) when (PreviewHostConnection.IsProtocolException(exception) || exception is InvalidOperationException or OperationCanceledException)
        {
            lease.SuspendParticles();
            if (!disposed && !lease.Disposed && generation == lease.Generation)
            {
                lease.Publish(null, view, exception.Message);
                lease.Pending = true;
                lease.RetryAfter = clock.Elapsed.TotalSeconds + 1;
            }
        }
        finally
        {
            if (thumbnail) thumbnailsBusy = false; else foregroundBusy = false;
            if (!thumbnail)
                scheduleForeground();
        }
    }

    private static double milliseconds(JsonObject? response, string key)
        => response?[key]?.GetValue<double>() ?? 0;

    private void onAssetChanged(object sender, FileSystemEventArgs args)
    {
        string relative = Path.GetRelativePath(gameData.ProjectPath, args.FullPath);
        if (relative.StartsWith("Assets" + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
            Interlocked.Exchange(ref assetsChanged, 1);
    }

    internal void InvalidateScene() => invalidate(false);
    internal void InvalidateResources() => invalidate(true);
    private void onDocumentsChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (args.Reset || args.Changes.Any(change => change.Section == "Materials"))
            invalidateMaterials();
        if (args.Reset || args.Changes.Any(change => change.Section is "WorldMaps" or "Blueprints" or "Tilesets" or "AutoTiles" or "Materials" or "Particles" or "Curves" or "Configs" || change.Section == "Maps" && change.IdentityChanged))
            invalidate(false);
    }
    private void onMapChanged(object? sender, MapPreviewChangedEventArgs args)
    {
        if (args.Edit is null || !MapPreviewSceneBuilder.IsActorEdit(args.Edit))
            invalidate(false);
    }
    private void onDataRestored(object? sender, EventArgs args)
    {
        invalidateMaterials();
        invalidate(true);
    }
    private void invalidateMaterials()
    {
        materialRevision++;
        materialSnapshot = null;
    }
    private void onRuntimeChanged(object? sender, EventArgs args) => Dispatcher.UIThread.Post(() => invalidate(true));
    private void invalidate(bool resources)
    {
        if (disposed)
            return;
        ContentRevision++;
        if (resources)
            ResourceRevision++;
        foreach (MapPreviewLease lease in leases)
            lease.Invalidate();
        Invalidated?.Invoke(this, EventArgs.Empty);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        timer.Stop();
        foregroundTimer.Stop();
        assets.Dispose();
        lifetime.Cancel();
        gameData.Documents.ContentChanged -= onDocumentsChanged;
        gameData.Maps.MapPreviewChanged -= onMapChanged;
        gameData.DataRestored -= onDataRestored;
        gameData.DataReloaded -= onDataRestored;
        runtime.Changed -= onRuntimeChanged;
        foreach (MapPreviewLease lease in leases.ToArray())
            lease.Dispose();
        foreground.Dispose();
        thumbnails.Dispose();
        lifetime.Dispose();
    }
}
