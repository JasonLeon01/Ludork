using System.Text.Json.Nodes;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.ViewModels;

public sealed partial class FileExplorerViewModel : ViewModelBase, IDisposable
{
    private static readonly List<string> clipboardPaths = [];
    private static bool clipboardCut;
    private readonly string projectPath;
    private readonly ProjectConfigService projectConfig;
    private readonly GameDataService gameData;
    private readonly BlueprintPreviewService previewService;
    private readonly FileIconService iconService;
    private readonly ReferenceIndexService referenceIndex;
    private readonly ExternalIdeService externalIdeService;
    private bool previewsActive;
    private bool disposed;
    private bool refreshingEntries;
    [ObservableProperty] private string currentPath;
    [ObservableProperty] private bool iconView = true;
    [ObservableProperty] private FileExplorerEntryViewModel? selectedEntry;

    public FileExplorerViewModel(
        string projectPath,
        ProjectConfigService projectConfig,
        GameDataService gameData,
        BlueprintPreviewService previewService,
        FileIconService iconService,
        ReferenceIndexService referenceIndex)
    {
        this.projectPath = Path.GetFullPath(projectPath);
        this.projectConfig = projectConfig;
        this.gameData = gameData;
        this.previewService = previewService;
        this.iconService = iconService;
        this.referenceIndex = referenceIndex;
        externalIdeService = new ExternalIdeService(this.projectPath, !projectConfig.IsStandalone);
        string? savedPath = projectConfig.LastFileExplorerPath;
        currentPath = !string.IsNullOrWhiteSpace(savedPath)
            && Directory.Exists(Path.Combine(this.projectPath, savedPath))
            ? Path.Combine(this.projectPath, savedPath)
            : this.projectPath;
        refreshBreadcrumbs();
        gameData.DataReloaded += onGameDataChanged;
        gameData.Documents.Changed += onDocumentsChanged;
        gameData.DataSaved += onGameDataChanged;
    }

    public ObservableCollection<FileExplorerEntryViewModel> Entries { get; } = [];
    public ObservableCollection<FileExplorerBreadcrumbViewModel> BreadcrumbItems { get; } = [];
    public event EventHandler<string>? FileClicked;
    public event EventHandler<string>? FileOpened;
    public event EventHandler<EditorDataCreationRequest>? DataCreationRequested;
    public event EventHandler<string>? ReferenceTreeRequested;
    public event EventHandler<FileExplorerFilesChangedEventArgs>? FilesChanging;
    public event EventHandler<FileExplorerFilesChangedEventArgs>? FilesChanged;

    public string ProjectPath => projectPath;
    public bool HasClipboard => clipboardPaths.Any(pathExists);

    public void RequestDataCreation(EditorDataCreationRequest request)
    {
        DataCreationRequested?.Invoke(this, request);
    }

    public void RequestReferenceTree(string path)
    {
        ReferenceTreeRequested?.Invoke(this, path);
    }

    public bool HasBlueprint(string key)
    {
        return gameData.BlueprintsData.ContainsKey(key.Replace('\\', '/').Trim('/'));
    }

    public bool CanShowReferenceTree(string path)
    {
        return referenceIndex.GetNodeIdForPath(path) is not null;
    }
    public string Breadcrumb => Path.GetRelativePath(projectPath, CurrentPath) is "." ? projectPath : Path.GetRelativePath(projectPath, CurrentPath);
    public bool CanGoUp => !string.Equals(CurrentPath, projectPath, StringComparison.OrdinalIgnoreCase);
    public string FileExplorerViewMode => LocaleService.Get(
        IconView ? "FILE_EXPLORER_LIST_VIEW" : "FILE_EXPLORER_ICON_VIEW");
    public string FileExplorerViewModeIcon => IconView ? "☷" : "▦";
    public string OpenContainingFolder => LocaleService.Get("OPEN_CONTAINING_FOLDER");
    public string ExternalEditorVSCode => LocaleService.Get("EXTERNAL_EDITOR_VSCODE");
    public string ExternalEditorCursor => LocaleService.Get("EXTERNAL_EDITOR_CURSOR");
    public string ExternalEditorClion => LocaleService.Get("EXTERNAL_EDITOR_CLION");
    public string ExternalEditorVisualStudio => LocaleService.Get("EXTERNAL_EDITOR_VISUAL_STUDIO");
    public bool HasVSCode => externalIdeService.IsInstalled(ExternalIde.VsCode);
    public bool HasCursor => externalIdeService.IsInstalled(ExternalIde.Cursor);
    public bool HasClion => !projectConfig.IsStandalone && externalIdeService.IsInstalled(ExternalIde.Clion);
    public bool HasVisualStudio => !projectConfig.IsStandalone
        && OperatingSystem.IsWindows()
        && externalIdeService.IsInstalled(ExternalIde.VisualStudio);

