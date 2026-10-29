using Avalonia;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Controls;

public sealed class WorldMapChildSource : IDisposable
{
    private readonly ProjectDataStore gameData;
    private readonly HashSet<object> consumers = new(ReferenceEqualityComparer.Instance);
    private CancellationTokenSource? pendingLoad;
    private IDisposable? cacheLease;
    private JsonObject? snapshot;
    private bool loadFailed;
    private bool waitingForReady;
    private bool disposed;

    public WorldMapChildSource(ProjectDataStore gameData, MapCatalogEntry entry)
    {
        this.gameData = gameData;
        Key = entry.Key;
        DisplayName = entry.DisplayName;
        Width = entry.Width;
        Height = entry.Height;
        LayerOrder = entry.LayerOrder;
        gameData.Maps.MapPreviewChanged += onMapPreviewChanged;
    }

    public event EventHandler? DataChanged;

    public string Key { get; }
    public string DisplayName { get; }
    public int Width { get; }
    public int Height { get; }
    public IReadOnlyList<string> LayerOrder { get; }
    public Rect? PreviewLightBounds => gameData.Maps.GetPreviewLightBounds(Key);

    public JsonObject? RequestData(object consumer)
    {
        if (disposed || gameData.IsDisposed)
            return null;
        if (consumers.Add(consumer) && consumers.Count == 1)
            cacheLease = gameData.Maps.RetainWorldChildPreview(Key);
        ensureLoad();
        return snapshot;
    }

    public void ReleaseData(object consumer)
    {
        if (consumers.Remove(consumer) && consumers.Count == 0)
        {
            clearData();
            cacheLease?.Dispose();
            cacheLease = null;
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        gameData.Maps.MapPreviewChanged -= onMapPreviewChanged;
        clearData();
        cacheLease?.Dispose();
        cacheLease = null;
        consumers.Clear();
        DataChanged = null;
    }

    private async Task loadAsync(CancellationTokenSource request)
    {
        WorldChildMapReadResult? loaded = null;
        try
        {
            if (!request.IsCancellationRequested)
                loaded = await gameData.Maps.ReadWorldChildMapSnapshotAsync(Key, request.Token).ConfigureAwait(false);
        }
        catch (OperationCanceledException) when (request.IsCancellationRequested)
        {
        }
        catch (Exception exception) when (exception is System.IO.IOException
            or UnauthorizedAccessException or System.Text.Json.JsonException)
        {
        }
        await Dispatcher.UIThread.InvokeAsync(() =>
        {
            if (disposed || request.IsCancellationRequested || !ReferenceEquals(pendingLoad, request))
            {
                request.Dispose();
                return;
            }
            pendingLoad = null;
            request.Dispose();
            bool retry = false;
            try
            {
                snapshot = loaded is null ? null : gameData.Maps.InstallWorldChildMapSnapshot(loaded, out retry);
            }
            catch (Exception exception) when (exception is System.IO.IOException or UnauthorizedAccessException)
            {
                snapshot = null;
            }
            loadFailed = snapshot is null && !retry;
            if (retry)
                ensureLoad();
            DataChanged?.Invoke(this, EventArgs.Empty);
        }, DispatcherPriority.Background);
    }

    private void clearData()
    {
        stopWaiting();
        pendingLoad?.Cancel();
        pendingLoad = null;
        snapshot = null;
        loadFailed = false;
    }

    private void ensureLoad()
    {
        if (disposed || gameData.IsDisposed || consumers.Count == 0
            || snapshot is not null || pendingLoad is not null || loadFailed)
            return;
        if (!gameData.Maps.CanLoadWorldChildPreview)
        {
            if (!waitingForReady)
            {
                waitingForReady = true;
                gameData.Maps.WorldChildPreviewReady += onPreviewReady;
            }
            return;
        }
        stopWaiting();
        startLoad();
    }

    private void startLoad()
    {
        CancellationTokenSource request = new();
        pendingLoad = request;
        Dispatcher.UIThread.Post(() => _ = loadAsync(request), DispatcherPriority.Background);
    }

    private void onPreviewReady(object? sender, EventArgs args)
    {
        stopWaiting();
        ensureLoad();
    }

    private void stopWaiting()
    {
        if (!waitingForReady)
            return;
        waitingForReady = false;
        gameData.Maps.WorldChildPreviewReady -= onPreviewReady;
    }

    private void onMapPreviewChanged(object? sender, MapPreviewChangedEventArgs args)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => onMapPreviewChanged(sender, args));
            return;
        }
        if (disposed)
            return;
        if (!args.ReloadData || args.MapKey is not null && !string.Equals(args.MapKey, Key, StringComparison.Ordinal))
            return;
        if (args.Edit is not null && snapshot is not null)
        {
            args.Edit.ApplyTo(snapshot);
            DataChanged?.Invoke(this, args);
            return;
        }
        clearData();
        ensureLoad();
        DataChanged?.Invoke(this, EventArgs.Empty);
    }
}
