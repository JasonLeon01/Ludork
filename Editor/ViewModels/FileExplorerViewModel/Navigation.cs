using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.ViewModels;

public sealed partial class FileExplorerViewModel
{
    private static readonly StringComparer PathComparer = OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
    private CancellationTokenSource? navigation;
    private long navigationVersion;
    private bool refreshScheduled;
    private string? publishedPath;
    private long visualVersion;

    public string LoadingLabel => LocaleService.Get("LOADING");

    public Task EnsureLoadedAsync() => !IsLoading && publishedPath != CurrentPath ? RefreshAsync() : Task.CompletedTask;

    public async Task RefreshAsync(CancellationToken cancellationToken = default)
    {
        if (disposed)
            return;
        navigation?.Cancel();
        navigation?.Dispose();
        navigation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        CancellationToken token = navigation.Token;
        long version = ++navigationVersion;
        visualVersion++;
        string path = CurrentPath;
        IsLoading = true;
        LoadingError = string.Empty;
        try
        {
            await EditorUiBatch.YieldAsync(token);
            DocumentPath[] documents = captureDocumentPaths();
            HashSet<string> textConfigKeys = gameData.Assets.TextConfigsData.Keys.ToHashSet(StringComparer.OrdinalIgnoreCase);
            DirectoryEntry[] next = await Task.Run(() => readDirectory(path, documents, textConfigKeys, token), token);
            token.ThrowIfCancellationRequested();
            if (version != navigationVersion || disposed)
                return;
            Dictionary<string, FileExplorerEntryViewModel> previous = Entries.ToDictionary(entry => entry.FullPath, PathComparer);
            HashSet<string> nextPaths = next.Select(entry => entry.Path).ToHashSet(PathComparer);
            Dictionary<string, DirectoryEntry[]> spellingMatches = next.GroupBy(entry => entry.Path, StringComparer.OrdinalIgnoreCase)
                .ToDictionary(group => group.Key, group => group.ToArray(), StringComparer.OrdinalIgnoreCase);
            foreach (FileExplorerEntryViewModel entry in Entries)
            {
                if (!nextPaths.Contains(entry.FullPath) && spellingMatches.TryGetValue(entry.FullPath, out DirectoryEntry[]? matches)
                    && matches.Length == 1 && !previous.ContainsKey(matches[0].Path))
                {
                    previous.Remove(entry.FullPath);
                    entry.UpdatePath(matches[0].Path);
                    previous.Add(entry.FullPath, entry);
                }
            }
            EditorUiBatch batch = new();
            for (int index = Entries.Count - 1; index >= 0; index--)
            {
                if (!nextPaths.Contains(Entries[index].FullPath))
                {
                    FileExplorerEntryViewModel removed = Entries[index];
                    Entries.RemoveAt(index);
                    removed.Dispose();
                }
                await batch.YieldIfNeededAsync(token);
            }
            for (int index = 0; index < next.Length; index++)
            {
                DirectoryEntry info = next[index];
                if (!previous.TryGetValue(info.Path, out FileExplorerEntryViewModel? entry))
                {
                    IImage placeholder = EditorIconResources.GetImage(info.IsDirectory ? "EditorImage.Folder" : "EditorImage.File");
                    entry = new FileExplorerEntryViewModel(info.Path, info.IsDirectory, placeholder, gameData.Thumbnails, previewService);
                    Entries.Insert(index, entry);
                }
                else if (index >= Entries.Count || !ReferenceEquals(Entries[index], entry))
                    Entries.Move(Entries.IndexOf(entry), index);
                entry.UpdatePath(info.Path);
                updateDocumentState(entry, info.Stamp);
                await batch.YieldIfNeededAsync(token);
            }
            publishedPath = path;
            visibleDocumentPaths = gameData.Documents.All.ToDictionary(document => document.Id, document => (document.Path, document.Exists));
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested)
        {
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            if (version == navigationVersion)
                LoadingError = exception.Message;
        }
        finally
        {
            if (version == navigationVersion)
                IsLoading = false;
        }
    }

