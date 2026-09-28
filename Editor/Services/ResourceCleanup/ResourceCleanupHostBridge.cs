using Avalonia.Threading;
using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.ResourceCleanup;

internal sealed class ResourceCleanupHostBridge : IResourceCleanupHost
{
    private readonly ProjectDataStore gameData;
    private readonly Action prepareProject;
    private readonly Action<IReadOnlyList<string>> notifyTrashedResources;
    private readonly SemaphoreSlim operationGate = new(1, 1);
    private readonly ResourceCleanupScanner scanner = new();
    private ResourceCleanupSnapshot? snapshot;
    private long documentRevision;

    public ResourceCleanupHostBridge(ProjectDataStore gameData, Action prepareProject,
        Action<IReadOnlyList<string>> notifyTrashedResources)
    {
        this.gameData = gameData;
        this.prepareProject = prepareProject;
        this.notifyTrashedResources = notifyTrashedResources;
    }

    public string ProjectPath => gameData.ProjectPath;

    public async Task<IReadOnlyList<string>> ReadKeepPathsAsync(CancellationToken cancellationToken)
    {
        return await Task.Run(() =>
        {
            cancellationToken.ThrowIfCancellationRequested();
            string path = ResourceCleanupFileSystem.ResolveSafePath(ProjectPath, ResourceCleanupFileSystem.KeepListPath, allowMissing: true);
            return File.Exists(path) ? normalizeKeepPaths(File.ReadAllLines(path, Encoding.UTF8)) : [];
        }, cancellationToken);
    }

