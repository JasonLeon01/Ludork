using Avalonia.Threading;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Ludork.Models;

namespace Ludork.ViewModels;

public sealed partial class FileExplorerViewModel
{
    private readonly Dictionary<Guid, DocumentPath> visibleDocumentPaths = [];
    private readonly Dictionary<string, FileExplorerEntryViewModel> entriesByPath = new(PathComparer);
    private readonly Dictionary<string, bool> pendingDocumentUpdates = new(PathComparer);
    private bool documentProjectionResetRequired;
    private bool previewRefreshRequired;
    private bool applyingDocumentUpdates;
    private bool documentUpdatesScheduled;
    private async Task<(bool Handled, string? Error)> tryMoveManagedPathAsync(string source, string destination,
        IProgress<EditorOperationProgress>? progress)
    {
        string assetsRoot = Path.Combine(projectPath, "Assets");
        if (EditorPathSandbox.IsSameOrChildPath(assetsRoot, source)
            && EditorPathSandbox.IsSameOrChildPath(assetsRoot, destination))
        {
            try
            {
                changing(new FileExplorerFilesChangedEventArgs([], [(source, destination)], []));
                bool moved = await gameData.AssetPaths.MoveAsync([(source, destination)], progress);
                return (true, moved ? null : LocaleService.Get("MOVE_FILE_FAILED"));
            }
            catch (Exception exception) when (exception is InvalidOperationException or IOException or UnauthorizedAccessException)
            {
                return (true, exception.Message);
            }
        }
        if (EditorPathSandbox.IsSameOrChildPath(assetsRoot, source))
            return (true, LocaleService.Get("MOVE_FILE_FAILED"));
        if (!gameData.IsManagedPath(source))
            return (false, null);
        try
        {
            changing(new FileExplorerFilesChangedEventArgs([], [(source, destination)], []));
            return await gameData.TryRenameManagedPathAsync(source, destination, progress);
        }
        catch (Exception exception) when (exception is InvalidOperationException or IOException or UnauthorizedAccessException)
        {
            return (true, exception.Message);
        }
    }

    private async Task<(bool Handled, string? Error)> tryCopyManagedPathAsync(string source, string destination,
        IProgress<EditorOperationProgress>? progress)
    {
        if (EditorPathSandbox.IsSameOrChildPath(Path.Combine(projectPath, "Assets"), source)
            || !gameData.IsManagedPath(source))
            return (false, null);
        try
        {
            changing(new FileExplorerFilesChangedEventArgs([destination], [], []));
            return await gameData.TryCopyManagedPathAsync(source, destination, progress);
        }
        catch (Exception exception) when (exception is InvalidOperationException or IOException or UnauthorizedAccessException)
        {
            return (true, exception.Message);
        }
    }

    private bool isVisibleDirectory(string path)
    {
        return gameData.AssetPaths.DirectoryExists(path) || visibleDocumentPaths.Values.Any(document =>
            document.Exists && document.Paths.Any(documentPath => !pathsEqual(documentPath, path)
                && EditorPathSandbox.IsSameOrChildPath(path, documentPath)));
    }

    private bool pathExists(string path)
    {
        return gameData.AssetPaths.FileExists(path) || isVisibleDirectory(path) || gameData.GetDocumentByPath(path)?.Exists == true;
    }

    private void onDocumentsChanged(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (disposed)
            return;
        HashSet<Guid> contentChanged = args.Changes.Where(change => change.ContentChanged || change.IdentityChanged)
            .Select(change => change.DocumentId).ToHashSet();
        List<(string OldPath, string NewPath)> moved = [];
        List<string> deleted = [];
        bool structureChanged = false;
        foreach (EditorDocument document in args.AffectedDocuments)
        {
            DocumentPath? next = gameData.Documents.IsRegistered(document) ? captureDocumentPath(document) : null;
            structureChanged |= updateDocumentPath(document.Id, next, contentChanged.Contains(document.Id), moved, deleted);
        }
        remapBrowsePaths(moved, deleted);
        if (args.Reset)
            documentProjectionResetRequired = true;
        if (structureChanged || args.Reset)
        {
            RequestRefresh();
            return;
        }
        scheduleDocumentUpdates();
    }

    private DocumentPath captureDocumentPath(EditorDocument document) => new(document.Path, document.SavedPath,
        gameData.Documents.GetPaths(document).ToArray(), document.Section, document.Exists);

