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
    private bool previewsActive;
    private bool disposed;
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
        string? savedPath = projectConfig.LastFileExplorerPath;
        currentPath = !string.IsNullOrWhiteSpace(savedPath)
            && Directory.Exists(Path.Combine(this.projectPath, savedPath))
            ? Path.Combine(this.projectPath, savedPath)
            : this.projectPath;
        refreshBreadcrumbs();
        gameData.DataReloaded += onGameDataChanged;
        gameData.DataRestored += onGameDataChanged;
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
    public bool HasClipboard => clipboardPaths.Any(path => File.Exists(path) || Directory.Exists(path));

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
        if (disposed)
            return;
        SelectedEntry = null;
        foreach (FileExplorerEntryViewModel entry in Entries)
            entry.Dispose();
        Entries.Clear();
        foreach (string path in Directory.EnumerateFileSystemEntries(CurrentPath)
                     .Where(shouldDisplay)
                     .OrderBy(path => !Directory.Exists(path))
                     .ThenBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase))
            Entries.Add(createEntry(path));
    }

    public void NavigateTo(string path)
    {
        string fullPath = Path.GetFullPath(path);
        if (!Directory.Exists(fullPath) || !isUnderRoot(fullPath))
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
        if (!File.Exists(path) && !Directory.Exists(path))
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

    public FileOperationResult Paste(string? targetDirectory = null)
    {
        string target = getTargetDirectory(targetDirectory);
        List<string> errors = [];
        List<string> externalAdded = [];
        List<string> uiAdded = [];
        List<(string OldPath, string NewPath)> moved = [];
        foreach (string source in clipboardPaths.Where(path => File.Exists(path) || Directory.Exists(path)).ToArray())
        {
            string destination = Path.Combine(target, Path.GetFileName(source));
            if (!canTransfer(source, target, destination, out string? error))
            {
                if (!string.IsNullOrWhiteSpace(error))
                    errors.Add(error);
                continue;
            }
            try
            {
                if (clipboardCut)
                {
                    changing(new FileExplorerFilesChangedEventArgs(
                        [],
                        [(source, destination)],
                        []));
                    movePath(source, destination);
                    moved.Add((source, destination));
                }
                else
                {
                    bool handled = copyManagedUiAssets(
                        source,
                        destination,
                        uiAdded,
                        out string? uiError);
                    if (uiError is not null)
                    {
                        errors.Add(uiError);
                        continue;
                    }
                    if (handled)
                    {
                        if (Directory.Exists(source))
                            externalAdded.Add(destination);
                        continue;
                    }
                    copyPath(source, destination);
                    externalAdded.Add(destination);
                }
            }
            catch (Exception exception)
            {
                errors.Add($"{Path.GetFileName(source)}: {exception.Message}");
            }
        }
        try
        {
            Exception? notificationError = applyFileChanges(externalAdded, moved, []);
            if (notificationError is not null)
                errors.Add(notificationError.Message);
            if (moved.Count != 0)
            {
                clipboardPaths.Clear();
                clipboardCut = false;
            }
        }
        catch (Exception exception)
        {
            rollbackMoves(moved);
            errors.Add(exception.Message);
            moved.Clear();
            if (externalAdded.Count != 0)
            {
                Exception? notificationError = applyFileChanges(externalAdded, [], []);
                if (notificationError is not null)
                    errors.Add(notificationError.Message);
            }
        }
        if (uiAdded.Count != 0)
        {
            changed(new FileExplorerFilesChangedEventArgs(
                uiAdded,
                [],
                []));
        }
        return new FileOperationResult(
            externalAdded.Count != 0 || uiAdded.Count != 0 || moved.Count != 0,
            errors);
    }

    private bool copyManagedUiAssets(
        string source,
        string destination,
        ICollection<string> uiAdded,
        out string? error)
    {
        error = null;
        if (File.Exists(source))
        {
            DataFileInfo? info = gameData.TryLoadDataFile(source);
            if (info is not { Type: "uiAsset", Key: not null })
                return false;
            string sourceKey = UiAssetSchema.ToLogicalAssetKey(info.Key);
            if (!tryGetUiAssetKeyForPath(destination, out string destinationKey)
                || sourceKey.Length == 0
                || !gameData.CopyUiAsset(sourceKey, destinationKey))
            {
                error = $"{Path.GetFileName(source)}: {LocaleService.Get("DUPLICATE_FAILED")}";
                return true;
            }
            uiAdded.Add(destination);
            return true;
        }
        if (!Directory.Exists(source))
            return false;
        List<(string SourcePath, string SourceKey, string DestinationPath, string DestinationKey)> mappings = [];
        foreach (string file in Directory.EnumerateFiles(
                     source,
                     "*" + DataConfig.DataFileExtension,
                     SearchOption.AllDirectories))
        {
            DataFileInfo? info = gameData.TryLoadDataFile(file);
            if (info is not { Type: "uiAsset", Key: not null })
                continue;
            string sourceKey = UiAssetSchema.ToLogicalAssetKey(info.Key);
            if (sourceKey.Length == 0)
                continue;
            string relative = Path.GetRelativePath(source, file);
            string destinationPath = Path.Combine(destination, relative);
            if (!tryGetUiAssetKeyForPath(destinationPath, out string destinationKey)
                || gameData.UiAssetsData.ContainsKey(UiAssetSchema.ToAssetDataKey(destinationKey)))
            {
                error = $"{Path.GetFileName(file)}: {LocaleService.Get("DUPLICATE_FAILED")}";
                return true;
            }
            mappings.Add((file, sourceKey, destinationPath, destinationKey));
        }
        if (mappings.Count == 0)
            return false;
        HashSet<string> skippedFiles = mappings
            .Select(mapping => Path.GetFullPath(mapping.SourcePath))
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        copyDirectory(source, destination, skippedFiles);
        foreach ((string sourcePath, string sourceKey, string destinationPath, string destinationKey) in mappings)
        {
            if (!gameData.CopyUiAsset(sourceKey, destinationKey))
            {
                error = $"{Path.GetFileName(sourcePath)}: {LocaleService.Get("DUPLICATE_FAILED")}";
                return true;
            }
            uiAdded.Add(destinationPath);
        }
        return true;
    }

    private bool tryGetUiAssetKeyForPath(string path, out string key)
    {
        string uiRoot = Path.GetFullPath(Path.Combine(projectPath, "Data", "UI", "Assets"));
        string fullPath = Path.GetFullPath(path);
        string relative = Path.GetRelativePath(uiRoot, fullPath);
        if (Path.IsPathRooted(relative)
            || relative == ".."
            || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal)
            || !string.Equals(
                Path.GetExtension(fullPath),
                DataConfig.DataFileExtension,
                StringComparison.Ordinal))
        {
            key = string.Empty;
            return false;
        }
        key = UiAssetSchema.NormalizeAssetKey(
            Path.ChangeExtension(relative, null)!.Replace('\\', '/'));
        if (key.Length == 0)
            return false;
        string expectedRelative =
            key.Replace('/', Path.DirectorySeparatorChar)
            + DataConfig.DataFileExtension;
        return string.Equals(
            expectedRelative,
            relative,
            StringComparison.Ordinal);
    }

    public FileOperationResult Duplicate(IEnumerable<string> paths)
    {
        List<string> errors = [];
        List<string> externalAdded = [];
        List<string> uiAdded = [];
        foreach (string source in normalizeTopLevelPaths(paths).Where(File.Exists))
        {
            DataFileInfo? info = gameData.TryLoadDataFile(source);
            if (info is { Type: "uiAsset", Key: not null })
            {
                string uiDestination = getDuplicatePath(source);
                string sourceKey = UiAssetSchema.ToLogicalAssetKey(info.Key);
                if (!tryGetUiAssetKeyForPath(uiDestination, out string destinationKey)
                    || sourceKey.Length == 0
                    || !gameData.CopyUiAsset(sourceKey, destinationKey))
                {
                    errors.Add($"{Path.GetFileName(source)}: {LocaleService.Get("DUPLICATE_FAILED")}");
                    continue;
                }
                uiAdded.Add(uiDestination);
                continue;
            }
            string destination = getDuplicatePath(source);
            try
            {
                File.Copy(source, destination);
                externalAdded.Add(destination);
            }
            catch (Exception exception)
            {
                errors.Add($"{Path.GetFileName(source)}: {exception.Message}");
            }
        }
        Exception? notificationError = applyFileChanges(externalAdded, [], []);
        if (notificationError is not null)
            errors.Add(notificationError.Message);
        if (uiAdded.Count != 0)
        {
            changed(new FileExplorerFilesChangedEventArgs(
                uiAdded,
                [],
                []));
        }
        return new FileOperationResult(
            externalAdded.Count != 0 || uiAdded.Count != 0,
            errors);
    }

    public void DuplicateSelected()
    {
        if (SelectedEntry is null || SelectedEntry.IsDirectory)
            return;
        Duplicate([SelectedEntry.FullPath]);
    }

    public FileOperationResult Delete(IEnumerable<string> paths)
    {
        string[] normalizedPaths = normalizeTopLevelPaths(paths).ToArray();
        ReferenceImpact impact = referenceIndex.GetImpactForPaths(normalizedPaths);
        List<ReferenceRecord> uiIncoming = impact.Incoming
            .Where(record => string.Equals(
                    referenceIndex.GetNode(record.Source)?.Type,
                    "uiAsset",
                    StringComparison.Ordinal)
                || string.Equals(
                    referenceIndex.GetNode(record.Target)?.Type,
                    "uiAsset",
                    StringComparison.Ordinal))
            .ToList();
        if (uiIncoming.Count != 0)
        {
            string references = string.Join(
                Environment.NewLine,
                uiIncoming.Select(formatIncomingReference));
            return new FileOperationResult(
                false,
                [
                    LocaleService.Get("UI_ASSET_DELETE_REFERENCED")
                        .Replace("{references}", references, StringComparison.Ordinal),
                ]);
        }
        Dictionary<string, string> textConfigPaths =
            getLoadedTextConfigPaths(normalizedPaths);
        HashSet<string> deferredDirectories = normalizedPaths
            .Where(Directory.Exists)
            .Where(path => textConfigPaths.Keys.Any(
                textConfigPath => isSameOrChildPath(path, textConfigPath)))
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        bool textConfigsDeleted = gameData.DeleteTextConfigs(textConfigPaths.Values);
        List<string> errors = [];
        List<string> deleted = [];
        foreach (string path in normalizedPaths)
        {
            if (textConfigPaths.ContainsKey(path))
                continue;
            if (deferredDirectories.Contains(path))
            {
                deleteOrdinaryDirectoryContents(
                    path,
                    textConfigPaths.Keys,
                    deleted,
                    errors);
                continue;
            }
            try
            {
                if (Directory.Exists(path))
                    Directory.Delete(path, true);
                else if (File.Exists(path))
                    File.Delete(path);
                else
                    continue;
                deleted.Add(path);
            }
            catch (Exception exception)
            {
                errors.Add($"{Path.GetFileName(path)}: {exception.Message}");
            }
        }
        List<string> changedDeleted = [..deleted];
        if (textConfigsDeleted)
            changedDeleted.AddRange(textConfigPaths.Keys);
        if (deleted.Count != 0)
        {
            Exception? notificationError = applyFileChanges([], [], deleted, changedDeleted);
            if (notificationError is not null)
                errors.Add(notificationError.Message);
        }
        else if (textConfigsDeleted)
            changed(new FileExplorerFilesChangedEventArgs(
                [],
                [],
                changedDeleted));
        if (!Directory.Exists(CurrentPath))
            NavigateTo(projectPath);
        return new FileOperationResult(deleted.Count != 0 || textConfigsDeleted, errors);
    }

    private string formatIncomingReference(ReferenceRecord record)
    {
        ReferenceNode? source = referenceIndex.GetNode(record.Source);
        string sourceName = source?.Key ?? record.Source;
        return $"{sourceName} [{record.Kind}] {record.Path}";
    }

    public void DeleteSelected()
    {
        if (SelectedEntry is not null)
            Delete([SelectedEntry.FullPath]);
    }

    public FileOperationResult CreateDirectory(string name, string? targetDirectory = null)
    {
        string target = getTargetDirectory(targetDirectory);
        if (!tryGetChildPath(target, name, out string newPath))
            return new FileOperationResult(false, [LocaleService.Get("CREATE_FOLDER_FAILED") + Environment.NewLine + name]);
        if (File.Exists(newPath) || Directory.Exists(newPath))
            return new FileOperationResult(false, [LocaleService.Get("FILE_ALREADY_EXISTS").Replace("{name}", name)]);
        try
        {
            Directory.CreateDirectory(newPath);
            changed(new FileExplorerFilesChangedEventArgs([newPath], [], []));
            return new FileOperationResult(true, []);
        }
        catch (Exception exception)
        {
            return new FileOperationResult(false, [$"{name}: {exception.Message}"]);
        }
    }

    public FileOperationResult RenameSelected(string newName)
    {
        if (SelectedEntry is null)
            return FileOperationResult.Empty;
        string oldPath = SelectedEntry.FullPath;
        string trimmedName = newName.Trim();
        if (!tryGetChildPath(CurrentPath, trimmedName, out string newPath))
            return new FileOperationResult(false, [LocaleService.Get("RENAME_FAILED") + Environment.NewLine + newName]);
        if (string.Equals(oldPath, newPath, StringComparison.Ordinal))
            return FileOperationResult.Empty;
        bool caseOnlyRename = isCaseOnlyPathChange(oldPath, newPath);
        if (!caseOnlyRename && (File.Exists(newPath) || Directory.Exists(newPath)))
            return new FileOperationResult(false, [LocaleService.Get("FILE_ALREADY_EXISTS").Replace("{name}", trimmedName)]);
        if (!canTransfer(oldPath, CurrentPath, newPath, out string? transferError))
        {
            return new FileOperationResult(
                false,
                [transferError ?? LocaleService.Get("RENAME_FAILED") + Environment.NewLine + newName]);
        }
        try
        {
            changing(new FileExplorerFilesChangedEventArgs(
                [],
                [(oldPath, newPath)],
                []));
            movePath(oldPath, newPath);
            Exception? notificationError = applyFileChanges([], [(oldPath, newPath)], []);
            SelectedEntry = Entries.FirstOrDefault(entry => string.Equals(entry.FullPath, newPath, StringComparison.OrdinalIgnoreCase));
            return new FileOperationResult(
                true,
                notificationError is null ? [] : [notificationError.Message]);
        }
        catch (Exception exception)
        {
            rollbackMoves([(oldPath, newPath)]);
            return new FileOperationResult(false, [$"{trimmedName}: {exception.Message}"]);
        }
    }

    public FileOperationResult Move(IEnumerable<string> paths, string targetDirectory)
    {
        string target = getTargetDirectory(targetDirectory);
        List<string> errors = [];
        List<(string OldPath, string NewPath)> moved = [];
        foreach (string source in normalizeTopLevelPaths(paths))
        {
            string destination = Path.Combine(target, Path.GetFileName(source));
            if (!canTransfer(source, target, destination, out string? error))
            {
                if (!string.IsNullOrWhiteSpace(error))
                    errors.Add(error);
                continue;
            }
            try
            {
                changing(new FileExplorerFilesChangedEventArgs(
                    [],
                    [(source, destination)],
                    []));
                movePath(source, destination);
                moved.Add((source, destination));
            }
            catch (Exception exception)
            {
                errors.Add($"{Path.GetFileName(source)}: {exception.Message}");
            }
        }
        try
        {
            Exception? notificationError = applyFileChanges([], moved, []);
            if (notificationError is not null)
                errors.Add(notificationError.Message);
        }
        catch (Exception exception)
        {
            rollbackMoves(moved);
            errors.Add(exception.Message);
            moved.Clear();
        }
        return new FileOperationResult(moved.Count != 0, errors);
    }

    public bool OpenExternalEditor(string command)
    {
        ProcessStartInfo startInfo = new();
        if (OperatingSystem.IsMacOS())
        {
            startInfo.FileName = "/usr/bin/open";
            startInfo.UseShellExecute = false;
            startInfo.ArgumentList.Add("-a");
            startInfo.ArgumentList.Add(command == "code" ? "Visual Studio Code" : "Cursor");
            startInfo.ArgumentList.Add(projectPath);
        }
        else
        {
            startInfo.FileName = command;
            startInfo.UseShellExecute = true;
            startInfo.Arguments = $"\"{projectPath}\"";
        }
        return startProcess(startInfo);
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
        bool directory = Directory.Exists(path);
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
        gameData.DataRestored -= onGameDataChanged;
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

    private Dictionary<string, string> getLoadedTextConfigPaths(
        IEnumerable<string> paths)
    {
        Dictionary<string, string> result =
            new(StringComparer.OrdinalIgnoreCase);
        foreach (string path in paths)
        {
            if (File.Exists(path))
            {
                addLoadedTextConfigPath(path, result);
                continue;
            }
            if (!Directory.Exists(path) || !isInsideTextConfigs(path))
                continue;
            foreach (string filePath in Directory.EnumerateFiles(
                         path,
                         "*" + DataConfig.DataFileExtension,
                         SearchOption.AllDirectories))
            {
                addLoadedTextConfigPath(filePath, result);
            }
        }
        return result;
    }

    private void addLoadedTextConfigPath(
        string path,
        Dictionary<string, string> result)
    {
        if (tryGetTextConfigKey(path, out string key)
            && gameData.TextConfigsData.ContainsKey(key))
        {
            result[path] = key;
        }
    }

    private void deleteOrdinaryDirectoryContents(
        string directory,
        IEnumerable<string> deferredPaths,
        List<string> deleted,
        List<string> errors)
    {
        HashSet<string> deferred =
            deferredPaths.ToHashSet(StringComparer.OrdinalIgnoreCase);
        foreach (string filePath in Directory.EnumerateFiles(
                     directory,
                     "*",
                     SearchOption.AllDirectories))
        {
            if (deferred.Contains(filePath))
                continue;
            try
            {
                File.Delete(filePath);
                deleted.Add(filePath);
            }
            catch (Exception exception)
            {
                errors.Add($"{Path.GetFileName(filePath)}: {exception.Message}");
            }
        }
        foreach (string directoryPath in Directory.EnumerateDirectories(
                     directory,
                     "*",
                     SearchOption.AllDirectories)
                     .OrderByDescending(path => path.Length))
        {
            try
            {
                if (!Directory.EnumerateFileSystemEntries(directoryPath).Any())
                    Directory.Delete(directoryPath);
            }
            catch (Exception exception)
            {
                errors.Add($"{Path.GetFileName(directoryPath)}: {exception.Message}");
            }
        }
    }

    private bool hasVisibleTextConfigContent(string directory)
    {
        foreach (string path in Directory.EnumerateFileSystemEntries(directory))
        {
            if (Directory.Exists(path))
            {
                if (hasVisibleTextConfigContent(path))
                    return true;
                continue;
            }
            if (!tryGetTextConfigKey(path, out string key)
                || gameData.TextConfigsData.ContainsKey(key))
            {
                return true;
            }
        }
        return false;
    }

    private bool isInsideTextConfigs(string path)
    {
        string root = Path.GetFullPath(Path.Combine(
            projectPath,
            "Data",
            "TextConfigs"));
        string relative = Path.GetRelativePath(root, Path.GetFullPath(path));
        return !Path.IsPathRooted(relative)
            && relative != ".."
            && !relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal);
    }

    private static bool isSameOrChildPath(string directory, string path)
    {
        string relative = Path.GetRelativePath(
            Path.GetFullPath(directory),
            Path.GetFullPath(path));
        return !Path.IsPathRooted(relative)
            && relative != ".."
            && !relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal);
    }

    private bool tryGetTextConfigKey(string path, out string key)
    {
        key = string.Empty;
        string root = Path.GetFullPath(Path.Combine(projectPath, "Data", "TextConfigs"));
        string fullPath = Path.GetFullPath(path);
        string relative = Path.GetRelativePath(root, fullPath);
        if (Path.IsPathRooted(relative)
            || relative == ".."
            || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal)
            || !string.Equals(
                Path.GetExtension(fullPath),
                DataConfig.DataFileExtension,
                StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }
        DataFileInfo? info = gameData.TryLoadDataFile(fullPath);
        if (info?.Type is not ("plainTextConfig" or "richTextConfig")
            || string.IsNullOrWhiteSpace(info.Key))
        {
            return false;
        }
        key = info.Key;
        return true;
    }

    private static Bitmap? loadImageThumbnail(string path, int size)
    {
        using FileStream stream = File.OpenRead(path);
        return Bitmap.DecodeToWidth(stream, size);
    }

    private bool isBlueprint(string path, out string key)
    {
        key = string.Empty;
        string blueprintsRoot = Path.Combine(projectPath, "Data", "Blueprints");
        if (!path.StartsWith(blueprintsRoot, StringComparison.OrdinalIgnoreCase)
            || !Path.GetExtension(path).Equals(DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase)
            || gameData.TryLoadDataFile(path)?.Type != "blueprint")
            return false;
        key = Path.GetRelativePath(blueprintsRoot, path).Replace('\\', '/');
        key = Path.ChangeExtension(key, null)!;
        return true;
    }

    private void changed(FileExplorerFilesChangedEventArgs changes)
    {
        Refresh();
        FilesChanged?.Invoke(this, changes);
    }

    private void changing(FileExplorerFilesChangedEventArgs changes)
    {
        FilesChanging?.Invoke(this, changes);
    }

    private static void rollbackMoves(
        IReadOnlyList<(string OldPath, string NewPath)> moved)
    {
        foreach ((string oldPath, string newPath) in moved.Reverse())
        {
            if (isCaseOnlyPathChange(oldPath, newPath))
            {
                if (File.Exists(newPath) || Directory.Exists(newPath))
                    movePath(newPath, oldPath);
                continue;
            }
            if ((File.Exists(oldPath) || Directory.Exists(oldPath))
                || !File.Exists(newPath) && !Directory.Exists(newPath))
            {
                continue;
            }
            movePath(newPath, oldPath);
        }
    }

    private Exception? applyFileChanges(
        IReadOnlyList<string> added,
        IReadOnlyList<(string OldPath, string NewPath)> moved,
        IReadOnlyList<string> deleted,
        IReadOnlyList<string>? changedDeleted = null)
    {
        if (added.Count == 0 && moved.Count == 0 && deleted.Count == 0)
            return null;
        gameData.ApplyExternalFileChanges(added, moved, deleted);
        try
        {
            changed(new FileExplorerFilesChangedEventArgs(
                added.ToArray(),
                moved.ToArray(),
                changedDeleted?.ToArray() ?? deleted.ToArray()));
            return null;
        }
        catch (Exception exception)
        {
            return exception;
        }
    }

    private void refreshBreadcrumbs()
    {
        BreadcrumbItems.Clear();
        BreadcrumbItems.Add(new FileExplorerBreadcrumbViewModel(projectPath, projectPath));
        string relative = Path.GetRelativePath(projectPath, CurrentPath);
        if (relative is "." or "")
            return;
        string current = projectPath;
        foreach (string part in relative.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar))
        {
            if (string.IsNullOrWhiteSpace(part))
                continue;
            current = Path.Combine(current, part);
            BreadcrumbItems.Add(new FileExplorerBreadcrumbViewModel(part, current, true));
        }
    }

    public bool IsUnderRoot(string path) => isUnderRoot(path);

    private bool isUnderRoot(string path)
    {
        string fullPath = Path.GetFullPath(path);
        string relative = Path.GetRelativePath(projectPath, fullPath);
        return relative == "."
            || (!Path.IsPathRooted(relative)
                && relative != ".."
                && !relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal));
    }
    private static bool isImage(string path) => new[] { ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp" }
        .Contains(Path.GetExtension(path), StringComparer.OrdinalIgnoreCase);

    private string getTargetDirectory(string? targetDirectory)
    {
        string target = string.IsNullOrWhiteSpace(targetDirectory) ? CurrentPath : Path.GetFullPath(targetDirectory);
        return Directory.Exists(target) && isUnderRoot(target) ? target : CurrentPath;
    }

    private bool tryGetChildPath(string directory, string name, out string path)
    {
        path = string.Empty;
        string trimmed = name.Trim();
        if (trimmed.Length == 0
            || trimmed is "." or ".."
            || Path.IsPathRooted(trimmed)
            || trimmed.Contains(Path.DirectorySeparatorChar)
            || trimmed.Contains(Path.AltDirectorySeparatorChar)
            || trimmed.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
        {
            return false;
        }
        path = Path.GetFullPath(Path.Combine(directory, trimmed));
        return isUnderRoot(path)
            && string.Equals(Path.GetDirectoryName(path), Path.GetFullPath(directory), StringComparison.OrdinalIgnoreCase);
    }

    private bool canTransfer(string source, string targetDirectory, string destination, out string? error)
    {
        error = null;
        if ((!File.Exists(source) && !Directory.Exists(source)) || !isUnderRoot(source) || !isUnderRoot(targetDirectory))
            return false;
        if (string.Equals(Path.GetFullPath(source), projectPath, StringComparison.OrdinalIgnoreCase))
            return false;
        if (string.Equals(Path.GetFullPath(source), Path.GetFullPath(destination), StringComparison.Ordinal))
            return false;
        if (Directory.Exists(source) && isPathInside(targetDirectory, source))
        {
            error = $"{Path.GetFileName(source)}: {LocaleService.Get("MOVE_FILE_FAILED")}";
            return false;
        }
        if (!isCaseOnlyPathChange(source, destination)
            && (File.Exists(destination) || Directory.Exists(destination)))
        {
            error = LocaleService.Get("FILE_ALREADY_EXISTS").Replace("{name}", Path.GetFileName(destination));
            return false;
        }
        if (gameData.ContainsDataPath(destination, Directory.Exists(source)))
        {
            error = LocaleService.Get("FILE_ALREADY_EXISTS").Replace("{name}", Path.GetFileName(destination));
            return false;
        }
        IReadOnlyList<(string SourcePath, string DestinationPath)> uiAssetMoves =
            getUiAssetTransferPaths(source, destination);
        if (uiAssetMoves.Count != 0)
        {
            string uiAssetsRoot = Path.Combine(projectPath, "Data", "UI", "Assets");
            if (!isPathInside(source, uiAssetsRoot)
                || !isPathInside(destination, uiAssetsRoot)
                || uiAssetMoves.Any(move =>
                    !tryGetUiAssetKeyForPath(move.SourcePath, out _)
                    || !tryGetUiAssetKeyForPath(move.DestinationPath, out _)))
            {
                error = $"{Path.GetFileName(source)}: {LocaleService.Get("MOVE_FILE_FAILED")}";
                return false;
            }
        }
        return true;
    }

    private IReadOnlyList<(string SourcePath, string DestinationPath)> getUiAssetTransferPaths(
        string source,
        string destination)
    {
        string uiAssetsRoot = Path.Combine(projectPath, "Data", "UI", "Assets");
        bool sourceIsFile = File.Exists(source);
        List<(string SourcePath, string DestinationPath)> result = [];
        foreach (string dataKey in gameData.GetUiAssetKeysForMove())
        {
            string logicalKey = UiAssetSchema.ToLogicalAssetKey(dataKey);
            if (logicalKey.Length == 0)
                continue;
            string assetPath = Path.Combine(
                uiAssetsRoot,
                logicalKey.Replace('/', Path.DirectorySeparatorChar)
                    + DataConfig.DataFileExtension);
            if (sourceIsFile)
            {
                if (!string.Equals(
                        Path.GetFullPath(assetPath),
                        Path.GetFullPath(source),
                        OperatingSystem.IsWindows()
                            ? StringComparison.OrdinalIgnoreCase
                            : StringComparison.Ordinal))
                {
                    continue;
                }
                result.Add((assetPath, destination));
                continue;
            }
            if (!Directory.Exists(source) || !isPathInside(assetPath, source))
                continue;
            string relative = Path.GetRelativePath(source, assetPath);
            result.Add((assetPath, Path.Combine(destination, relative)));
        }
        IEnumerable<string> diskFiles = sourceIsFile
            ? [source]
            : Directory.Exists(source)
                ? Directory.EnumerateFiles(source, "*", SearchOption.AllDirectories)
                : [];
        foreach (string diskFile in diskFiles)
        {
            if (gameData.TryLoadDataFile(diskFile)?.Type != UiAssetSchema.UiAssetType
                || result.Any(move => pathsEqual(move.SourcePath, diskFile)))
            {
                continue;
            }
            string destinationPath = sourceIsFile
                ? destination
                : Path.Combine(destination, Path.GetRelativePath(source, diskFile));
            result.Add((diskFile, destinationPath));
        }
        return result;
    }

    private static bool isPathInside(string path, string root)
    {
        string relative = Path.GetRelativePath(Path.GetFullPath(root), Path.GetFullPath(path));
        return relative == "."
            || (!Path.IsPathRooted(relative)
                && relative != ".."
                && !relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal));
    }

    private static IReadOnlyList<string> normalizeTopLevelPaths(IEnumerable<string> paths)
    {
        string[] normalized = paths
            .Where(path => !string.IsNullOrWhiteSpace(path))
            .Select(Path.GetFullPath)
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(path => path.Length)
            .ToArray();
        return normalized
            .Where(path => !normalized.Any(parent => !string.Equals(parent, path, StringComparison.OrdinalIgnoreCase)
                && isPathInside(path, parent)))
            .ToArray();
    }

    private static void movePath(string source, string target)
    {
        if (isCaseOnlyPathChange(source, target))
        {
            string directory = Path.GetDirectoryName(source)!;
            string temporary = Path.Combine(
                directory,
                ".ludork-case-move-" + Guid.NewGuid().ToString("N"));
            if (Directory.Exists(source))
            {
                Directory.Move(source, temporary);
                try
                {
                    Directory.Move(temporary, target);
                }
                catch
                {
                    Directory.Move(temporary, source);
                    throw;
                }
            }
            else
            {
                File.Move(source, temporary);
                try
                {
                    File.Move(temporary, target);
                }
                catch
                {
                    File.Move(temporary, source);
                    throw;
                }
            }
            return;
        }
        if (Directory.Exists(source))
            Directory.Move(source, target);
        else
            File.Move(source, target);
    }

    private static bool isCaseOnlyPathChange(string source, string target)
    {
        return OperatingSystem.IsWindows()
            && !string.Equals(source, target, StringComparison.Ordinal)
            && string.Equals(
                Path.GetFullPath(source),
                Path.GetFullPath(target),
                StringComparison.OrdinalIgnoreCase);
    }

    private static bool pathsEqual(string left, string right)
    {
        return string.Equals(
            Path.GetFullPath(left),
            Path.GetFullPath(right),
            OperatingSystem.IsWindows()
                ? StringComparison.OrdinalIgnoreCase
                : StringComparison.Ordinal);
    }

    private static void copyPath(string source, string target)
    {
        if (Directory.Exists(source))
            copyDirectory(source, target);
        else
            File.Copy(source, target);
    }

    private static string getDuplicatePath(string source)
    {
        string directory = Path.GetDirectoryName(source)!;
        string extension = Path.GetExtension(source);
        string stem = Path.GetFileNameWithoutExtension(source);
        string candidate = Path.Combine(directory, stem + "_copy" + extension);
        int suffix = 2;
        while (File.Exists(candidate) || Directory.Exists(candidate))
            candidate = Path.Combine(directory, stem + "_copy" + suffix++ + extension);
        return candidate;
    }

    private static void copyDirectory(string source, string target)
    {
        Directory.CreateDirectory(target);
        foreach (string file in Directory.GetFiles(source))
            File.Copy(file, Path.Combine(target, Path.GetFileName(file)));
        foreach (string child in Directory.GetDirectories(source))
            copyDirectory(child, Path.Combine(target, Path.GetFileName(child)));
    }

    private static void copyDirectory(
        string source,
        string target,
        ISet<string> skippedFiles)
    {
        Directory.CreateDirectory(target);
        foreach (string file in Directory.GetFiles(source))
        {
            if (!skippedFiles.Contains(Path.GetFullPath(file)))
                File.Copy(file, Path.Combine(target, Path.GetFileName(file)));
        }
        foreach (string child in Directory.GetDirectories(source))
        {
            copyDirectory(
                child,
                Path.Combine(target, Path.GetFileName(child)),
                skippedFiles);
        }
    }
}

