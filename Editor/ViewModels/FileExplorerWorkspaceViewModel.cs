using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;

namespace Ludork.ViewModels;

public sealed class FileExplorerWorkspaceViewModel : ViewModelBase, IDisposable
{
    private readonly string projectPath;
    private readonly ProjectConfigService projectConfig;
    private readonly ProjectDataStore gameData;
    private readonly BlueprintPreviewService previewService;
    private readonly ReferenceIndexService referenceIndex;
    private FileExplorerTabViewModel activeTab;
    private bool visible;
    private bool isReadOnly;
    private bool suppressPersistence;
    private bool disposed;

    public FileExplorerWorkspaceViewModel(
        string projectPath,
        ProjectConfigService projectConfig,
        ProjectDataStore gameData,
        BlueprintPreviewService previewService,
        ReferenceIndexService referenceIndex)
    {
        this.projectPath = Path.GetFullPath(projectPath);
        this.projectConfig = projectConfig;
        this.gameData = gameData;
        this.previewService = previewService;
        this.referenceIndex = referenceIndex;
        FileExplorerWorkspaceState? saved = projectConfig.FileExplorerWorkspace;
        HashSet<int> numbers = [];
        if (saved is not null)
        {
            foreach (FileExplorerTabState state in saved.Tabs)
            {
                if (state.Number > 0 && numbers.Add(state.Number))
                    Tabs.Add(createTab(state.Number, normalizeState(state)));
            }
        }
        FileExplorerTabViewModel? primary = Tabs.FirstOrDefault(tab => tab.Number == 1);
        if (primary is null)
        {
            primary = createTab(1, null);
            Tabs.Insert(0, primary);
        }
        else if (Tabs.IndexOf(primary) != 0)
        {
            Tabs.Move(Tabs.IndexOf(primary), 0);
        }
        activeTab = Tabs.FirstOrDefault(tab => tab.Number == saved?.ActiveTabNumber) ?? primary;
    }

    public ObservableCollection<FileExplorerTabViewModel> Tabs { get; } = [];
    public FileExplorerTabViewModel ActiveTab => activeTab;
    public event EventHandler<FileExplorerFileEventArgs>? FileClicked;
    public event EventHandler<FileExplorerFileEventArgs>? FileOpened;
    public event EventHandler<EditorDataCreationRequest>? DataCreationRequested;
    public event EventHandler<string>? ReferenceTreeRequested;
    public event EventHandler<FileExplorerFilesChangedEventArgs>? FilesChanging;
    public event EventHandler<FileExplorerFilesChangedEventArgs>? FilesChanged;

    public bool IsReadOnly
    {
        get => isReadOnly;
        set
        {
            if (!SetProperty(ref isReadOnly, value))
                return;
            foreach (FileExplorerTabViewModel tab in Tabs)
                tab.Explorer.IsReadOnly = value;
            if (!value)
                saveState();
        }
    }

    public void ActivateTab(FileExplorerTabViewModel tab)
    {
        if (disposed || !Tabs.Contains(tab) || ReferenceEquals(activeTab, tab))
            return;
        activeTab = tab;
        updateActivity();
        OnPropertyChanged(nameof(ActiveTab));
        saveState();
    }

