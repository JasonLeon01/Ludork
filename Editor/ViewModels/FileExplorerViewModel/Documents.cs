using Ludork.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Ludork.ViewModels;

public sealed partial class FileExplorerViewModel
{
    private Dictionary<Guid, (string Path, bool Exists)> visibleDocumentPaths = [];
    private bool tryMoveManagedPath(string source, string destination, out string? error)
    {
        error = null;
        if (!gameData.IsManagedPath(source))
            return false;
        try
        {
            changing(new FileExplorerFilesChangedEventArgs([], [(source, destination)], []));
            return gameData.TryRenameManagedPath(source, destination, out error);
        }
        catch (Exception exception) when (exception is InvalidOperationException or IOException or UnauthorizedAccessException)
        {
            error = exception.Message;
            return true;
        }
    }

    private bool tryCopyManagedPath(string source, string destination, out string? error)
    {
        error = null;
        if (!gameData.IsManagedPath(source))
            return false;
        try
        {
            changing(new FileExplorerFilesChangedEventArgs([destination], [], []));
            return gameData.TryCopyManagedPath(source, destination, out error);
        }
        catch (Exception exception) when (exception is InvalidOperationException or IOException or UnauthorizedAccessException)
        {
            error = exception.Message;
            return true;
        }
    }

    private bool isVisibleDirectory(string path)
    {
        return Directory.Exists(path) || gameData.Documents.All.Any(document =>
            document.Exists && !pathsEqual(document.Path, path) && isPathInside(document.Path, path));
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
