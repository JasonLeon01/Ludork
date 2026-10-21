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
        if (!gameData.IsManagedPath(source))
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
        return Directory.Exists(path) || gameData.Documents.All.Any(document =>
            document.Exists && !pathsEqual(document.Path, path) && EditorPathSandbox.IsSameOrChildPath(path, document.Path));
    }

    private bool pathExists(string path)
    {
        return File.Exists(path) || isVisibleDirectory(path) || gameData.GetDocumentByPath(path)?.Exists == true;
    }

    private void onDocumentsChanged(object? sender, EventArgs args)
    {
        if (disposed)
            return;
        Dictionary<Guid, (string Path, bool Exists)> paths = gameData.Documents.All
            .ToDictionary(document => document.Id, document => (document.Path, document.Exists));
        bool structureChanged = paths.Count != visibleDocumentPaths.Count
            || paths.Any(pair => !visibleDocumentPaths.TryGetValue(pair.Key, out (string Path, bool Exists) previous)
                || previous != pair.Value);
        visibleDocumentPaths = paths;
        if (structureChanged)
        {
            RequestRefresh();
            return;
        }
        foreach (FileExplorerEntryViewModel entry in Entries)
            updateDocumentState(entry);
    }
}
