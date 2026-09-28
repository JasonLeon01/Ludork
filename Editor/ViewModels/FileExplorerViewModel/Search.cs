using CommunityToolkit.Mvvm.ComponentModel;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;

namespace Ludork.ViewModels;

public sealed partial class FileExplorerViewModel
{
    private bool suppressSearchRefresh;
    private string? publishedQuery;
    [ObservableProperty] private string searchText = string.Empty;

    public bool IsSearching => SearchText.Trim().Length != 0;

    partial void OnSearchTextChanged(string value)
    {
        OnPropertyChanged(nameof(IsSearching));
        OnPropertyChanged(nameof(IconTileHeight));
        if (!suppressSearchRefresh)
            _ = refreshAsync(TimeSpan.FromMilliseconds(150), CancellationToken.None);
    }

    private void clearSearchForNavigation()
    {
        suppressSearchRefresh = true;
        SearchText = string.Empty;
        suppressSearchRefresh = false;
    }

    private DirectoryReadResult readSearchResults(string directory, string query, DocumentPath[] documents,
        HashSet<string> textConfigKeys, CancellationToken token)
    {
        TextConfigVisibility textVisibility = new(projectPath, textConfigKeys);
        HashSet<string> hidden = getHiddenPaths(documents);
        Dictionary<string, DirectoryEntry> paths = new(PathComparer);
        Dictionary<string, bool> directoryVisibility = new(PathComparer);
        List<string> errors = [];
        Stack<DirectoryInfo> pending = new();
        if (Directory.Exists(directory))
            pending.Push(new DirectoryInfo(directory));
        else if (!documents.Any(document => document.Exists && document.Paths.Any(path => EditorPathSandbox.IsSameOrChildPath(directory, path))))
            throw new DirectoryNotFoundException(directory);
        string textRoot = Path.Combine(projectPath, "Data", "TextConfigs");
        while (pending.TryPop(out DirectoryInfo? current))
        {
            token.ThrowIfCancellationRequested();
            try
            {
                bool visible = !EditorPathSandbox.IsLink(current);
                directoryVisibility[current.FullName] = visible;
                if (!visible || hidden.Contains(current.FullName))
                    continue;
                foreach (FileSystemInfo file in current.EnumerateFileSystemInfos())
                {
                    token.ThrowIfCancellationRequested();
                    try
                    {
                        if (hidden.Contains(file.FullName) || !DataConfig.shouldDisplay(file.FullName))
                            continue;
                        if (file is DirectoryInfo child)
                        {
                            pending.Push(child);
                            continue;
                        }
                        if (!file.Name.Contains(query, StringComparison.OrdinalIgnoreCase)
                            || EditorPathSandbox.IsSameOrChildPath(textRoot, file.FullName)
                            && !textVisibility.IsVisible(file.FullName, false, token))
                            continue;
                        long length = file is FileInfo info ? info.Length : 0;
                        paths[file.FullName] = new DirectoryEntry(file.FullName, false, $"{file.LastWriteTimeUtc.Ticks}:{length}");
                    }
                    catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
                    {
                        errors.Add($"{Path.GetRelativePath(directory, file.FullName)}: {exception.Message}");
                    }
                }
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                errors.Add($"{Path.GetRelativePath(directory, current.FullName)}: {exception.Message}");
            }
        }
        foreach (DocumentPath document in documents)
        {
            token.ThrowIfCancellationRequested();
            if (!document.Exists)
                continue;
            foreach (string path in document.Paths)
            {
                token.ThrowIfCancellationRequested();
                if (!Path.GetFileName(path).Contains(query, StringComparison.OrdinalIgnoreCase)
                    || !EditorPathSandbox.IsSameOrChildPath(directory, path) || PathComparer.Equals(directory, path))
                    continue;
                try
                {
                    if (!isVisibleSearchDocumentPath(directory, path, directoryVisibility, token)
                        || EditorPathSandbox.IsSameOrChildPath(textRoot, path) && !textVisibility.IsVisible(path, false, token))
                        continue;
                    paths.TryAdd(path, new DirectoryEntry(path, false, string.Empty));
                }
                catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
                {
                    errors.Add($"{Path.GetRelativePath(directory, path)}: {exception.Message}");
                }
            }
        }
        DirectoryEntry[] entries = paths.Values
            .OrderBy(entry => Path.GetFileName(entry.Path), StringComparer.OrdinalIgnoreCase)
            .ThenBy(entry => Path.GetRelativePath(directory, entry.Path), StringComparer.OrdinalIgnoreCase)
            .ThenBy(entry => entry.Path, PathComparer)
            .ToArray();
        return new DirectoryReadResult(entries, string.Join(Environment.NewLine, errors.Distinct(StringComparer.Ordinal)));
    }

    private static bool isVisibleSearchDocumentPath(string directory, string path,
        Dictionary<string, bool> directoryVisibility, CancellationToken token)
    {
        if (!DataConfig.shouldDisplay(path) || directoryVisibility.GetValueOrDefault(directory, true) == false)
            return false;
        string relative = Path.GetRelativePath(directory, path);
        string[] parts = relative.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        string current = directory;
        for (int index = 0; index < parts.Length - 1; index++)
        {
            token.ThrowIfCancellationRequested();
            current = Path.Combine(current, parts[index]);
            if (!directoryVisibility.TryGetValue(current, out bool visible))
            {
                visible = DataConfig.shouldDisplay(current)
                    && (!Directory.Exists(current) || !EditorPathSandbox.IsLink(current));
                directoryVisibility[current] = visible;
            }
            if (!visible)
                return false;
        }
        return true;
    }

    private static HashSet<string> getHiddenPaths(DocumentPath[] documents)
    {
        HashSet<string> hidden = documents.Where(document => document.Path != document.SavedPath || !document.Exists)
            .Select(document => document.SavedPath).ToHashSet(PathComparer);
        foreach (DocumentPath document in documents.Where(document => document.Section == "WorldMaps"
            && (!document.Exists || Path.GetDirectoryName(document.Path) != Path.GetDirectoryName(document.SavedPath))))
            hidden.Add(Path.GetDirectoryName(document.SavedPath)!);
        return hidden;
    }

    private sealed record DirectoryReadResult(DirectoryEntry[] Entries, string Error);
}
