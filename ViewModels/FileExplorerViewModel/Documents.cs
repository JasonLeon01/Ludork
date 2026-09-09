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

    private IEnumerable<string> enumerateVisiblePaths()
    {
        HashSet<string> paths = new(StringComparer.OrdinalIgnoreCase);
        HashSet<string> movedFiles = gameData.Documents.All
            .Where(document => !string.Equals(document.SavedPath, document.Path, StringComparison.Ordinal))
            .Select(document => document.SavedPath).ToHashSet(StringComparer.OrdinalIgnoreCase);
        HashSet<string> movedWorldDirectories = gameData.Documents.All
            .Where(document => document.Section == "WorldMaps"
                && !pathsEqual(Path.GetDirectoryName(document.SavedPath)!, Path.GetDirectoryName(document.Path)!))
            .Select(document => Path.GetDirectoryName(document.SavedPath)!)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        if (Directory.Exists(CurrentPath))
        {
            foreach (string path in Directory.EnumerateFileSystemEntries(CurrentPath))
            {
                if (movedWorldDirectories.Contains(path))
                    continue;
                if (movedFiles.Contains(path) || gameData.Documents.FindByPath(path) is { Exists: false })
                {
                    continue;
                }
                paths.Add(path);
            }
        }
        foreach (EditorDocument document in gameData.Documents.All)
        {
            if (!document.Exists)
                continue;
            foreach (string documentPath in gameData.Documents.GetPaths(document))
            {
                string relative = Path.GetRelativePath(CurrentPath, documentPath);
                if (Path.IsPathRooted(relative) || relative == ".."
                    || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal))
                {
                    continue;
                }
                string first = relative.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)[0];
                if (first.Length != 0 && first != ".")
                    paths.Add(Path.Combine(CurrentPath, first));
            }
        }
        return paths.Where(shouldDisplay)
            .OrderBy(path => !isVisibleDirectory(path))
            .ThenBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase);
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
        if (disposed || refreshingEntries)
            return;
        Dictionary<Guid, (string Path, bool Exists)> paths = gameData.Documents.All
            .ToDictionary(document => document.Id, document => (document.Path, document.Exists));
        bool structureChanged = paths.Count != visibleDocumentPaths.Count
            || paths.Any(pair => !visibleDocumentPaths.TryGetValue(pair.Key, out (string Path, bool Exists) previous)
                || previous != pair.Value);
        visibleDocumentPaths = paths;
        if (structureChanged)
        {
            Refresh();
            return;
        }
        foreach (FileExplorerEntryViewModel entry in Entries)
            entry.IsModified = !entry.IsDirectory && gameData.Documents.FindByPath(entry.FullPath)?.IsModified == true;
    }
}
