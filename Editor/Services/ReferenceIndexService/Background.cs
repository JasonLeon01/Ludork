using Avalonia.Threading;
using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ReferenceIndexService
{
    private long version;
    private long schemaVersion;
    private long publication;
    private bool activated;
    private DispatcherTimer? refreshTimer;
    private DispatcherTimer? fileTimer;
    private CancellationTokenSource? backgroundCancellation;
    private Task<ReferenceIndexSnapshotBuilder.Result>? backgroundTask;
    private ReferenceIndexSnapshotBuilder? backgroundBuilder;
    private ReferenceInputFiles? inputFiles;
    private ReferenceInputFiles? observedFiles;
    private readonly Dictionary<(string Section, string Key), CapturedDocument> capturedDocuments = [];

    public ReferenceIndexSnapshot? CurrentSnapshot { get; private set; }
    public bool IsUpdating { get; private set; }
    public string? UpdateError { get; private set; }
    public event EventHandler? SnapshotChanged;

    internal void EnableBackgroundUpdates()
    {
        if (refreshTimer is not null || disposed)
            return;
        refreshTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(150) };
        refreshTimer.Tick += onRefreshTick;
        fileTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        fileTimer.Tick += onFileTick;
        if (activated)
            fileTimer.Start();
    }

    public void RequestRefresh()
    {
        if (disposed)
            return;
        EnableBackgroundUpdates();
        activate();
        try
        {
            if (UpdateError is not null || inputFiles is not null && !inputFiles.IsCurrent())
                MarkDirty();
            if (CurrentSnapshot is null || CurrentSnapshot.Version != version || dirty || pendingDocuments.Count != 0)
                scheduleRefresh();
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            IsUpdating = false;
            UpdateError = exception.Message;
            SnapshotChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    private void activate()
    {
        if (activated)
            return;
        activated = true;
        fileTimer?.Start();
    }

    private void invalidateBackground(bool schemaChanged)
    {
        version++;
        if (schemaChanged)
            schemaVersion++;
        backgroundCancellation?.Cancel();
        if (activated)
            scheduleRefresh();
    }

    private void scheduleRefresh()
    {
        if (refreshTimer is null || disposed)
            return;
        refreshTimer.Stop();
        refreshTimer.Start();
        IsUpdating = true;
        UpdateError = null;
        SnapshotChanged?.Invoke(this, EventArgs.Empty);
    }

    private void onFileTick(object? sender, EventArgs args)
    {
        if (disposed || backgroundTask is not null || gameData.Documents.HasPendingNotifications)
            return;
        try
        {
            if (observedFiles is not null && !observedFiles.IsCurrent())
                MarkDirty();
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            UpdateError = exception.Message;
            SnapshotChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    private void onRefreshTick(object? sender, EventArgs args)
    {
        refreshTimer!.Stop();
        if (disposed)
            return;
        if (backgroundTask is not null || gameData.Documents.HasPendingNotifications)
        {
            refreshTimer.Start();
            return;
        }
        if (CurrentSnapshot?.Version == version && !dirty && pendingDocuments.Count == 0)
        {
            IsUpdating = false;
            SnapshotChanged?.Invoke(this, EventArgs.Empty);
            return;
        }
        ReferenceBuildInput input;
        try
        {
            input = captureBuildInput();
            observedFiles = ReferenceInputFiles.Capture(gameData.ProjectPath,
                input.Documents.Where(document => document.Data is null).Select(document => document.Path));
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or InvalidOperationException)
        {
            IsUpdating = false;
            UpdateError = exception.Message;
            SnapshotChanged?.Invoke(this, EventArgs.Empty);
            return;
        }
        backgroundBuilder ??= new ReferenceIndexSnapshotBuilder(gameData.ProjectPath);
        ReferenceIndexSnapshotBuilder builder = backgroundBuilder;
        CancellationTokenSource cancellation = new();
        backgroundCancellation = cancellation;
        long previousPublication = publication;
        Task<ReferenceIndexSnapshotBuilder.Result> task = Task.Run(() => builder.Build(input, cancellation.Token));
        backgroundTask = task;
        _ = task.ContinueWith(completed => Dispatcher.UIThread.Post(() =>
            completeBackground(completed, cancellation, input.Version, previousPublication)), TaskScheduler.Default);
    }

    private void completeBackground(Task<ReferenceIndexSnapshotBuilder.Result> task,
        CancellationTokenSource cancellation, long requestedVersion, long previousPublication)
    {
        bool canceled = cancellation.IsCancellationRequested;
        Exception? failure = task.Exception?.GetBaseException();
        cancellation.Dispose();
        if (ReferenceEquals(backgroundCancellation, cancellation))
            backgroundCancellation = null;
        backgroundTask = null;
        if (disposed)
            return;
        if (canceled || requestedVersion != version || previousPublication != publication)
        {
            if (CurrentSnapshot?.Version != version || dirty || pendingDocuments.Count != 0)
                scheduleRefresh();
            else
            {
                IsUpdating = false;
                SnapshotChanged?.Invoke(this, EventArgs.Empty);
            }
            return;
        }
        if (failure is not null)
        {
            IsUpdating = false;
            UpdateError = failure.Message;
            SnapshotChanged?.Invoke(this, EventArgs.Empty);
            return;
        }
        try
        {
            ReferenceIndexSnapshotBuilder.Result result = task.GetAwaiter().GetResult();
            if (!result.Files.IsCurrent())
            {
                MarkDirty();
                return;
            }
            importSnapshot(result.Snapshot);
            inputFiles = result.Files;
            observedFiles = result.Files;
            metadataRevision = metadataService.Revision;
            publish(result.Snapshot);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or InvalidOperationException)
        {
            IsUpdating = false;
            UpdateError = exception.Message;
            SnapshotChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    private ReferenceBuildInput captureBuildInput()
    {
        List<ReferenceBuildDocument> documents = [];
        HashSet<(string Section, string Key)> present = [];
        foreach ((string section, string key, JsonObject data) in gameData.ReferenceDocuments)
        {
            present.Add((section, key));
            EditorDocument? document = gameData.Documents.Find(section, key);
            long revision = document?.Revision ?? -1;
            if (!capturedDocuments.TryGetValue((section, key), out CapturedDocument? captured)
                || captured.Revision != revision || !ReferenceEquals(captured.Original, data))
            {
                string type = ReferenceIndexSnapshot.DataRoots.Single(pair => pair.Value == section).Key;
                string nodeKey = type == "blueprint" ? BlueprintPrefix + key.Replace('/', '.') : key;
                string path = ReferenceIndexSnapshot.ResolvePath(gameData.ProjectPath, type + ":" + nodeKey,
                    new Dictionary<string, string>());
                captured = new CapturedDocument(data, revision,
                    new ReferenceBuildDocument(section, key, path, (JsonObject)data.DeepClone()));
                capturedDocuments[(section, key)] = captured;
            }
            documents.Add(captured.Input);
        }
        foreach ((string Section, string Key) key in capturedDocuments.Keys.Where(key => !present.Contains(key)).ToArray())
            capturedDocuments.Remove(key);
        foreach (MapCatalogEntry entry in gameData.Maps.MapCatalog.Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap))
            if (!present.Contains(("Maps", entry.Key)))
                documents.Add(new ReferenceBuildDocument("Maps", entry.Key, gameData.Maps.getReadableMapDataPath(entry.Key), null, entry with { LayerOrder = entry.LayerOrder.ToArray(), ActorTags = entry.ActorTags.ToArray() }));
        return new ReferenceBuildInput(gameData.ProjectPath, version, schemaVersion, documents);
    }

    private IEnumerable<string> unloadedMapPaths() => gameData.Maps.MapCatalog
        .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap && !gameData.Maps.LoadedMapData.ContainsKey(entry.Key))
        .Select(entry => gameData.Maps.getReadableMapDataPath(entry.Key));

    private void importSnapshot(ReferenceIndexSnapshot snapshot)
    {
        nodes.Clear();
        generalMemberTypes.Clear();
        referencesBySource.Clear();
        referencedByTarget.Clear();
        seen.Clear();
        declaredNodes.Clear();
        mapReferenceCache.Clear();
        pendingDocuments.Clear();
        foreach (ReferenceNode node in snapshot.Nodes)
            nodes[node.Id] = node;
        foreach (KeyValuePair<string, string> member in snapshot.GeneralMemberTypes)
            generalMemberTypes[member.Key] = member.Value;
        declaredNodes.UnionWith(snapshot.DeclaredNodes);
        foreach (ReferenceRecord record in snapshot.References)
            addReference(record.Source, record.Target, record.Kind, record.Path);
        foreach (MapCatalogEntry entry in gameData.Maps.MapCatalog.Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap))
            mapReferenceCache[entry.Key] = snapshot.GetOutgoing(nodeId("map", entry.Key));
        allWorldChildMapReferencesBuilt = true;
        dirty = false;
    }

    private void publish(ReferenceIndexSnapshot snapshot)
    {
        CurrentSnapshot = snapshot;
        publication++;
        IsUpdating = false;
        UpdateError = null;
        refreshTimer?.Stop();
        SnapshotChanged?.Invoke(this, EventArgs.Empty);
    }

    private void disposeBackground()
    {
        IsUpdating = false;
        refreshTimer?.Stop();
        fileTimer?.Stop();
        backgroundCancellation?.Cancel();
        if (backgroundTask is Task<ReferenceIndexSnapshotBuilder.Result> task)
        {
            ReferenceIndexSnapshotBuilder? builder = backgroundBuilder;
            CancellationTokenSource? cancellation = backgroundCancellation;
            _ = task.ContinueWith(completed =>
            {
                _ = completed.Exception;
                builder?.Dispose();
                cancellation?.Dispose();
            }, TaskScheduler.Default);
        }
        else
            backgroundBuilder?.Dispose();
        capturedDocuments.Clear();
        SnapshotChanged = null;
    }

    private sealed record CapturedDocument(JsonObject Original, long Revision, ReferenceBuildDocument Input);
}