    partial void OnIconViewChanged(bool value)
    {
        OnPropertyChanged(nameof(FileExplorerViewMode));
        OnPropertyChanged(nameof(FileExplorerViewModeIcon));
    }

    partial void OnCurrentPathChanged(string value)
    {
        OnPropertyChanged(nameof(Breadcrumb));
        OnPropertyChanged(nameof(CanGoUp));
        refreshBreadcrumbs();
    }

    partial void OnSelectedEntryChanged(FileExplorerEntryViewModel? value)
    {
        if (value is not null && !value.IsDirectory)
            FileClicked?.Invoke(this, value.FullPath);
    }

    public void Refresh()
    {
        if (disposed || refreshingEntries)
            return;
        refreshingEntries = true;
        try
        {
            SelectedEntry = null;
            foreach (FileExplorerEntryViewModel entry in Entries)
                entry.Dispose();
            Entries.Clear();
            foreach (string path in enumerateVisiblePaths().ToArray())
                Entries.Add(createEntry(path));
            visibleDocumentPaths = gameData.Documents.All
                .ToDictionary(document => document.Id, document => (document.Path, document.Exists));
        }
        finally
        {
            refreshingEntries = false;
        }
    }

    public void NavigateTo(string path)
    {
        string fullPath = Path.GetFullPath(path);
        if (!isVisibleDirectory(fullPath) || !isUnderRoot(fullPath))
            return;
        CurrentPath = fullPath;
        projectConfig.LastFileExplorerPath = Path.GetRelativePath(projectPath, fullPath);
        Refresh();
    }

    public void GoUp()
    {
        string? parent = Directory.GetParent(CurrentPath)?.FullName;
        if (parent is not null)
            NavigateTo(parent);
    }

    public bool LocatePath(string path)
    {
        if (!pathExists(path))
            return false;
        string fullPath = Path.GetFullPath(path);
        if (!isUnderRoot(fullPath))
            return false;
        NavigateTo(Directory.GetParent(fullPath)?.FullName ?? projectPath);
        SelectedEntry = Entries.FirstOrDefault(entry => string.Equals(entry.FullPath, fullPath, StringComparison.OrdinalIgnoreCase));
        return SelectedEntry is not null;
    }

    public void OpenSelected()
    {
        if (SelectedEntry is null)
            return;
        if (SelectedEntry.IsDirectory)
            NavigateTo(SelectedEntry.FullPath);
        else
            FileOpened?.Invoke(this, SelectedEntry.FullPath);
    }

    public void CopySelected(bool cut)
    {
        if (SelectedEntry is null)
            return;
        SetClipboard([SelectedEntry.FullPath], cut);
    }

    public void SetClipboard(IEnumerable<string> paths, bool cut)
    {
        clipboardPaths.Clear();
        clipboardPaths.AddRange(normalizeTopLevelPaths(paths));
        clipboardCut = cut && clipboardPaths.Count != 0;
    }

    public bool RequiresIdeInitialization(ExternalIde ide)
    {
        return externalIdeService.RequiresInitialization(ide);
    }

