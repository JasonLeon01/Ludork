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
    private readonly List<FileExplorerBrowseState> backHistory = [];
    private readonly List<FileExplorerBrowseState> forwardHistory = [];
    private bool restoringBrowseState;
    private bool presentationRestorePending;
    private bool refreshRequired = true;
    private bool isActive;
    private long browseVersion;

    public event EventHandler? CapturePresentationRequested;
    public event EventHandler? PresentationRestoreRequested;
    public FileExplorerPresentationState PresentationState { get; private set; } = FileExplorerPresentationState.Empty;
    public long PresentationRevision { get; private set; }
    public bool CanGoBack => backHistory.Count != 0;
    public bool CanGoForward => forwardHistory.Count != 0;
    public string BackLabel => LocaleService.Get("FILE_EXPLORER_BACK");
    public string ForwardLabel => LocaleService.Get("FILE_EXPLORER_FORWARD");

    public bool IsActive
    {
        get => isActive;
        set
        {
            if (disposed || isActive == value)
                return;
            if (!value)
            {
                capturePresentation();
                browseVersion++;
                PresentationRevision++;
            }
            SetProperty(ref isActive, value);
            if (value)
                _ = EnsureLoadedAsync();
            else
            {
                navigation?.Cancel();
                navigationVersion++;
                IsLoading = false;
            }
        }
    }

    public FileExplorerTabState CaptureTabState(int number) => new(number,
        Path.GetRelativePath(projectPath, CurrentPath), IconView, Zoom, IsSourcesExpanded);

    public void UpdatePresentationState(FileExplorerPresentationState state)
    {
        if (!disposed && !presentationRestorePending)
            PresentationState = state;
    }

    public void CompletePresentationRestore(long revision)
    {
        if (revision == PresentationRevision)
            presentationRestorePending = false;
    }

    public Task GoBackAsync() => navigateHistoryAsync(backHistory, forwardHistory);
    public Task GoForwardAsync() => navigateHistoryAsync(forwardHistory, backHistory);

    private void capturePresentation()
    {
        if (IsActive && !presentationRestorePending)
            CapturePresentationRequested?.Invoke(this, EventArgs.Empty);
    }

    private FileExplorerBrowseState captureBrowseState()
    {
        capturePresentation();
        return new FileExplorerBrowseState(CurrentPath, SearchText, IconView, Zoom,
            IsSourcesExpanded, expandedDirectories.ToArray(), PresentationState);
    }

    private async Task navigateHistoryAsync(List<FileExplorerBrowseState> source, List<FileExplorerBrowseState> destination)
    {
        if (disposed)
            return;
        while (source.Count != 0)
        {
            FileExplorerBrowseState state = source[^1];
            source.RemoveAt(source.Count - 1);
            if (!isUnderRoot(state.Path) || !isVisibleDirectory(state.Path))
                continue;
            destination.Add(captureBrowseState());
            notifyHistoryChanged();
            await restoreBrowseStateAsync(state);
            return;
        }
        notifyHistoryChanged();
    }

    private async Task restoreBrowseStateAsync(FileExplorerBrowseState state, CancellationToken cancellationToken = default)
    {
        browseVersion++;
        PresentationRevision++;
        presentationRestorePending = true;
        PresentationState = state.Presentation;
        restoringBrowseState = true;
        suppressSearchRefresh = true;
        try
        {
            SelectedEntry = null;
            if (!PathComparer.Equals(CurrentPath, state.Path))
            {
                foreach (FileExplorerEntryViewModel entry in Entries)
                    entry.Dispose();
                Entries.Clear();
                entriesByPath.Clear();
            }
            CurrentPath = state.Path;
            SearchText = state.SearchText;
            expandedDirectories.Clear();
            expandedDirectories.UnionWith(state.ExpandedDirectories);
            IconView = state.IconView;
            Zoom = state.Zoom;
            IsSourcesExpanded = state.SourcesExpanded;
        }
        finally
        {
            suppressSearchRefresh = false;
            restoringBrowseState = false;
        }
        await RefreshAsync(cancellationToken);
    }

    private void notifyHistoryChanged()
    {
        OnPropertyChanged(nameof(CanGoBack));
        OnPropertyChanged(nameof(CanGoForward));
    }

    private void requestPresentationRestore()
    {
        PresentationRevision++;
        presentationRestorePending = true;
        SelectedEntry = Entries.FirstOrDefault(entry => PathComparer.Equals(entry.FullPath, PresentationState.PrimaryPath));
        if (PresentationRestoreRequested is null)
            presentationRestorePending = false;
        else
            PresentationRestoreRequested.Invoke(this, EventArgs.Empty);
    }

    public void ApplyFileChanges(FileExplorerFilesChangedEventArgs changes)
    {
        if (disposed)
            return;
        bool relevant = changes.Added.Any(affectsCurrentPath) || changes.Deleted.Any(affectsCurrentPath)
            || changes.Moved.Any(move => affectsCurrentPath(move.OldPath) || affectsCurrentPath(move.NewPath));
        remapBrowsePaths(changes.Moved, changes.Deleted);
        if (relevant)
            RequestRefresh();
    }

    private void remapBrowsePaths(IReadOnlyList<(string OldPath, string NewPath)> moved, IReadOnlyList<string> deleted)
    {
        if (moved.Count == 0 && deleted.Count == 0)
            return;
        bool currentAffected = moved.Any(move => affectsCurrentPath(move.OldPath) || affectsCurrentPath(move.NewPath))
            || deleted.Any(affectsCurrentPath);
        if (currentAffected)
            capturePresentation();
        FileExplorerBrowseState state = remapBrowseState(new FileExplorerBrowseState(CurrentPath, SearchText,
            IconView, Zoom, IsSourcesExpanded, expandedDirectories.ToArray(), PresentationState), moved, deleted);
        if (currentAffected)
        {
            browseVersion++;
            PresentationState = state.Presentation;
            PresentationRevision++;
            presentationRestorePending = true;
            CurrentPath = state.Path;
        }
        expandedDirectories.Clear();
        expandedDirectories.UnionWith(state.ExpandedDirectories);
        for (int index = 0; index < backHistory.Count; index++)
            backHistory[index] = remapBrowseState(backHistory[index], moved, deleted);
        for (int index = 0; index < forwardHistory.Count; index++)
            forwardHistory[index] = remapBrowseState(forwardHistory[index], moved, deleted);
        notifyHistoryChanged();
    }

    private static FileExplorerBrowseState remapBrowseState(FileExplorerBrowseState state,
        IReadOnlyList<(string OldPath, string NewPath)> moved, IReadOnlyList<string> deleted)
    {
        string remap(string path)
        {
            foreach ((string oldPath, string newPath) in moved.OrderByDescending(move => move.OldPath.Length))
                if (EditorPathSandbox.IsSameOrChildPath(oldPath, path))
                    return Path.GetFullPath(Path.Combine(newPath, Path.GetRelativePath(oldPath, path)));
            return path;
        }
        bool exists(string path) => !deleted.Any(removed => EditorPathSandbox.IsSameOrChildPath(removed, path));
        string? remapOptional(string? path) => path is null ? null : exists(remap(path)) ? remap(path) : null;
        FileExplorerPresentationState presentation = state.Presentation;
        return state with
        {
            Path = remap(state.Path),
            ExpandedDirectories = state.ExpandedDirectories.Select(remap).Where(exists).Distinct(PathComparer).ToArray(),
            Presentation = presentation with
            {
                SelectedPaths = presentation.SelectedPaths.Select(remap).Where(exists).Distinct(PathComparer).ToArray(),
                PrimaryPath = remapOptional(presentation.PrimaryPath),
                SelectionAnchorPath = remapOptional(presentation.SelectionAnchorPath),
                ScrollAnchorPath = remapOptional(presentation.ScrollAnchorPath),
            },
        };
    }

    private string resolveExistingDirectory(string path)
    {
        FileExplorerRootViewModel root = getContentRoot(path) ?? ContentRoots[0];
        string current = path;
        while (isUnderRoot(current))
        {
            if (isVisibleDirectory(current))
                return current;
            if (isContentRoot(current))
                break;
            current = Path.GetDirectoryName(current) ?? root.Path;
        }
        return root.Path;
    }
}
