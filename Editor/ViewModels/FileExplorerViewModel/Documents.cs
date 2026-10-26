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
    private Dictionary<Guid, (string Path, bool Exists)> visibleDocumentPaths = [];
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
        return gameData.AssetPaths.DirectoryExists(path) || gameData.Documents.All.Any(document =>
            document.Exists && !pathsEqual(document.Path, path) && EditorPathSandbox.IsSameOrChildPath(path, document.Path));
    }

    private bool pathExists(string path)
    {
        return gameData.AssetPaths.FileExists(path) || isVisibleDirectory(path) || gameData.GetDocumentByPath(path)?.Exists == true;
    }

    private void onDocumentsChanged(object? sender, EventArgs args)
    {
        if (disposed)
            return;
        EditorDocument[] documents = gameData.Documents.All.ToArray();
        Dictionary<Guid, (string Path, bool Exists)> paths = documents
            .ToDictionary(document => document.Id, document => (document.Path, document.Exists));
        bool structureChanged = paths.Count != visibleDocumentPaths.Count
            || paths.Any(pair => !visibleDocumentPaths.TryGetValue(pair.Key, out (string Path, bool Exists) previous)
                || previous != pair.Value);
        (string OldPath, string NewPath)[] moved = documents
            .Where(document => document.Exists && visibleDocumentPaths.TryGetValue(document.Id, out (string Path, bool Exists) previous)
                && previous.Path != document.Path)
            .Select(document => document.Section == "WorldMaps"
                ? (Path.GetDirectoryName(visibleDocumentPaths[document.Id].Path)!, Path.GetDirectoryName(document.Path)!)
                : (visibleDocumentPaths[document.Id].Path, document.Path))
            .Distinct().ToArray();
        string[] deleted = visibleDocumentPaths.Where(item => item.Value.Exists
                && (!paths.TryGetValue(item.Key, out (string Path, bool Exists) next) || !next.Exists))
            .Select(item => item.Value.Path).ToArray();
        remapBrowsePaths(moved, deleted);
        visibleDocumentPaths = paths;
        if (structureChanged)
        {
            RequestRefresh();
            return;
        }
        if (!IsActive)
        {
            refreshRequired = true;
            return;
        }
        foreach (FileExplorerEntryViewModel entry in Entries)
            updateDocumentState(entry);
    }
}