    public async Task NavigateToAsync(string path, CancellationToken cancellationToken = default)
    {
        string fullPath = Path.GetFullPath(path);
        if (disposed || !isUnderRoot(fullPath))
            return;
        if (!PathComparer.Equals(CurrentPath, fullPath))
        {
            SelectedEntry = null;
            foreach (FileExplorerEntryViewModel entry in Entries)
                entry.Dispose();
            Entries.Clear();
        }
        CurrentPath = fullPath;
        await RefreshAsync(cancellationToken);
        if (!disposed && CurrentPath == fullPath && LoadingError.Length == 0 && !IsReadOnly)
            projectConfig.LastFileExplorerPath = Path.GetRelativePath(projectPath, fullPath);
    }

    public Task GoUpAsync()
    {
        string? parent = Directory.GetParent(CurrentPath)?.FullName;
        return parent is null ? Task.CompletedTask : NavigateToAsync(parent);
    }

    public async Task<bool> LocatePathAsync(string path)
    {
        string fullPath = Path.GetFullPath(path);
        if (!isUnderRoot(fullPath))
            return false;
        string directory = Directory.GetParent(fullPath)?.FullName ?? projectPath;
        if (publishedPath != directory || CurrentPath != directory || IsLoading)
            await NavigateToAsync(directory);
        if (CurrentPath != directory)
            return false;
        SelectedEntry = Entries.FirstOrDefault(entry => PathComparer.Equals(entry.FullPath, fullPath));
        return SelectedEntry is not null;
    }

    public async Task OpenSelectedAsync()
    {
        if (SelectedEntry is { IsDirectory: true } directory)
        {
            await NavigateToAsync(directory.FullPath);
            return;
        }
        if (SelectedEntry is not { } file || IsReadOnly)
            return;
        try
        {
            DataFileInfo? info = await file.ReadInfoAsync(gameData);
            if (!disposed && ReferenceEquals(SelectedEntry, file))
                FileOpened?.Invoke(this, new FileExplorerFileEventArgs(file.FullPath, info));
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            LoadingError = exception.Message;
        }
    }

    private async Task notifyFileClickedAsync(FileExplorerEntryViewModel entry)
    {
        try
        {
            DataFileInfo? info = await entry.ReadInfoAsync(gameData);
            if (!disposed && ReferenceEquals(SelectedEntry, entry))
                FileClicked?.Invoke(this, new FileExplorerFileEventArgs(entry.FullPath, info));
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            if (ReferenceEquals(SelectedEntry, entry))
                LoadingError = exception.Message;
        }
    }

    internal void RequestRefresh()
    {
        if (disposed || refreshScheduled)
            return;
        refreshScheduled = true;
        Dispatcher.UIThread.Post(async () =>
        {
            refreshScheduled = false;
            await RefreshAsync();
        }, DispatcherPriority.Background);
    }

    private void updateDocumentState(FileExplorerEntryViewModel entry, string? stamp = null)
    {
        EditorDocument? document = gameData.Documents.FindByPath(entry.FullPath);
        entry.IsModified = document?.IsModified == true;
        string? blueprint = document is { Section: "Blueprints", Exists: true }
            ? "Data.Blueprints." + document.Key.Replace('/', '.') : null;
        if (document is not null)
            entry.UpdateSource($"document:{document.Id}:{document.Revision}:{previewService.ResolutionRevision}:{visualVersion}", blueprint);
        else if (stamp is not null)
            entry.UpdateSource(stamp, null);
    }

    private DocumentPath[] captureDocumentPaths()
    {
        return gameData.Documents.All.Select(document => new DocumentPath(document.Path, document.SavedPath,
            gameData.Documents.GetPaths(document).ToArray(), document.Section, document.Exists)).ToArray();
    }

