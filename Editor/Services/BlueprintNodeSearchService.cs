using Avalonia.Threading;
using Ludork.Models;
using MoonSharp.Interpreter;
using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class BlueprintNodeSearchService : IDisposable
{
    private readonly ProjectDataStore gameData;
    private readonly DispatcherTimer refreshTimer;
    private FileSystemWatcher? watcher;
    private CancellationTokenSource? rebuildCancellation;
    private long version;
    private bool started;
    private bool disposed;
    private HashSet<string> enumModules = [];

    internal BlueprintNodeSearchService(ProjectDataStore gameData, CancellationToken cancellationToken)
    {
        this.gameData = gameData;
        try
        {
            Types = gameData.Metadata.CaptureTypeSnapshot(cancellationToken);
            IsCurrent = true;
        }
        catch (Exception error) when (error is not OperationCanceledException)
        {
            Error = error.Message;
        }
        refreshTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(150) };
        refreshTimer.Tick += onRefresh;
    }

    public LuaMetadataTypeSnapshot? Types { get; private set; }
    public string? Error { get; private set; }
    public bool IsCurrent { get; private set; }
    public event EventHandler? Invalidated;
    public event EventHandler<BlueprintNodeSearchValidationEventArgs>? Validating;
    public event EventHandler? Changed;

    internal void Start()
    {
        if (started || disposed)
            return;
        started = true;
        enumModules = gameData.GetProjectEnumCatalog().Definitions.Keys.ToHashSet(StringComparer.Ordinal);
        gameData.Documents.ContentChanged += onDocumentsChanged;
        gameData.DataReloaded += onDataReloaded;
        string scripts = Path.Combine(gameData.ProjectPath, "Scripts");
        if (!Directory.Exists(scripts))
            return;
        watcher = new FileSystemWatcher(scripts)
        {
            IncludeSubdirectories = true,
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName | NotifyFilters.LastWrite | NotifyFilters.Size,
        };
        watcher.Changed += onFileChanged;
        watcher.Created += onFileChanged;
        watcher.Deleted += onFileChanged;
        watcher.Renamed += onFileRenamed;
        watcher.Error += onWatcherError;
        watcher.EnableRaisingEvents = true;
    }

    private bool affectsTypes(string path)
    {
        string relative = Path.GetRelativePath(Path.Combine(gameData.ProjectPath, "Scripts"), path).Replace('\\', '/');
        return relative.EndsWith("_meta.lua", StringComparison.OrdinalIgnoreCase)
            || relative.StartsWith("stub/", StringComparison.OrdinalIgnoreCase) && relative.EndsWith(".d.lua", StringComparison.OrdinalIgnoreCase)
            || relative.StartsWith("Enums/", StringComparison.OrdinalIgnoreCase) && relative.EndsWith(".lua", StringComparison.OrdinalIgnoreCase)
            || !Path.HasExtension(relative);
    }

    private void onFileChanged(object sender, FileSystemEventArgs args)
    {
        if (affectsTypes(args.FullPath))
            Dispatcher.UIThread.Post(RequestRefresh);
    }

    private void onFileRenamed(object sender, RenamedEventArgs args)
    {
        if (affectsTypes(args.FullPath) || affectsTypes(args.OldFullPath))
            Dispatcher.UIThread.Post(RequestRefresh);
    }

    private void onWatcherError(object sender, ErrorEventArgs args) => Dispatcher.UIThread.Post(RequestRefresh);

    private void onDocumentsChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (args.Reset)
        {
            RequestRefresh();
            return;
        }
        if (!args.Changes.Any(change => change.Section == "General" && change.IdentityChanged))
            return;
        HashSet<string> modules = gameData.GetProjectEnumCatalog().Definitions.Keys.ToHashSet(StringComparer.Ordinal);
        if (enumModules.SetEquals(modules))
            return;
        enumModules = modules;
        RequestRefresh();
    }

    private void onDataReloaded(object? sender, EventArgs args) => RequestRefresh();

    internal void RequestRefresh()
    {
        if (disposed)
            return;
        version++;
        IsCurrent = false;
        rebuildCancellation?.Cancel();
        Invalidated?.Invoke(this, EventArgs.Empty);
        refreshTimer.Stop();
        refreshTimer.Start();
    }

    private async void onRefresh(object? sender, EventArgs args)
    {
        refreshTimer.Stop();
        if (disposed)
            return;
        if (gameData.EditOperations.IsBusy || gameData.Documents.HasPendingNotifications)
        {
            refreshTimer.Start();
            return;
        }
        long requestVersion = version;
        ProjectEnumCatalog enums = gameData.GetProjectEnumCatalog();
        CancellationTokenSource cancellation = new();
        rebuildCancellation = cancellation;
        (LuaMetadataService Metadata, LuaMetadataTypeSnapshot Types) prepared;
        try
        {
            prepared = await Task.Run(() =>
            {
                LuaMetadataService metadata = new(gameData.ProjectPath, strictReads: true,
                    cancellationToken: cancellation.Token, enums: new LuaEnumService(gameData.ProjectPath, () => enums));
                return (metadata, metadata.CaptureTypeSnapshot(cancellation.Token));
            }, cancellation.Token);
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested)
        {
            return;
        }
        catch (Exception error)
        {
            if (disposed || requestVersion != version)
                return;
            Error = error.Message;
            Changed?.Invoke(this, EventArgs.Empty);
            return;
        }
        finally
        {
            if (ReferenceEquals(rebuildCancellation, cancellation))
                rebuildCancellation = null;
            cancellation.Dispose();
        }
        if (disposed || requestVersion != version)
            return;
        try
        {
            using IDisposable preparedRead = prepared.Metadata.BeginPreparedRead();
            using BlueprintClassResolver classes = new(gameData, prepared.Metadata);
            Validating?.Invoke(this, new BlueprintNodeSearchValidationEventArgs(prepared.Metadata, classes));
        }
        catch (Exception error) when (error is IOException or InvalidDataException
            or UnauthorizedAccessException or InterpreterException or ArgumentException)
        {
            Error = error.Message;
            Changed?.Invoke(this, EventArgs.Empty);
            return;
        }
        gameData.Metadata.AdoptPreparedState(prepared.Metadata);
        using IDisposable read = gameData.Metadata.BeginPreparedRead();
        gameData.BlueprintClasses.Invalidate();
        Types = prepared.Types;
        Error = null;
        IsCurrent = true;
        using IDisposable batch = gameData.BlueprintClasses.BeginBatch();
        Changed?.Invoke(this, EventArgs.Empty);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        IsCurrent = false;
        version++;
        refreshTimer.Stop();
        refreshTimer.Tick -= onRefresh;
        rebuildCancellation?.Cancel();
        watcher?.Dispose();
        gameData.Documents.ContentChanged -= onDocumentsChanged;
        gameData.DataReloaded -= onDataReloaded;
        Invalidated?.Invoke(this, EventArgs.Empty);
    }
}
