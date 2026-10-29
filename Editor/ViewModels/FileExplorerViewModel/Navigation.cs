using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Models;
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
    private static readonly StringComparer PathComparer = OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
    private CancellationTokenSource? navigation;
    private long navigationVersion;
    private bool refreshScheduled;
    private string? publishedPath;
    private long visualVersion;
    private long refreshVersion;

    public string LoadingLabel => LocaleService.Get("LOADING");

    public Task EnsureLoadedAsync()
    {
        if (!IsActive || IsLoading)
            return Task.CompletedTask;
        if (refreshRequired || publishedPath != CurrentPath || publishedQuery != SearchText.Trim() || publishedIconView != IconView)
            return RefreshAsync();
        applyPendingDocumentUpdates();
        return Task.CompletedTask;
    }

    public Task RefreshAsync(CancellationToken cancellationToken = default) => refreshAsync(TimeSpan.Zero, cancellationToken);

    private async Task refreshAsync(TimeSpan delay, CancellationToken cancellationToken)
    {
        if (disposed)
            return;
        refreshRequired = true;
        if (!IsActive)
            return;
        if (documentProjectionResetRequired)
            rebuildDocumentPaths(true);
        long requestedRefreshVersion = refreshVersion;
        capturePresentation();
        presentationRestorePending = true;
        PresentationRevision++;
        string existingPath = resolveExistingDirectory(CurrentPath);
        if (!PathComparer.Equals(CurrentPath, existingPath))
        {
            browseVersion++;
            CurrentPath = existingPath;
            PresentationState = FileExplorerPresentationState.Empty;
        }
        navigation?.Cancel();
        navigation?.Dispose();
        navigation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        CancellationToken token = navigation.Token;
        long version = ++navigationVersion;
        string path = CurrentPath;
        string query = SearchText.Trim();
        bool showIcons = IconView;
        HashSet<string> expanded = showIcons ? new(PathComparer) : new(expandedDirectories, PathComparer);
        IsLoading = true;
        LoadingError = string.Empty;
        try
        {
            if (delay > TimeSpan.Zero)
                await Task.Delay(delay, token);
            await EditorUiBatch.YieldAsync(token);
            DocumentPath[] documents = captureDocumentPaths();
            HashSet<string> textConfigKeys = gameData.Assets.TextConfigsData.Keys.ToHashSet(StringComparer.OrdinalIgnoreCase);
            DirectoryReadResult result = await Task.Run(() => query.Length == 0
                ? readDirectoryTree(path, documents, textConfigKeys, expanded, token)
                : readSearchResults(path, query, documents, textConfigKeys, token), token);
            token.ThrowIfCancellationRequested();
            if (version != navigationVersion || disposed || path != CurrentPath || query != SearchText.Trim() || showIcons != IconView)
                return;
            DirectoryEntry[] next = result.Entries;
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
                    entriesByPath.Remove(entry.FullPath);
                    entry.UpdatePath(matches[0].Path, path);
                    previous.Add(entry.FullPath, entry);
                    entriesByPath[entry.FullPath] = entry;
                }
            }
            EditorUiBatch batch = new();
            for (int index = Entries.Count - 1; index >= 0; index--)
            {
                token.ThrowIfCancellationRequested();
                if (!nextPaths.Contains(Entries[index].FullPath))
                {
                    FileExplorerEntryViewModel removed = Entries[index];
                    if (ReferenceEquals(SelectedEntry, removed))
                        SelectedEntry = null;
                    Entries.RemoveAt(index);
                    entriesByPath.Remove(removed.FullPath);
                    removed.Dispose();
                }
                await batch.YieldIfNeededAsync(token);
            }
            for (int index = 0; index < next.Length; index++)
            {
                token.ThrowIfCancellationRequested();
                DirectoryEntry info = next[index];
                int previousIndex = previous.TryGetValue(info.Path, out FileExplorerEntryViewModel? entry)
                    ? index < Entries.Count && ReferenceEquals(Entries[index], entry) ? index : Entries.IndexOf(entry)
                    : -1;
                if (entry is null || previousIndex < 0)
                {
                    IImage placeholder = info.IsDirectory ? EditorIconResources.GetImage("EditorImage.Folder") : MediaFileThumbnail.GetPlaceholder(info.Path);
                    entry = new FileExplorerEntryViewModel(info.Path, info.IsDirectory, placeholder, gameData.Thumbnails, previewService, gameData.AssetPaths);
                    Entries.Insert(index, entry);
                    entriesByPath[entry.FullPath] = entry;
                }
                else if (previousIndex != index)
                    Entries.Move(previousIndex, index);
                entry.UpdatePath(info.Path, path);
                entry.UpdateHierarchy(info.Depth, info.CanExpand, info.IsExpanded, Zoom);
                updateDocumentState(entry, info.Stamp);
                await batch.YieldIfNeededAsync(token);
            }
            token.ThrowIfCancellationRequested();
            if (version != navigationVersion || requestedRefreshVersion != refreshVersion)
                return;
            publishedPath = path;
            publishedQuery = query;
            publishedIconView = showIcons;
            LoadingError = result.Error;
            if (requestedRefreshVersion == refreshVersion)
                refreshRequired = false;
            requestPresentationRestore();
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested)
        {
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            if (version == navigationVersion)
            {
                LoadingError = exception.Message;
                presentationRestorePending = false;
            }
        }
        finally
        {
            if (version == navigationVersion)
            {
                IsLoading = false;
                applyPendingDocumentUpdates();
            }
        }
    }

    public async Task NavigateToAsync(string path, CancellationToken cancellationToken = default)
    {
        string fullPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
        if (disposed || !isUnderRoot(fullPath) || !isVisibleDirectory(fullPath))
            return;
        if (PathComparer.Equals(CurrentPath, fullPath) && SearchText.Length == 0)
        {
            await RefreshAsync(cancellationToken);
            return;
        }
        backHistory.Add(captureBrowseState());
        forwardHistory.Clear();
        notifyHistoryChanged();
        await restoreBrowseStateAsync(new FileExplorerBrowseState(fullPath, string.Empty, IconView, Zoom,
            IsSourcesExpanded, expandedDirectories.ToArray(), FileExplorerPresentationState.Empty), cancellationToken);
    }

    public Task GoUpAsync()
    {
        if (!CanGoUp)
            return Task.CompletedTask;
        string? parent = Directory.GetParent(CurrentPath)?.FullName;
        return parent is null ? Task.CompletedTask : NavigateToAsync(parent);
    }

    public async Task<bool> LocatePathAsync(string path)
    {
        string fullPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
        FileExplorerRootViewModel? root = getContentRoot(fullPath);
        if (root is null)
            return false;
        if (isContentRoot(fullPath))
        {
            await NavigateToAsync(root.Path);
            return PathComparer.Equals(CurrentPath, root.Path) && LoadingError.Length == 0;
        }
        string directory = Directory.GetParent(fullPath)?.FullName ?? root.Path;
        bool sameDirectory = PathComparer.Equals(CurrentPath, directory) && SearchText.Length == 0;
        FileExplorerBrowseState? previous = sameDirectory ? captureBrowseState() : null;
        if (publishedPath != directory || CurrentPath != directory || IsLoading || SearchText.Length != 0)
            await NavigateToAsync(directory);
        if (CurrentPath != directory)
            return false;
        FileExplorerEntryViewModel? located = Entries.FirstOrDefault(entry => PathComparer.Equals(entry.FullPath, fullPath));
        if (located is not null)
        {
            if (previous is not null && (previous.Presentation.SelectedPaths.Length != 1
                || !PathComparer.Equals(previous.Presentation.PrimaryPath, fullPath)))
            {
                backHistory.Add(previous);
                forwardHistory.Clear();
                notifyHistoryChanged();
            }
            browseVersion++;
            SelectedEntry = located;
            PresentationState = FileExplorerPresentationState.Empty with
            {
                SelectedPaths = [fullPath],
                PrimaryPath = fullPath,
                SelectionAnchorPath = fullPath,
                ScrollAnchorPath = fullPath,
            };
            requestPresentationRestore();
        }
        return located is not null;
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

    public async Task NotifyUserSelectionAsync(FileExplorerEntryViewModel entry)
    {
        if (disposed || !IsActive || IsReadOnly || IsLoading || entry.IsDirectory || !Entries.Contains(entry))
            return;
        long version = browseVersion;
        try
        {
            DataFileInfo? info = await entry.ReadInfoAsync(gameData);
            if (!disposed && IsActive && !IsReadOnly && !IsLoading && version == browseVersion && ReferenceEquals(SelectedEntry, entry))
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
        if (disposed)
            return;
        refreshRequired = true;
        refreshVersion++;
        navigation?.Cancel();
        if (!IsActive || refreshScheduled)
            return;
        refreshScheduled = true;
        Dispatcher.UIThread.Post(async () =>
        {
            refreshScheduled = false;
            await RefreshAsync();
        }, DispatcherPriority.Background);
    }

    private void updateDocumentState(FileExplorerEntryViewModel entry, string? stamp = null, bool updateSource = true)
    {
        EditorDocument? document = gameData.Documents.FindByPath(entry.FullPath);
        entry.IsModified = document?.IsModified == true;
        if (!updateSource)
            return;
        string? blueprint = previewService.GetBlueprintReferenceForPath(entry.FullPath);
        if (document is not null)
            entry.UpdateSource($"document:{document.Id}:{document.LastContentChangeRevision}:{previewService.ResolutionRevision}:{visualVersion}", blueprint);
        else if (stamp is not null)
            entry.UpdateSource($"{stamp}:{gameData.Thumbnails.GetSourceRevision(entry.FullPath)}", null);
    }

    private DocumentPath[] captureDocumentPaths()
    {
        string assetsRoot = Path.Combine(projectPath, "Assets");
        return visibleDocumentPaths.Values.Where(document => !EditorPathSandbox.IsSameOrChildPath(assetsRoot, document.Path)
            && documentAffectsCurrentPath(document)).ToArray();
    }

    private DirectoryEntry[] readDirectory(string directory, DirectorySnapshot snapshot, CancellationToken token)
    {
        Dictionary<string, DirectoryEntry> paths = new(PathComparer);
        if (gameData.AssetPaths.DirectoryExists(directory))
        {
            foreach (ProjectAssetPathEntry file in gameData.AssetPaths.EnumerateEntries(directory))
            {
                token.ThrowIfCancellationRequested();
                if (snapshot.Hidden.Contains(file.Path) || !DataConfig.shouldDisplay(file.Path))
                    continue;
                bool isDirectory = file.IsDirectory;
                bool canExpand = isDirectory && !EditorPathSandbox.IsLink(new DirectoryInfo(file.ReadPath));
                long length = file.Length;
                paths[file.Path] = new DirectoryEntry(file.Path, isDirectory,
                    $"{file.LastWriteTimeUtc.Ticks}:{length}", CanExpand: canExpand);
            }
        }
        else if (!snapshot.Children.ContainsKey(directory))
            throw new DirectoryNotFoundException(directory);
        if (snapshot.Children.TryGetValue(directory, out Dictionary<string, DirectoryEntry>? children))
        {
            foreach (DirectoryEntry child in children.Values)
            {
                token.ThrowIfCancellationRequested();
                paths.TryAdd(child.Path, child);
            }
        }
        string textRoot = Path.Combine(projectPath, "Data", "TextConfigs");
        return paths.Values.Where(entry => !EditorPathSandbox.IsSameOrChildPath(textRoot, entry.Path)
                || snapshot.TextVisibility.IsVisible(entry.Path, entry.IsDirectory, token))
            .OrderBy(entry => !entry.IsDirectory)
            .ThenBy(entry => Path.GetFileName(entry.Path), StringComparer.OrdinalIgnoreCase).ToArray();
    }

    private sealed record DocumentPath(string Path, string SavedPath, string[] Paths, string Section, bool Exists);
    private sealed record DirectoryEntry(string Path, bool IsDirectory, string Stamp,
        int Depth = 0, bool CanExpand = false, bool IsExpanded = false);
}