    public Task<IdeInitializationResult> InitializeIdeAsync(ExternalIde ide)
    {
        return externalIdeService.InitializeAsync(ide);
    }

    public bool OpenExternalIde(ExternalIde ide)
    {
        return externalIdeService.Open(ide);
    }

    public bool OpenCurrentFolder()
    {
        ProcessStartInfo startInfo = new();
        if (OperatingSystem.IsMacOS())
        {
            startInfo.FileName = "/usr/bin/open";
            startInfo.UseShellExecute = false;
            startInfo.ArgumentList.Add(CurrentPath);
        }
        else if (OperatingSystem.IsWindows())
        {
            startInfo.FileName = "explorer.exe";
            startInfo.UseShellExecute = false;
            startInfo.ArgumentList.Add(CurrentPath);
        }
        else
        {
            startInfo.FileName = "xdg-open";
            startInfo.UseShellExecute = false;
            startInfo.ArgumentList.Add(CurrentPath);
        }
        return startProcess(startInfo);
    }

    private static bool startProcess(ProcessStartInfo startInfo)
    {
        try
        {
            Process.Start(startInfo);
            return true;
        }
        catch (Win32Exception)
        {
            return false;
        }
        catch (InvalidOperationException)
        {
            return false;
        }
    }

    private FileExplorerEntryViewModel createEntry(string path)
    {
        bool directory = isVisibleDirectory(path);
        int iconSize = IconView ? 80 : 28;
        IImage? icon;
        ActorPreviewLease? previewLease = null;
        if (!directory && isImage(path))
        {
            icon = loadImageThumbnail(path, iconSize)
                ?? iconService.getShellIcon(path, false, iconSize);
        }
        else if (!directory && isBlueprint(path, out string blueprintKey))
        {
            gameData.BlueprintsData.TryGetValue(blueprintKey, out JsonObject? blueprint);
            Bitmap? fallback = previewService.tryLoadPreview(blueprint ?? [], iconSize, blueprintKey);
            ActorVisualDescriptor? descriptor = previewService.tryResolveActorVisual(
                blueprint ?? [],
                blueprintKey);
            icon = fallback ?? iconService.getShellIcon(path, false, iconSize);
            previewLease = descriptor is { RequiresPreviewService: true }
                ? previewService.ActorPreviews.Acquire(descriptor, iconSize, previewsActive)
                : null;
        }
        else
        {
            icon = iconService.getShellIcon(path, directory, iconSize);
        }
        FileExplorerEntryViewModel entry = new(path, directory, icon, previewLease);
        entry.IsPreviewActive = previewsActive;
        entry.IsModified = !directory && gameData.Documents.FindByPath(path)?.IsModified == true;
        return entry;
    }

    public void SetPreviewActive(bool active)
    {
        previewsActive = active;
        foreach (FileExplorerEntryViewModel entry in Entries)
            entry.IsPreviewActive = active;
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        gameData.DataReloaded -= onGameDataChanged;
        gameData.Documents.Changed -= onDocumentsChanged;
        gameData.DataSaved -= onGameDataChanged;
        foreach (FileExplorerEntryViewModel entry in Entries)
            entry.Dispose();
        Entries.Clear();
        SelectedEntry = null;
    }

    private void onGameDataChanged(object? sender, EventArgs args)
    {
        Refresh();
    }

    private bool shouldDisplay(string path)
    {
        if (!DataConfig.shouldDisplay(path))
            return false;
        if (Directory.Exists(path))
        {
            return isInsideTextConfigs(path)
                ? hasVisibleTextConfigContent(path)
                : true;
        }
        if (!tryGetTextConfigKey(path, out string key))
        {
            return true;
        }
        return gameData.TextConfigsData.ContainsKey(key);
    }

}

public sealed record FileOperationResult(bool Changed, IReadOnlyList<string> Errors)
{
    public static FileOperationResult Empty { get; } = new(false, Array.Empty<string>());
}

public sealed record FileExplorerBreadcrumbViewModel(string Label, string Path, bool ShowSeparator = false);