    private bool updateDocumentPath(Guid id, DocumentPath? next, bool contentChanged,
        List<(string OldPath, string NewPath)> moved, List<string> deleted)
    {
        visibleDocumentPaths.TryGetValue(id, out DocumentPath? previous);
        bool structureChanged = previous is null || next is null || previous.Exists != next.Exists
            || previous.SavedPath != next.SavedPath || !previous.Paths.SequenceEqual(next.Paths, StringComparer.Ordinal);
        bool relevant = structureChanged && (previous is not null && documentAffectsCurrentPath(previous)
            || next is not null && documentAffectsCurrentPath(next));
        if (previous is { Exists: true })
        {
            if (next is not { Exists: true })
            {
                deleted.Add(getDocumentBrowsePath(previous));
                deleted.AddRange(previous.Paths.Skip(1));
            }
            else if (previous.Path != next.Path)
                moved.Add((getDocumentBrowsePath(previous), getDocumentBrowsePath(next)));
            if (next is not null)
                deleted.AddRange(previous.Paths.Skip(1).Except(next.Paths, StringComparer.Ordinal));
        }
        if (previous is not null)
            queueDocumentUpdates(previous.Paths, contentChanged || structureChanged);
        if (next is null)
            visibleDocumentPaths.Remove(id);
        else
        {
            visibleDocumentPaths[id] = next;
            queueDocumentUpdates(next.Paths, contentChanged || structureChanged);
        }
        return relevant;
    }

    private static string getDocumentBrowsePath(DocumentPath document, bool saved = false)
    {
        string path = saved ? document.SavedPath : document.Path;
        return document.Section == "WorldMaps" ? Path.GetDirectoryName(path)! : path;
    }

    private bool documentAffectsCurrentPath(DocumentPath document) => document.Paths.Any(affectsCurrentPath)
        || affectsCurrentPath(getDocumentBrowsePath(document)) || affectsCurrentPath(getDocumentBrowsePath(document, true));

    private bool affectsCurrentPath(string path) => EditorPathSandbox.IsSameOrChildPath(CurrentPath, path)
        || EditorPathSandbox.IsSameOrChildPath(path, CurrentPath);

    private void queueDocumentUpdates(IEnumerable<string> paths, bool contentChanged)
    {
        foreach (string path in paths)
            if (entriesByPath.ContainsKey(path) || EditorPathSandbox.IsSameOrChildPath(CurrentPath, path))
                pendingDocumentUpdates[path] = contentChanged || pendingDocumentUpdates.GetValueOrDefault(path);
    }

    private void rebuildDocumentPaths(bool remap)
    {
        Dictionary<Guid, DocumentPath> next = gameData.Documents.All.ToDictionary(document => document.Id, captureDocumentPath);
        if (!remap)
        {
            visibleDocumentPaths.Clear();
            foreach (KeyValuePair<Guid, DocumentPath> document in next)
                visibleDocumentPaths.Add(document.Key, document.Value);
        }
        else
        {
            List<(string OldPath, string NewPath)> moved = [];
            List<string> deleted = [];
            foreach (Guid id in visibleDocumentPaths.Keys.Except(next.Keys).ToArray())
                updateDocumentPath(id, null, true, moved, deleted);
            foreach (KeyValuePair<Guid, DocumentPath> document in next)
                updateDocumentPath(document.Key, document.Value, true, moved, deleted);
            remapBrowsePaths(moved, deleted);
        }
        documentProjectionResetRequired = false;
    }

    private void applyPendingDocumentUpdates()
    {
        if (disposed || !IsActive || IsLoading || refreshRequired || applyingDocumentUpdates)
            return;
        applyingDocumentUpdates = true;
        try
        {
            bool refreshPreviews = previewRefreshRequired;
            previewRefreshRequired = false;
            KeyValuePair<string, bool>[] updates = pendingDocumentUpdates.ToArray();
            pendingDocumentUpdates.Clear();
            if (refreshPreviews)
            {
                foreach (FileExplorerEntryViewModel entry in Entries)
                    updateDocumentState(entry);
            }
            else
            {
                foreach (KeyValuePair<string, bool> update in updates)
                    if (entriesByPath.TryGetValue(update.Key, out FileExplorerEntryViewModel? entry))
                        updateDocumentState(entry, updateSource: update.Value);
            }
        }
        finally
        {
            applyingDocumentUpdates = false;
        }
        if (pendingDocumentUpdates.Count != 0 || previewRefreshRequired)
            scheduleDocumentUpdates();
    }

    private void scheduleDocumentUpdates()
    {
        if (disposed || !IsActive || IsLoading || refreshRequired || documentUpdatesScheduled
            || pendingDocumentUpdates.Count == 0 && !previewRefreshRequired)
            return;
        documentUpdatesScheduled = true;
        Dispatcher.UIThread.Post(() =>
        {
            documentUpdatesScheduled = false;
            applyPendingDocumentUpdates();
        }, DispatcherPriority.Background);
    }
}