    private DirectoryEntry[] readDirectory(string directory, DocumentPath[] documents,
        HashSet<string> textConfigKeys, CancellationToken token)
    {
        HashSet<string> hidden = documents.Where(document => document.Path != document.SavedPath || !document.Exists)
            .Select(document => document.SavedPath).ToHashSet(PathComparer);
        foreach (DocumentPath document in documents.Where(document => document.Section == "WorldMaps"
            && Path.GetDirectoryName(document.Path) != Path.GetDirectoryName(document.SavedPath)))
            hidden.Add(Path.GetDirectoryName(document.SavedPath)!);
        Dictionary<string, DirectoryEntry> paths = new(PathComparer);
        if (Directory.Exists(directory))
        {
            foreach (FileSystemInfo file in new DirectoryInfo(directory).EnumerateFileSystemInfos())
            {
                token.ThrowIfCancellationRequested();
                if (hidden.Contains(file.FullName) || !DataConfig.shouldDisplay(file.FullName))
                    continue;
                bool isDirectory = file is DirectoryInfo;
                long length = file is FileInfo info ? info.Length : 0;
                paths[file.FullName] = new DirectoryEntry(file.FullName, isDirectory, $"{file.LastWriteTimeUtc.Ticks}:{length}");
            }
        }
        else if (!documents.Any(document => document.Exists && isSameOrChildPath(directory, document.Path)))
            throw new DirectoryNotFoundException(directory);
        foreach (DocumentPath document in documents)
        {
            token.ThrowIfCancellationRequested();
            if (!document.Exists)
                continue;
            foreach (string path in document.Paths)
            {
                string relative = Path.GetRelativePath(directory, path);
                if (Path.IsPathRooted(relative) || relative == ".." || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal))
                    continue;
                string first = relative.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)[0];
                if (first.Length == 0 || first == ".")
                    continue;
                string visiblePath = Path.Combine(directory, first);
                if (DataConfig.shouldDisplay(visiblePath))
                    paths.TryAdd(visiblePath, new DirectoryEntry(visiblePath, relative != first, string.Empty));
            }
        }
        string textRoot = Path.Combine(projectPath, "Data", "TextConfigs");
        return paths.Values.Where(entry => !isSameOrChildPath(textRoot, entry.Path)
                || hasVisibleTextContent(entry.Path, entry.IsDirectory, textConfigKeys, token))
            .OrderBy(entry => !entry.IsDirectory)
            .ThenBy(entry => Path.GetFileName(entry.Path), StringComparer.OrdinalIgnoreCase).ToArray();
    }

    private bool hasVisibleTextContent(string path, bool directory, HashSet<string> textKeys, CancellationToken token)
    {
        token.ThrowIfCancellationRequested();
        if (directory)
        {
            string relative = Path.GetRelativePath(Path.Combine(projectPath, "Data", "TextConfigs"), path).Replace('\\', '/').Trim('/');
            if (textKeys.Any(key => key.StartsWith(relative + "/", StringComparison.OrdinalIgnoreCase)))
                return true;
            return Directory.Exists(path) && new DirectoryInfo(path).EnumerateFileSystemInfos()
                .Any(file => hasVisibleTextContent(file.FullName, file is DirectoryInfo, textKeys, token));
        }
        if (!Path.GetExtension(path).Equals(DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase))
            return true;
        string key = Path.ChangeExtension(Path.GetRelativePath(Path.Combine(projectPath, "Data", "TextConfigs"), path), null)!.Replace('\\', '/');
        if (textKeys.Contains(key) || !File.Exists(path))
            return true;
        try
        {
            JsonObject? data = JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
            return data?["type"]?.ToString() is not ("plainTextConfig" or "richTextConfig");
        }
        catch (System.Text.Json.JsonException)
        {
            return true;
        }
    }

    private sealed record DocumentPath(string Path, string SavedPath, string[] Paths, string Section, bool Exists);
    private sealed record DirectoryEntry(string Path, bool IsDirectory, string Stamp);
}
