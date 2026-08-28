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

public sealed partial class FileExplorerViewModel
{
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
            if (isSameOrChildPath(gameData.MapPathPolicy.MapsRoot, source))
            {
                errors.Add($"{Path.GetFileName(source)}: {LocaleService.Get("DUPLICATE_FAILED")}");
                continue;
            }
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
        string[] protectedMapPaths = normalizedPaths
            .Where(path => !gameData.MapPathPolicy.CanDeletePath(path))
            .ToArray();
        if (protectedMapPaths.Length != 0)
        {
            return new FileOperationResult(
                false,
                protectedMapPaths
                    .Select(path => $"{Path.GetFileName(path)}: {LocaleService.Get("MAP_DELETE_USE_MAP_LIST")}")
                    .ToArray());
        }
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
        if (isSameOrChildPath(gameData.MapPathPolicy.MapsRoot, target)
            && !gameData.MapPathPolicy.CanCreateDirectory(target))
        {
            return new FileOperationResult(
                false,
                [LocaleService.Get("CREATE_FOLDER_FAILED") + Environment.NewLine + name]);
        }
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

}

