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
using System.Threading;
using System.Threading.Tasks;
using Ludork.Views.Utils;

namespace Ludork.ViewModels;

public sealed partial class FileExplorerViewModel : ViewModelBase, IDisposable
{
    private static readonly List<string> clipboardPaths = [];
    private static bool clipboardCut;
    private readonly string projectPath;
    private readonly ProjectConfigService projectConfig;
    private readonly GameDataService gameData;
    private readonly BlueprintPreviewService previewService;
    private readonly ReferenceIndexService referenceIndex;
    private readonly ExternalIdeService externalIdeService;
    private bool disposed;
    [ObservableProperty] private bool isLoading;
    [ObservableProperty] private string loadingError = string.Empty;
    [ObservableProperty] private string currentPath;
    [ObservableProperty] private bool iconView = true;
    [ObservableProperty] private FileExplorerEntryViewModel? selectedEntry;

    public FileExplorerViewModel(
        string projectPath,
        ProjectConfigService projectConfig,
        GameDataService gameData,
        BlueprintPreviewService previewService,
        ReferenceIndexService referenceIndex)
    {
        this.projectPath = Path.GetFullPath(projectPath);
        this.projectConfig = projectConfig;
        this.gameData = gameData;
        this.previewService = previewService;
        this.referenceIndex = referenceIndex;
        externalIdeService = new ExternalIdeService(this.projectPath, !projectConfig.IsStandalone);
        string? savedPath = projectConfig.LastFileExplorerPath;
        currentPath = !string.IsNullOrWhiteSpace(savedPath)
            && Directory.Exists(Path.Combine(this.projectPath, savedPath))
            ? Path.GetFullPath(Path.Combine(this.projectPath, savedPath))
            : this.projectPath;
        refreshBreadcrumbs();
        gameData.DataReloaded += onGameDataChanged;
        gameData.Documents.Changed += onDocumentsChanged;
        gameData.DataSaved += onGameDataChanged;
        previewService.VisualsInvalidated += onVisualsInvalidated;
    }

    public ObservableCollection<FileExplorerEntryViewModel> Entries { get; } = [];
    public ObservableCollection<FileExplorerBreadcrumbViewModel> BreadcrumbItems { get; } = [];
    public event EventHandler<FileExplorerFileEventArgs>? FileClicked;
    public event EventHandler<FileExplorerFileEventArgs>? FileOpened;
    public event EventHandler<EditorDataCreationRequest>? DataCreationRequested;
    public event EventHandler<string>? ReferenceTreeRequested;
    public event EventHandler<FileExplorerFilesChangedEventArgs>? FilesChanging;
    public event EventHandler<FileExplorerFilesChangedEventArgs>? FilesChanged;

    public string ProjectPath => projectPath;
    public EditorThumbnailService Thumbnails => gameData.Thumbnails;
    public bool IsReadOnly { get; set; }
    public bool HasClipboard => clipboardPaths.Any(pathExists);

    public void RequestDataCreation(EditorDataCreationRequest request)
    {
        if (IsReadOnly)
            return;
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
    public string ParentFolder => LocaleService.Get("FILE_DIALOG_PARENT_FOLDER");
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
            _ = notifyFileClickedAsync(value);
    }

    public void CopySelected(bool cut)
    {
        if (SelectedEntry is null)
            return;
        SetClipboard([SelectedEntry.FullPath], cut);
    }

    public void SetClipboard(IEnumerable<string> paths, bool cut)
    {
        if (IsReadOnly && cut)
            return;
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

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        navigation?.Cancel();
        navigation?.Dispose();
        gameData.DataReloaded -= onGameDataChanged;
        gameData.Documents.Changed -= onDocumentsChanged;
        gameData.DataSaved -= onGameDataChanged;
        previewService.VisualsInvalidated -= onVisualsInvalidated;
        foreach (FileExplorerEntryViewModel entry in Entries)
            entry.Dispose();
        Entries.Clear();
        SelectedEntry = null;
    }

    private void onGameDataChanged(object? sender, EventArgs args) => RequestRefresh();

    private void onVisualsInvalidated(object? sender, EventArgs args)
    {
        visualVersion++;
        foreach (FileExplorerEntryViewModel entry in Entries)
            updateDocumentState(entry);
    }

}

public sealed record FileOperationResult(bool Changed, IReadOnlyList<string> Errors)
{
    public static FileOperationResult Empty { get; } = new(false, Array.Empty<string>());
}

public sealed record FileExplorerBreadcrumbViewModel(string Label, string Path, bool ShowSeparator = false);
