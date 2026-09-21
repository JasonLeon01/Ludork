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
        if (IsReadOnly)
            return FileOperationResult.Empty;
        string target = getTargetDirectory(targetDirectory);
        List<string> errors = [];
        List<string> externalAdded = [];
        List<string> managedAdded = [];
        List<(string OldPath, string NewPath)> moved = [];
        List<(string OldPath, string NewPath)> managedMoved = [];
        foreach (string source in clipboardPaths.Where(pathExists).ToArray())
        {
            string destination = Path.Combine(target, Path.GetFileName(source));
            if (clipboardCut && tryMoveManagedPath(source, destination, out string? managedError))
            {
                if (managedError is not null)
                    errors.Add(managedError);
                else
                    managedMoved.Add((source, destination));
                continue;
            }
            if (!clipboardCut && tryCopyManagedPath(source, destination, out string? copyError))
            {
                if (copyError is not null)
                    errors.Add(copyError);
                else
                    managedAdded.Add(destination);
                continue;
            }
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
            if (moved.Count != 0 || managedMoved.Count != 0)
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
        if (managedMoved.Count != 0)
            changed(new FileExplorerFilesChangedEventArgs([], managedMoved, []));
        if (managedAdded.Count != 0)
        {
            changed(new FileExplorerFilesChangedEventArgs(
                managedAdded,
                [],
                []));
        }
        return new FileOperationResult(
            externalAdded.Count != 0 || managedAdded.Count != 0 || moved.Count != 0 || managedMoved.Count != 0,
            errors);
    }

    public FileOperationResult Duplicate(IEnumerable<string> paths)
    {
        if (IsReadOnly)
            return FileOperationResult.Empty;
        List<string> errors = [];
        List<string> externalAdded = [];
        List<string> managedAdded = [];
        foreach (string source in normalizeTopLevelPaths(paths).Where(path => pathExists(path) && !isVisibleDirectory(path)))
        {
            if (isSameOrChildPath(gameData.Worlds.MapPathPolicy.MapsRoot, source))
            {
                errors.Add($"{Path.GetFileName(source)}: {LocaleService.Get("DUPLICATE_FAILED")}");
                continue;
            }
            string destination = getDuplicatePath(source);
            if (tryCopyManagedPath(source, destination, out string? copyError))
            {
                if (copyError is not null)
                    errors.Add(copyError);
                else
                    managedAdded.Add(destination);
                continue;
            }
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
        if (managedAdded.Count != 0)
        {
            changed(new FileExplorerFilesChangedEventArgs(
                managedAdded,
                [],
                []));
        }
        return new FileOperationResult(
            externalAdded.Count != 0 || managedAdded.Count != 0,
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
        if (IsReadOnly)
            return FileOperationResult.Empty;
        string[] normalizedPaths = normalizeTopLevelPaths(paths).ToArray();
        string[] protectedMapPaths = normalizedPaths
            .Where(path => !gameData.Worlds.MapPathPolicy.CanDeletePath(path))
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
        List<string> errors = [];
        List<string> deleted = [];
        List<string> externalDeleted = [];
        foreach (string path in normalizedPaths)
        {
            try
            {
                if (gameData.TryDeleteManagedPath(path, out string? managedError))
                {
                    if (managedError is not null)
                        errors.Add(managedError);
                    else
                        deleted.Add(path);
                    continue;
                }
                if (Directory.Exists(path))
                    Directory.Delete(path, true);
                else if (File.Exists(path))
                    File.Delete(path);
                else
                    continue;
                deleted.Add(path);
                externalDeleted.Add(path);
            }
            catch (Exception exception)
            {
                errors.Add($"{Path.GetFileName(path)}: {exception.Message}");
            }
        }
        if (externalDeleted.Count != 0)
        {
            Exception? notificationError = applyFileChanges([], [], externalDeleted, deleted);
            if (notificationError is not null)
                errors.Add(notificationError.Message);
        }
        else if (deleted.Count != 0)
            changed(new FileExplorerFilesChangedEventArgs([], [], deleted));
        if (!isVisibleDirectory(CurrentPath))
            _ = NavigateToAsync(projectPath);
        return new FileOperationResult(deleted.Count != 0, errors);
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
        if (IsReadOnly)
            return FileOperationResult.Empty;
        string target = getTargetDirectory(targetDirectory);
        if (isSameOrChildPath(gameData.Worlds.MapPathPolicy.MapsRoot, target)
            && !gameData.Worlds.MapPathPolicy.CanCreateDirectory(target))
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
        if (IsReadOnly)
            return FileOperationResult.Empty;
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
        if (tryMoveManagedPath(oldPath, newPath, out string? managedError))
        {
            if (managedError is not null)
                return new FileOperationResult(false, [managedError]);
            changed(new FileExplorerFilesChangedEventArgs([], [(oldPath, newPath)], []));
            SelectedEntry = Entries.FirstOrDefault(entry => pathsEqual(entry.FullPath, newPath));
            return new FileOperationResult(true, []);
        }
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
        if (IsReadOnly)
            return FileOperationResult.Empty;
        string target = getTargetDirectory(targetDirectory);
        List<string> errors = [];
        List<(string OldPath, string NewPath)> moved = [];
        List<(string OldPath, string NewPath)> managedMoved = [];
        foreach (string source in normalizeTopLevelPaths(paths))
        {
            string destination = Path.Combine(target, Path.GetFileName(source));
            if (tryMoveManagedPath(source, destination, out string? managedError))
            {
                if (managedError is not null)
                    errors.Add(managedError);
                else
                    managedMoved.Add((source, destination));
                continue;
            }
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
        if (managedMoved.Count != 0)
            changed(new FileExplorerFilesChangedEventArgs([], managedMoved, []));
        return new FileOperationResult(moved.Count != 0 || managedMoved.Count != 0, errors);
    }

}