    public FileExplorerTabViewModel AddTab(FileExplorerTabViewModel? source = null)
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        source = source is not null && Tabs.Contains(source) ? source : activeTab;
        int number = 2;
        while (Tabs.Any(tab => tab.Number == number))
            number++;
        FileExplorerTabViewModel added = createTab(number, source.Explorer.CaptureTabState(number));
        Tabs.Add(added);
        ActivateTab(added);
        return added;
    }

    public bool CloseTab(FileExplorerTabViewModel tab)
    {
        int index = Tabs.IndexOf(tab);
        if (disposed || !tab.CanClose || index < 0)
            return false;
        bool wasActive = ReferenceEquals(activeTab, tab);
        if (wasActive)
            activeTab = Tabs[index - 1];
        detachTab(tab);
        Tabs.RemoveAt(index);
        tab.Dispose();
        if (wasActive)
        {
            updateActivity();
            OnPropertyChanged(nameof(ActiveTab));
        }
        saveState();
        return true;
    }

    public void SetVisible(bool value)
    {
        if (disposed || visible == value)
            return;
        visible = value;
        updateActivity();
    }

    public void RequestRefresh()
    {
        if (disposed)
            return;
        foreach (FileExplorerTabViewModel tab in Tabs)
            tab.Explorer.RequestRefresh();
    }

    public void ApplyFileChanges(FileExplorerFilesChangedEventArgs args)
    {
        if (disposed)
            return;
        suppressPersistence = true;
        try
        {
            foreach (FileExplorerTabViewModel tab in Tabs)
                tab.Explorer.ApplyFileChanges(args);
        }
        finally
        {
            suppressPersistence = false;
        }
        saveState();
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        foreach (FileExplorerTabViewModel tab in Tabs)
        {
            detachTab(tab);
            tab.Dispose();
        }
        Tabs.Clear();
    }

    private FileExplorerTabViewModel createTab(int number, FileExplorerTabState? state)
    {
        FileExplorerViewModel explorer = new(projectPath, projectConfig, gameData, previewService, referenceIndex, state)
        {
            IsReadOnly = isReadOnly,
        };
        explorer.PropertyChanged += onExplorerPropertyChanged;
        explorer.FileClicked += onFileClicked;
        explorer.FileOpened += onFileOpened;
        explorer.DataCreationRequested += onDataCreationRequested;
        explorer.ReferenceTreeRequested += onReferenceTreeRequested;
        explorer.FilesChanging += onFilesChanging;
        explorer.FilesChanged += onFilesChanged;
        return new FileExplorerTabViewModel(number, explorer);
    }

    private void detachTab(FileExplorerTabViewModel tab)
    {
        FileExplorerViewModel explorer = tab.Explorer;
        explorer.PropertyChanged -= onExplorerPropertyChanged;
        explorer.FileClicked -= onFileClicked;
        explorer.FileOpened -= onFileOpened;
        explorer.DataCreationRequested -= onDataCreationRequested;
        explorer.ReferenceTreeRequested -= onReferenceTreeRequested;
        explorer.FilesChanging -= onFilesChanging;
        explorer.FilesChanged -= onFilesChanged;
    }

    private FileExplorerTabState normalizeState(FileExplorerTabState state)
    {
        string path = "Assets";
        if (!Path.IsPathRooted(state.Path)
            && EditorPathSandbox.TryResolve(projectPath, state.Path, out string fullPath, true))
        {
            string? root = new[] { "Assets", "Data" }
                .Select(name => Path.Combine(projectPath, name))
                .FirstOrDefault(candidate => EditorPathSandbox.IsSameOrChildPath(candidate, fullPath));
            if (root is not null)
            {
                while (!Directory.Exists(fullPath) && !string.Equals(fullPath, root, OperatingSystem.IsWindows()
                    ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal))
                    fullPath = Path.GetDirectoryName(fullPath)!;
                if (Directory.Exists(fullPath))
                    path = Path.GetRelativePath(projectPath, fullPath).Replace('\\', '/');
            }
        }
        return state with
        {
            Path = path,
            Zoom = double.IsFinite(state.Zoom) && state.Zoom > 0 ? Math.Clamp(state.Zoom, 50, 200) : 100,
        };
    }

    private void updateActivity()
    {
        foreach (FileExplorerTabViewModel tab in Tabs)
            tab.Explorer.IsActive = visible && ReferenceEquals(tab, activeTab);
    }

    private void saveState()
    {
        if (disposed || suppressPersistence || isReadOnly)
            return;
        projectConfig.FileExplorerWorkspace = new FileExplorerWorkspaceState(
            Tabs.Select(tab => tab.Explorer.CaptureTabState(tab.Number)).ToArray(), activeTab.Number);
    }

    private void onExplorerPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (args.PropertyName is nameof(FileExplorerViewModel.IconView) or nameof(FileExplorerViewModel.Zoom)
            or nameof(FileExplorerViewModel.IsSourcesExpanded)
            || args.PropertyName == nameof(FileExplorerViewModel.IsLoading)
                && sender is FileExplorerViewModel { IsLoading: false, LoadingError.Length: 0 })
            saveState();
    }

    private void onFileClicked(object? sender, FileExplorerFileEventArgs args) => FileClicked?.Invoke(sender, args);
    private void onFileOpened(object? sender, FileExplorerFileEventArgs args) => FileOpened?.Invoke(sender, args);
    private void onDataCreationRequested(object? sender, EditorDataCreationRequest args) => DataCreationRequested?.Invoke(sender, args);
    private void onReferenceTreeRequested(object? sender, string args) => ReferenceTreeRequested?.Invoke(sender, args);
    private void onFilesChanging(object? sender, FileExplorerFilesChangedEventArgs args) => FilesChanging?.Invoke(sender, args);

    private void onFilesChanged(object? sender, FileExplorerFilesChangedEventArgs args)
    {
        ApplyFileChanges(args);
        FilesChanged?.Invoke(sender, args);
    }
}