public sealed record FileOperationResult(bool Changed, IReadOnlyList<string> Errors)
{
    public static FileOperationResult Empty { get; } = new(false, Array.Empty<string>());
}

public sealed class FileExplorerFilesChangedEventArgs : EventArgs
{
    public FileExplorerFilesChangedEventArgs(
        IReadOnlyList<string> added,
        IReadOnlyList<(string OldPath, string NewPath)> moved,
        IReadOnlyList<string> deleted)
    {
        Added = added;
        Moved = moved;
        Deleted = deleted;
    }

    public IReadOnlyList<string> Added { get; }
    public IReadOnlyList<(string OldPath, string NewPath)> Moved { get; }
    public IReadOnlyList<string> Deleted { get; }
}

public sealed class FileExplorerEntryViewModel : ViewModelBase, IDisposable
{
    private IImage? fallback;
    private ActorPreviewLease? previewLease;
    private IImage? icon;
    private bool disposed;

    public FileExplorerEntryViewModel(string fullPath, bool isDirectory, IImage? icon)
        : this(fullPath, isDirectory, icon, null)
    {
    }

    public FileExplorerEntryViewModel(
        string fullPath,
        bool isDirectory,
        IImage? fallback,
        ActorPreviewLease? previewLease)
    {
        FullPath = fullPath;
        IsDirectory = isDirectory;
        this.fallback = fallback;
        icon = fallback;
        this.previewLease = previewLease;
        Name = Path.GetFileName(fullPath);
        if (previewLease is not null)
        {
            previewLease.FrameChanged += onPreviewFrameChanged;
            updateIcon();
        }
    }

    public string FullPath { get; }
    public bool IsDirectory { get; }
    public IImage? Icon
    {
        get => icon;
        private set => SetProperty(ref icon, value);
    }
    public string Name { get; }

    public bool IsPreviewActive
    {
        get => previewLease?.IsActive == true;
        set
        {
            if (previewLease is not null)
                previewLease.IsActive = value;
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        if (previewLease is not null)
        {
            previewLease.FrameChanged -= onPreviewFrameChanged;
            previewLease.Dispose();
            previewLease = null;
        }
        (fallback as IDisposable)?.Dispose();
        fallback = null;
        icon = null;
    }

    private void onPreviewFrameChanged(object? sender, EventArgs args)
    {
        if (Dispatcher.UIThread.CheckAccess())
        {
            updateIcon();
            return;
        }
        Dispatcher.UIThread.Post(() => updateIcon());
    }

    private void updateIcon()
    {
        if (disposed)
            return;
        IImage? next = previewLease?.Frame ?? fallback ?? icon;
        if (ReferenceEquals(Icon, next))
        {
            OnPropertyChanged(nameof(Icon));
            return;
        }
        Icon = next;
    }
}

public sealed record FileExplorerBreadcrumbViewModel(string Label, string Path, bool ShowSeparator = false);
