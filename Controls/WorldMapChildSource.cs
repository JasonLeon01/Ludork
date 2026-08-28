using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Templates;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Controls;

public sealed class WorldMapChildSource
{
    private readonly Func<JsonObject?> loadData;
    private readonly Func<bool>? isDataLoaded;
    private readonly Func<Task<JsonObject?>>? readDataAsync;
    private readonly Func<JsonObject, JsonObject?>? installData;
    private JsonObject? data;
    private bool loadScheduled;
    private bool loadFailed;

    public WorldMapChildSource(
        string key,
        string displayName,
        int width,
        int height,
        Func<JsonObject?> loadData,
        IReadOnlyList<string>? layerOrder = null,
        Func<bool>? isDataLoaded = null,
        Func<Task<JsonObject?>>? readDataAsync = null,
        Func<JsonObject, JsonObject?>? installData = null)
    {
        Key = key;
        DisplayName = displayName;
        Width = width;
        Height = height;
        this.loadData = loadData;
        this.isDataLoaded = isDataLoaded;
        this.readDataAsync = readDataAsync;
        this.installData = installData;
        LayerOrder = layerOrder ?? [];
    }

    public string Key { get; }
    public string DisplayName { get; }
    public int Width { get; }
    public int Height { get; }
    public bool HasData => isDataLoaded?.Invoke() ?? data is not null;
    public IReadOnlyList<string> LayerOrder { get; }

    public JsonObject? LoadData()
    {
        if (isDataLoaded is not null)
        {
            JsonObject? loaded = loadData();
            loadFailed = loaded is null;
            return loaded;
        }
        data ??= loadData();
        loadFailed = data is null;
        return data;
    }

    public void ReleaseData()
    {
        if (isDataLoaded is null)
            data = null;
    }

    public void ScheduleLoad(Action completed)
    {
        if (HasData || loadScheduled || loadFailed)
            return;
        loadScheduled = true;
        if (readDataAsync is not null && installData is not null)
        {
            _ = loadInBackground(completed);
            return;
        }
        Dispatcher.UIThread.Post(
            () =>
            {
                try
                {
                    LoadData();
                }
                finally
                {
                    loadScheduled = false;
                    completed();
                }
            },
            DispatcherPriority.Background);
    }

    private async Task loadInBackground(Action completed)
    {
        JsonObject? loaded = await readDataAsync!().ConfigureAwait(false);
        await Dispatcher.UIThread.InvokeAsync(() =>
        {
            JsonObject? installed = loaded is null ? null : installData!(loaded);
            loadFailed = installed is null;
            loadScheduled = false;
            completed();
        });
    }
}

public sealed class WorldMapPlacementChangedEventArgs(
    string worldKey,
    string childMapKey,
    int x,
    int y) : EventArgs
{
    public string WorldKey { get; } = worldKey;
    public string ChildMapKey { get; } = childMapKey;
    public int X { get; } = x;
    public int Y { get; } = y;
}

public sealed class WorldMapPlacementRemovedEventArgs(
    string worldKey,
    string childMapKey) : EventArgs
{
    public string WorldKey { get; } = worldKey;
    public string ChildMapKey { get; } = childMapKey;
}

public sealed class WorldMapCellSelectedEventArgs(int x, int y) : EventArgs
{
    public int X { get; } = x;
    public int Y { get; } = y;
}