    public async Task SaveKeepPathsAsync(IReadOnlyList<string> paths, CancellationToken cancellationToken)
    {
        string[] requested = paths.ToArray();
        await operationGate.WaitAsync(cancellationToken);
        try
        {
            snapshot = null;
            await Task.Run(() =>
            {
                string[] normalized = normalizeKeepPaths(requested);
                string path = ResourceCleanupFileSystem.ResolveSafePath(ProjectPath, ResourceCleanupFileSystem.KeepListPath, allowMissing: true);
                cancellationToken.ThrowIfCancellationRequested();
                if (normalized.Length == 0)
                {
                    if (File.Exists(path))
                        File.Delete(path);
                    return;
                }
                string content = string.Join('\n', normalized) + "\n";
                if (File.Exists(path) && File.ReadAllText(path, Encoding.UTF8) == content)
                    return;
                byte[] bytes = new UTF8Encoding(false, true).GetBytes(content);
                FilePersistence.WriteAtomic(path, stream => stream.Write(bytes), () =>
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    ResourceCleanupFileSystem.ResolveSafePath(ProjectPath, ResourceCleanupFileSystem.KeepListPath, allowMissing: true);
                });
            }, cancellationToken);
        }
        finally
        {
            operationGate.Release();
        }
    }

    public async Task<ResourceCleanupReport> ScanAsync(IReadOnlyList<string> nativeKeepPaths,
        IReadOnlyList<string> userKeepPaths, IProgress<ResourceCleanupProgress>? progress,
        CancellationToken cancellationToken)
    {
        string[] nativePaths = nativeKeepPaths.ToArray();
        string[] userPaths = userKeepPaths.ToArray();
        await operationGate.WaitAsync(cancellationToken);
        try
        {
            snapshot = null;
            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                ensureSaved();
                documentRevision = gameData.Documents.Revision;
            });
            ResourceCleanupSnapshot result = await Task.Run(() => scanner.Scan(ProjectPath, nativePaths, userPaths,
                progress, cancellationToken), cancellationToken);
            await Dispatcher.UIThread.InvokeAsync(ensureUnchanged);
            cancellationToken.ThrowIfCancellationRequested();
            snapshot = result;
            return result.Report;
        }
        finally
        {
            operationGate.Release();
        }
    }

    public async Task<ResourceCleanupTrashResult> TrashAsync(string reportId,
        IProgress<ResourceCleanupProgress>? progress, CancellationToken cancellationToken)
    {
        await operationGate.WaitAsync(cancellationToken);
        List<string> recycled = [];
        List<string> recycledPaths = [];
        List<string> pendingSynchronization = [];
        try
        {
            ResourceCleanupSnapshot current = snapshot is { } available && available.Report.Id == reportId
                && available.Report.CanTrash ? available
                : throw new InvalidOperationException("The cleanup report is unavailable. Scan the project again.");
            snapshot = null;
            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                ensureSaved();
                ensureUnchanged();
            });
            await Task.Run(() => scanner.Validate(current, progress, cancellationToken), cancellationToken);
            await Dispatcher.UIThread.InvokeAsync(ensureUnchanged);
            return await SystemTrashService.RunAsync(trash =>
            {
                string error = string.Empty;
                bool cancelled = false;
                foreach (string relativePath in current.DeletionOrder)
                {
                    try
                    {
                        cancellationToken.ThrowIfCancellationRequested();
                        Dispatcher.UIThread.InvokeAsync(ensureUnchanged).GetAwaiter().GetResult();
                        string absolutePath = scanner.ValidateCandidate(current, relativePath, cancellationToken);
                        progress?.Report(new ResourceCleanupProgress("Recycle", recycled.Count, current.DeletionOrder.Count, relativePath));
                        trash.MoveToTrash(absolutePath);
                        recycled.Add(relativePath);
                        recycledPaths.Add(absolutePath);
                        pendingSynchronization.Add(absolutePath);
                        Dispatcher.UIThread.InvokeAsync(() =>
                        {
                            string? conflict = null;
                            try
                            {
                                ensureUnchanged();
                            }
                            catch (InvalidOperationException exception)
                            {
                                conflict = exception.Message;
                            }
                            gameData.AcceptTrashedResources([absolutePath], false);
                            pendingSynchronization.Remove(absolutePath);
                            if (conflict is not null)
                                throw new InvalidOperationException(conflict);
                            documentRevision = gameData.Documents.Revision;
                        }).GetAwaiter().GetResult();
                    }
                    catch (OperationCanceledException)
                    {
                        cancelled = true;
                        break;
                    }
                    catch (Exception exception)
                    {
                        error = relativePath + ": " + exception.Message;
                        break;
                    }
                }
                progress?.Report(new ResourceCleanupProgress("Recycle", recycled.Count, current.DeletionOrder.Count, string.Empty));
                return new ResourceCleanupTrashResult(recycled.ToArray(), current.DeletionOrder.Except(recycled, StringComparer.Ordinal).ToArray(), error, cancelled);
            }, cancellationToken);
        }
        finally
        {
            try
            {
                if (recycledPaths.Count != 0)
                    await Dispatcher.UIThread.InvokeAsync(() =>
                    {
                        try
                        {
                            if (pendingSynchronization.Count != 0)
                                gameData.AcceptTrashedResources(pendingSynchronization.ToArray(), false);
                        }
                        finally
                        {
                            gameData.CompleteTrashedResources();
                            notifyTrashedResources(recycledPaths.ToArray());
                        }
                    });
            }
            finally
            {
                operationGate.Release();
            }
        }
    }

    private string[] normalizeKeepPaths(IEnumerable<string> paths)
    {
        string[] result = paths.Where(path => !string.IsNullOrWhiteSpace(path))
            .Select(ResourceCleanupFileSystem.NormalizeKeepPath).Distinct(StringComparer.Ordinal).ToArray();
        foreach (string path in result)
            ResourceCleanupFileSystem.ResolveSafePath(ProjectPath, path, allowMissing: true);
        return result;
    }

    private void ensureSaved()
    {
        prepareProject();
        if (gameData.IsModified || gameData.Documents.IsModified)
            throw new InvalidOperationException("Save the project before scanning or recycling resources.");
    }

    private void ensureUnchanged()
    {
        prepareProject();
        if (gameData.IsModified || gameData.Documents.Revision != documentRevision)
            throw new InvalidOperationException("The project changed after scanning. Scan the project again.");
    }
}
