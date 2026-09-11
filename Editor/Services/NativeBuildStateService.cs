using System;
using System.IO;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class NativeBuildStateService : IDisposable
{
    private readonly object stateLock = new();
    private readonly string projectPath;
    private readonly string cacheDirectory;
    private readonly ProjectStateWorker stateWorker;
    private readonly string successfulBuildPath;
    private readonly FileSystemWatcher watcher;
    private readonly SemaphoreSlim checkLock = new(1, 1);
    private readonly CancellationTokenSource lifetime = new();
    private volatile bool disposed;

    public NativeBuildStateService(string projectPath, ProjectStateWorker stateWorker)
    {
        this.projectPath = Path.GetFullPath(projectPath);
        this.stateWorker = stateWorker;
        cacheDirectory = Path.Combine(this.projectPath, ProjectToolConstants.EditorCacheDirectory);
        successfulBuildPath = Path.Combine(cacheDirectory, "NativeBuild-Debug.json");
        watcher = new FileSystemWatcher(this.projectPath)
        {
            IncludeSubdirectories = true,
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName,
        };
        watcher.Created += onFileChanged;
        watcher.Deleted += onFileChanged;
        watcher.Renamed += onFileRenamed;
        watcher.Error += (_, _) => RefreshAvailability();
        watcher.EnableRaisingEvents = true;
        RefreshAvailability();
    }

    public bool HasSuccessfulBuild { get; private set; }
    public string Detail { get; private set; } = string.Empty;
    public event EventHandler? Changed;

    public async Task<bool> CheckAsync(CancellationToken cancellationToken = default)
    {
        if (disposed)
            return false;
        using CancellationTokenSource cancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken, lifetime.Token);
        await checkLock.WaitAsync(cancellation.Token);
        try
        {
            (JsonElement result, _) = await stateWorker.ExecuteAsync("native-check", cancellation.Token, "Debug");
            bool current = result.GetProperty("current").GetBoolean();
            string detail = result.GetProperty("detail").GetString() ?? string.Empty;
            return setResult(current, detail);
        }
        finally
        {
            RefreshAvailability();
            checkLock.Release();
        }
    }

    public void RefreshAvailability()
    {
        lock (stateLock)
        {
            if (disposed)
                return;
            bool hasSuccessfulBuild = File.Exists(successfulBuildPath);
            if (HasSuccessfulBuild == hasSuccessfulBuild)
                return;
            HasSuccessfulBuild = hasSuccessfulBuild;
            Changed?.Invoke(this, EventArgs.Empty);
        }
    }

    private bool setResult(bool current, string detail)
    {
        Detail = detail;
        return current;
    }

    private void onFileChanged(object sender, FileSystemEventArgs args)
    {
        if (isBuildRecordPath(args.FullPath))
            RefreshAvailability();
    }

    private void onFileRenamed(object sender, RenamedEventArgs args)
    {
        if (isBuildRecordPath(args.OldFullPath) || isBuildRecordPath(args.FullPath))
            RefreshAvailability();
    }

    private bool isBuildRecordPath(string path)
    {
        return path.Equals(successfulBuildPath, StringComparison.OrdinalIgnoreCase)
            || path.Equals(cacheDirectory, StringComparison.OrdinalIgnoreCase);
    }

    public void Dispose()
    {
        lock (stateLock)
        {
            if (disposed)
                return;
            disposed = true;
        }
        watcher.Dispose();
        lifetime.Cancel();
    }
}
