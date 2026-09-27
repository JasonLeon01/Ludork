using Ludork.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.ViewModels;

public sealed partial class FileExplorerViewModel
{
    private readonly HashSet<string> expandedDirectories = new(PathComparer);
    private bool? publishedIconView;

    public string ExpandFolderLabel => LocaleService.Get("FILE_EXPLORER_EXPAND_FOLDER");
    public string CollapseFolderLabel => LocaleService.Get("FILE_EXPLORER_COLLAPSE_FOLDER");

    public async Task ToggleDirectoryAsync(FileExplorerEntryViewModel entry)
    {
        if (disposed || IconView || IsSearching || !entry.CanExpand || !Entries.Contains(entry))
            return;
        bool expanded = expandedDirectories.Add(entry.FullPath);
        if (!expanded)
        {
            expandedDirectories.Remove(entry.FullPath);
            if (SelectedEntry is { } selected && !ReferenceEquals(selected, entry)
                && isSameOrChildPath(entry.FullPath, selected.FullPath))
                SelectedEntry = entry;
        }
        entry.UpdateHierarchy(entry.Depth, entry.CanExpand, expanded, Zoom);
        await RefreshAsync();
    }

    public FileExplorerEntryViewModel? GetParentEntry(FileExplorerEntryViewModel entry)
    {
        if (IsSearching || entry.Depth == 0)
            return null;
        string? parent = Path.GetDirectoryName(entry.FullPath);
        return Entries.FirstOrDefault(candidate => candidate.IsDirectory && PathComparer.Equals(candidate.FullPath, parent));
    }

    private void updateExpandedDirectories(FileExplorerFilesChangedEventArgs changes)
    {
        foreach ((string oldPath, string newPath) in changes.Moved)
        {
            string[] moved = expandedDirectories.Where(path => isSameOrChildPath(oldPath, path)).ToArray();
            foreach (string path in moved)
            {
                expandedDirectories.Remove(path);
                expandedDirectories.Add(Path.GetFullPath(Path.Combine(newPath, Path.GetRelativePath(oldPath, path))));
            }
        }
        expandedDirectories.RemoveWhere(path => changes.Deleted.Any(deleted => isSameOrChildPath(deleted, path)));
    }

    private DirectoryReadResult readDirectoryTree(string directory, DocumentPath[] documents,
        HashSet<string> textConfigKeys, HashSet<string> expanded, CancellationToken token)
    {
        DirectorySnapshot snapshot = createDirectorySnapshot(directory, documents, textConfigKeys, token);
        Stack<DirectoryEntry> pending = new(readDirectory(directory, snapshot, token).Reverse());
        List<DirectoryEntry> entries = [];
        List<string> errors = [];
        while (pending.TryPop(out DirectoryEntry? entry))
        {
            token.ThrowIfCancellationRequested();
            bool isExpanded = entry.CanExpand && expanded.Contains(entry.Path);
            entries.Add(entry with { IsExpanded = isExpanded });
            if (!isExpanded)
                continue;
            try
            {
                DirectoryEntry[] children = readDirectory(entry.Path, snapshot, token);
                for (int index = children.Length - 1; index >= 0; index--)
                    pending.Push(children[index] with { Depth = entry.Depth + 1 });
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                errors.Add($"{Path.GetRelativePath(directory, entry.Path)}: {exception.Message}");
            }
        }
        return new DirectoryReadResult(entries.ToArray(), string.Join(Environment.NewLine, errors));
    }

    private static DirectorySnapshot createDirectorySnapshot(string directory, DocumentPath[] documents,
        HashSet<string> textConfigKeys, CancellationToken token)
    {
        Dictionary<string, Dictionary<string, DirectoryEntry>> children = new(PathComparer);
        foreach (DocumentPath document in documents)
        {
            token.ThrowIfCancellationRequested();
            if (!document.Exists)
                continue;
            foreach (string path in document.Paths)
            {
                if (!isSameOrChildPath(directory, path))
                    continue;
                string current = path;
                bool isDirectory = false;
                while (!PathComparer.Equals(current, directory))
                {
                    token.ThrowIfCancellationRequested();
                    string parent = Path.GetDirectoryName(current)!;
                    if (!DataConfig.shouldDisplay(current))
                        break;
                    if (!children.TryGetValue(parent, out Dictionary<string, DirectoryEntry>? siblings))
                    {
                        siblings = new Dictionary<string, DirectoryEntry>(PathComparer);
                        children.Add(parent, siblings);
                    }
                    siblings.TryAdd(current, new DirectoryEntry(current, isDirectory, string.Empty, CanExpand: isDirectory));
                    current = parent;
                    isDirectory = true;
                }
            }
        }
        return new DirectorySnapshot(children, getHiddenPaths(documents), textConfigKeys);
    }

    private sealed record DirectorySnapshot(Dictionary<string, Dictionary<string, DirectoryEntry>> Children,
        HashSet<string> Hidden, HashSet<string> TextConfigKeys);
}
