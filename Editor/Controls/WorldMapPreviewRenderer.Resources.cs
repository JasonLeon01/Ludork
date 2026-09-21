using Ludork.Models;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Controls;

internal sealed partial class WorldMapPreviewRenderer
{
    private FileSystemWatcher resourceWatcher = null!;
    private readonly DispatcherTimer resourceRefresh = new() { Interval = TimeSpan.FromMilliseconds(150) };
    private string assetsDirectory = string.Empty;
    private int resourceNotificationPending;
    private readonly Dictionary<ResourceImageKey, EditorThumbnailLease?> resourceImages = [];
    private readonly List<EditorThumbnailLease> retiredImages = [];
    private CancellationTokenSource resourceLifetime = new();

    private void initializeResourceWatcher()
    {
        assetsDirectory = Path.Combine(gameData.ProjectPath, "Assets");
        resourceRefresh.Tick += onResourceRefresh;
        resourceWatcher = new FileSystemWatcher(gameData.ProjectPath)
        {
            IncludeSubdirectories = true,
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName | NotifyFilters.LastWrite | NotifyFilters.Size,
        };
        resourceWatcher.Changed += onResourceChanged;
        resourceWatcher.Created += onResourceChanged;
        resourceWatcher.Deleted += onResourceChanged;
        resourceWatcher.Renamed += onResourceRenamed;
        resourceWatcher.Error += onResourceWatcherError;
        resourceWatcher.EnableRaisingEvents = true;
    }

    private void disposeResourceWatcher()
    {
        resourceWatcher.Dispose();
        resourceRefresh.Stop();
        resourceRefresh.Tick -= onResourceRefresh;
        resourceLifetime.Cancel();
        resourceLifetime.Dispose();
        foreach (EditorThumbnailLease? lease in resourceImages.Values)
            lease?.Dispose();
        resourceImages.Clear();
        releaseRetiredResources();
    }

    private bool isAssetPath(string path)
    {
        return string.Equals(path, assetsDirectory, StringComparison.OrdinalIgnoreCase)
            || path.StartsWith(assetsDirectory + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase);
    }

    private void onResourceChanged(object sender, FileSystemEventArgs args)
    {
        if (isAssetPath(args.FullPath))
            scheduleResourceRefresh();
    }

    private void onResourceRenamed(object sender, RenamedEventArgs args)
    {
        if (isAssetPath(args.FullPath) || isAssetPath(args.OldFullPath))
            scheduleResourceRefresh();
    }

    private void onResourceWatcherError(object sender, ErrorEventArgs args) => scheduleResourceRefresh();

    private void scheduleResourceRefresh()
    {
        if (Interlocked.Exchange(ref resourceNotificationPending, 1) != 0)
            return;
        Dispatcher.UIThread.Post(() =>
        {
            Interlocked.Exchange(ref resourceNotificationPending, 0);
            if (disposed)
                return;
            resourceRefresh.Stop();
            resourceRefresh.Start();
        }, DispatcherPriority.Background);
    }

    private void onResourceRefresh(object? sender, EventArgs args)
    {
        resourceRefresh.Stop();
        if (disposed)
            return;
        clearResourceCache();
        ClearPendingWork();
        clearPreviewCache();
        actorRenderer?.InvalidateResources();
        PreviewChanged?.Invoke(this, EventArgs.Empty);
    }

    private void clearResourceCache()
    {
        resourceLifetime.Cancel();
        resourceLifetime.Dispose();
        resourceLifetime = new CancellationTokenSource();
        foreach (EditorThumbnailLease? lease in resourceImages.Values)
        {
            if (lease is not null)
                retiredImages.Add(lease);
        }
        resourceImages.Clear();
        autoTileRenderer.Dispose();
    }

    private Bitmap? getResourceImage(bool autoTile, string? key)
    {
        if (string.IsNullOrWhiteSpace(key) || disposed)
            return null;
        ResourceImageKey resourceKey = new(autoTile, key);
        if (resourceImages.TryGetValue(resourceKey, out EditorThumbnailLease? lease))
            return lease?.Bitmap;
        resourceImages[resourceKey] = null;
        IReadOnlyDictionary<string, TilesetSnapshot> definitions = autoTile ? gameData.Assets.AutoTileData : gameData.Assets.TilesetData;
        if (definitions.TryGetValue(key, out TilesetSnapshot? definition))
        {
            _ = loadResourceImageAsync(resourceKey, definition.FileName, gameData.Thumbnails, resourceLifetime.Token);
        }
        return null;
    }

    private async Task loadResourceImageAsync(
        ResourceImageKey key,
        string fileName,
        EditorThumbnailService thumbnails,
        CancellationToken cancellationToken)
    {
        EditorThumbnailLease? lease = null;
        try
        {
            string? path = await Task.Run(() =>
                GameAssetPath.TryResolveExistingFile(gameData.ProjectPath, fileName, out string resolved) ? resolved : null,
                cancellationToken).ConfigureAwait(false);
            if (path is not null)
                lease = await thumbnails.AcquireAsync(path, 0, cancellationToken).ConfigureAwait(false);
        }
        catch (OperationCanceledException)
        {
        }
        await Dispatcher.UIThread.InvokeAsync(() =>
        {
            if (disposed || cancellationToken.IsCancellationRequested)
            {
                lease?.Dispose();
                return;
            }
            resourceImages[key] = lease;
            ClearPendingWork();
            clearPreviewCache();
            PreviewChanged?.Invoke(this, EventArgs.Empty);
        }, DispatcherPriority.Background);
    }

    private void releaseRetiredResources()
    {
        foreach (EditorThumbnailLease lease in retiredImages)
            lease.Dispose();
        retiredImages.Clear();
    }

    private readonly record struct ResourceImageKey(bool AutoTile, string Key);
}
