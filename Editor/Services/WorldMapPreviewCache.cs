using Avalonia.Threading;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed class WorldMapPreviewCache : IDisposable
{
    private readonly ProjectDataStore store;
    private readonly Dictionary<string, Entry> entries = new(StringComparer.Ordinal);
    private readonly Dictionary<string, int> pins = new(StringComparer.Ordinal);
    private readonly Dictionary<string, long> generations = new(StringComparer.Ordinal);
    private long epoch;
    private long nextGeneration;
    private long nextAccess;
    private long loadedBytes;
    private int maximumEntries = 32;
    private long maximumBytes = 256L * 1024L * 1024L;
    private bool dirty;
    private bool queued;
    private bool readyQueued;
    private bool disposed;

    internal WorldMapPreviewCache(ProjectDataStore store)
    {
        this.store = store;
        store.EditOperations.Changed += onWorkCompleted;
        store.Documents.NotificationScopesCompleted += onWorkCompleted;
    }

    internal event EventHandler? Ready;

    internal bool CanLoad => !disposed && !store.IsDisposed
        && !store.EditOperations.IsBusy && !store.Documents.HasPendingNotifications;

    internal Version GetVersion(string key)
    {
        if (!generations.TryGetValue(key, out long generation))
        {
            generation = ++nextGeneration;
            generations.Add(key, generation);
        }
        return new Version(epoch, generation);
    }

    internal bool IsCurrent(string key, Version version) => !disposed && epoch == version.Epoch
        && generations.TryGetValue(key, out long generation) && generation == version.Generation;

    internal IDisposable Acquire(string key)
    {
        if (!disposed)
        {
            pins[key] = pins.GetValueOrDefault(key) + 1;
            if (entries.TryGetValue(key, out Entry? entry))
                entry.Access = ++nextAccess;
        }
        return new Lease(this, key);
    }

    internal WorldChildMapReadResult? Read(string key)
    {
        if (!entries.TryGetValue(key, out Entry? entry))
            return null;
        if (WorldChildMapReadResult.FileVersion.Read(entry.Path) != entry.File)
        {
            Invalidate(key);
            return null;
        }
        entry.Access = ++nextAccess;
        return new WorldChildMapReadResult(key, (JsonObject)entry.Data.DeepClone(), GetVersion(key), entry.Path, entry.File);
    }

    internal void Install(WorldChildMapReadResult read)
    {
        if (entries.TryGetValue(read.Key, out Entry? existing)
            && existing.Path == read.Path && existing.File == read.File)
        {
            existing.Access = ++nextAccess;
            return;
        }
        remove(read.Key);
        Entry entry = new((JsonObject)read.Snapshot!.DeepClone(), read.Path!, read.File!.Value, ++nextAccess);
        entries[read.Key] = entry;
        loadedBytes += entry.File.Length;
        requestMaintenance();
    }

    internal bool TryTake(string key, string path, out JsonObject? data, out long bytes)
    {
        data = null;
        bytes = 0;
        if (!entries.TryGetValue(key, out Entry? entry))
            return false;
        bool current = string.Equals(entry.Path, path, OperatingSystem.IsWindows()
                ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal)
            && WorldChildMapReadResult.FileVersion.Read(entry.Path) == entry.File;
        Invalidate(key);
        if (!current)
            return false;
        data = entry.Data;
        bytes = entry.File.Length;
        return true;
    }

    internal void Invalidate(string key)
    {
        generations.Remove(key);
        remove(key);
    }

    internal void InvalidatePrefix(string prefix)
    {
        foreach (string key in generations.Keys.Concat(entries.Keys).Distinct(StringComparer.Ordinal)
                     .Where(key => ProjectDataStore.keyMatchesPrefix(key, prefix)).ToArray())
            Invalidate(key);
    }

    internal void Clear()
    {
        epoch++;
        generations.Clear();
        entries.Clear();
        loadedBytes = 0;
        nextAccess = 0;
        dirty = false;
    }

    internal void SetBudget(int count, long bytes)
    {
        count = Math.Max(0, count);
        bytes = Math.Max(0, bytes);
        if (maximumEntries == count && maximumBytes == bytes)
            return;
        maximumEntries = count;
        maximumBytes = bytes;
        requestMaintenance();
    }

    private bool overBudget => entries.Count > maximumEntries || loadedBytes > maximumBytes;

    private void release(string key)
    {
        if (disposed || !pins.TryGetValue(key, out int count))
            return;
        if (count > 1)
        {
            pins[key] = count - 1;
            return;
        }
        pins.Remove(key);
        if (entries.TryGetValue(key, out Entry? entry))
            entry.Access = ++nextAccess;
        else
            generations.Remove(key);
        requestMaintenance();
    }

    private void remove(string key)
    {
        if (entries.Remove(key, out Entry? entry))
            loadedBytes -= entry.File.Length;
    }

    private void requestMaintenance()
    {
        if (disposed || !overBudget)
            return;
        dirty = true;
        queueMaintenance();
    }

    private void queueMaintenance()
    {
        if (!dirty || queued || !CanLoad)
            return;
        queued = true;
        Dispatcher.UIThread.Post(maintain, DispatcherPriority.Background);
    }

    private void maintain()
    {
        queued = false;
        if (!CanLoad || !dirty)
            return;
        dirty = false;
        if (!overBudget)
            return;
        string[] candidates = entries.Where(pair => !pins.ContainsKey(pair.Key))
            .OrderBy(pair => pair.Value.Access).Select(pair => pair.Key).ToArray();
        foreach (string key in candidates)
        {
            if (!overBudget)
                break;
            remove(key);
            generations.Remove(key);
        }
    }

    private void onWorkCompleted(object? sender, EventArgs args)
    {
        if (!CanLoad || readyQueued || Ready is null && !dirty)
            return;
        readyQueued = true;
        Dispatcher.UIThread.Post(() =>
        {
            readyQueued = false;
            if (!CanLoad)
                return;
            queueMaintenance();
            Ready?.Invoke(this, EventArgs.Empty);
        }, DispatcherPriority.Background);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        store.EditOperations.Changed -= onWorkCompleted;
        store.Documents.NotificationScopesCompleted -= onWorkCompleted;
        Clear();
        pins.Clear();
        Ready = null;
    }

    internal readonly record struct Version(long Epoch, long Generation);

    private sealed class Entry(JsonObject data, string path, WorldChildMapReadResult.FileVersion file, long access)
    {
        internal JsonObject Data { get; } = data;
        internal string Path { get; } = path;
        internal WorldChildMapReadResult.FileVersion File { get; } = file;
        internal long Access { get; set; } = access;
    }

    private sealed class Lease(WorldMapPreviewCache cache, string key) : IDisposable
    {
        private WorldMapPreviewCache? owner = cache;

        public void Dispose()
        {
            WorldMapPreviewCache? current = owner;
            owner = null;
            current?.release(key);
        }
    }
}
