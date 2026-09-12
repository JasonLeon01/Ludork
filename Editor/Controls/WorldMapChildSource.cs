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
    private readonly GameDataService gameData;
    private readonly HashSet<object> consumers = new(ReferenceEqualityComparer.Instance);
    private CancellationTokenSource? pendingLoad;
    private JsonObject? snapshot;
    private bool loadFailed;
    private bool disposed;

    public WorldMapChildSource(GameDataService gameData, MapCatalogEntry entry)
    {
        this.gameData = gameData;
        Key = entry.Key;
        DisplayName = entry.DisplayName;
        Width = entry.Width;
        Height = entry.Height;
        LayerOrder = entry.LayerOrder;
        gameData.MapPreviewChanged += onMapPreviewChanged;
    }

    public event EventHandler? DataChanged;

    public string Key { get; }
    public string DisplayName { get; }
    public int Width { get; }
    public int Height { get; }
    public IReadOnlyList<string> LayerOrder { get; }

    public JsonObject? RequestData(object consumer)
    {
        if (disposed)
            return null;
        consumers.Add(consumer);
        if (snapshot is null && pendingLoad is null && !loadFailed)
        {
            CancellationTokenSource request = new();
            pendingLoad = request;
            Dispatcher.UIThread.Post(() => _ = loadAsync(request), DispatcherPriority.Background);
        }
        return snapshot;
    }

    public void ReleaseData(object consumer)
    {
        if (consumers.Remove(consumer) && consumers.Count == 0)
            clearData();
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        gameData.MapPreviewChanged -= onMapPreviewChanged;
        clearData();
        consumers.Clear();
        DataChanged = null;
    }

    private async Task loadAsync(CancellationTokenSource request)
    {
        JsonObject? loaded = null;
        try
        {
            if (!request.IsCancellationRequested)
                loaded = await gameData.ReadWorldChildMapSnapshotAsync(Key, request.Token).ConfigureAwait(false);
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
            try
            {
                snapshot = loaded is null ? null : gameData.InstallWorldChildMapSnapshot(Key, loaded);
            }
            catch (Exception exception) when (exception is System.IO.IOException or UnauthorizedAccessException)
            {
                snapshot = null;
            }
            loadFailed = snapshot is null;
            DataChanged?.Invoke(this, EventArgs.Empty);
        }, DispatcherPriority.Background);
    }

    private void clearData()
    {
        pendingLoad?.Cancel();
        pendingLoad = null;
        snapshot = null;
        loadFailed = false;
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
        clearData();
        DataChanged?.Invoke(this, EventArgs.Empty);
    }
}
