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

public sealed partial class FileExplorerViewModel
{
    public async Task<FileOperationResult> PasteAsync(string? targetDirectory = null, IProgress<EditorOperationProgress>? progress = null)
    {
        string? target = getTargetDirectory(targetDirectory);
        if (target is null)
            return FileOperationResult.Empty;
        List<string> errors = [];
        List<string> externalAdded = [];
        List<string> managedAdded = [];
        List<(string OldPath, string NewPath)> moved = [];
        List<(string OldPath, string NewPath)> managedMoved = [];
        foreach (string source in clipboardPaths.Where(path => pathExists(path) && isEditablePath(path)).ToArray())
        {
            string destination = Path.Combine(target, Path.GetFileName(source));
            if (!clipboardCut && pathExists(destination))
                destination = getDuplicatePath(source, target);
            (bool managed, string? managedError) = clipboardCut
                ? await tryMoveManagedPathAsync(source, destination, progress) : (false, null);
            if (managed)
            {
                if (managedError is not null)
                    errors.Add(managedError);
                else
                    managedMoved.Add((source, destination));
                continue;
            }
            (bool copied, string? copyError) = !clipboardCut
                ? await tryCopyManagedPathAsync(source, destination, progress) : (false, null);
            if (copied)
            {
                if (copyError is not null)
                    errors.Add(copyError);
                else
                    managedAdded.Add(destination);
                continue;
            }
            (bool allowed, string? error) = await canTransferAsync(source, target, destination);
            if (!allowed)
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
                    progress?.Report(new EditorOperationProgress("EDIT_OPERATION_MOVING", source));
                    await Task.Run(() => movePath(source, destination));
                    moved.Add((source, destination));
                }
                else
                {
                    progress?.Report(new EditorOperationProgress("COPY", source));
                    await Task.Run(() => copyPath(source, destination));
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
            Exception? notificationError = await applyFileChangesAsync(externalAdded, moved, [], progress);
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
            await Task.Run(() => rollbackMoves(moved));
            errors.Add(exception.Message);
            moved.Clear();
            if (externalAdded.Count != 0)
            {
                Exception? notificationError = await applyFileChangesAsync(externalAdded, [], [], progress);
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

    public async Task<FileOperationResult> DuplicateAsync(IEnumerable<string> paths, IProgress<EditorOperationProgress>? progress = null)
    {
        List<string> errors = [];
        List<string> externalAdded = [];
        List<string> managedAdded = [];
        foreach (string source in normalizeTopLevelPaths(paths)
            .Where(path => isEditablePath(path) && pathExists(path) && !isVisibleDirectory(path)))
        {
            if (EditorPathSandbox.IsSameOrChildPath(gameData.Worlds.MapPathPolicy.MapsRoot, source))
            {
                errors.Add($"{Path.GetFileName(source)}: {LocaleService.Get("DUPLICATE_FAILED")}");
                continue;
            }
            string destination = getDuplicatePath(source);
            (bool copied, string? copyError) = await tryCopyManagedPathAsync(source, destination, progress);
            if (copied)
            {
                if (copyError is not null)
                    errors.Add(copyError);
                else
                    managedAdded.Add(destination);
                continue;
            }
            try
            {
                gameData.AssetPaths.AssertCanWrite([source, destination]);
                progress?.Report(new EditorOperationProgress("COPY", source));
                await Task.Run(() => File.Copy(source, destination));
                externalAdded.Add(destination);
            }
            catch (Exception exception)
            {
                errors.Add($"{Path.GetFileName(source)}: {exception.Message}");
            }
        }
        Exception? notificationError = await applyFileChangesAsync(externalAdded, [], [], progress);
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

    public async Task<FileOperationResult> DeleteAsync(IEnumerable<string> paths, IProgress<EditorOperationProgress>? progress = null)
    {
        string[] normalizedPaths = normalizeTopLevelPaths(paths).Where(isEditablePath).ToArray();
        if (normalizedPaths.Length == 0)
            return FileOperationResult.Empty;
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
        string[] uiIncoming = await ProjectSnapshotReader.ReadAsync(gameData, store =>
        {
            ReferenceImpact impact = store.ReferenceIndex.GetImpactForPaths(normalizedPaths);
            return impact.Incoming.Where(record => store.ReferenceIndex.GetNode(record.Source)?.Type == "uiAsset"
                    || store.ReferenceIndex.GetNode(record.Target)?.Type == "uiAsset")
                .Select(record => $"{store.ReferenceIndex.GetNode(record.Source)?.Key ?? record.Source} [{record.Kind}] {record.Path}")
                .ToArray();
        }, progress);
        if (uiIncoming.Length != 0)
        {
            string references = string.Join(
                Environment.NewLine,
                uiIncoming);
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
        using ProjectResourceDeletionValidation deletionValidation = new(gameData.ProjectPath);
        for (int index = 0; index < normalizedPaths.Length; index++)
        {
            string path = normalizedPaths[index];
            try
            {
                gameData.AssetPaths.AssertCanWrite([path]);
                if (gameData.IsBlueprintResourcePath(path))
                {
                    int end = index + 1;
                    while (end < normalizedPaths.Length && gameData.IsBlueprintResourcePath(normalizedPaths[end]))
                        end++;
                    BlueprintDeletionBatchResult batch = await gameData.DeleteBlueprintResourcesAsync(normalizedPaths[index..end], progress);
                    index = end - 1;
                    foreach (BlueprintDeletionBatchResult.Item item in batch.Items)
                    {
                        if (item.Deleted)
                            deleted.Add(item.Path);
                        if (item.Error is not null)
                            errors.Add(item.Error);
                    }
                    if (batch.Aborted)
                    {
                        errors.Add(batch.Error!);
                        break;
                    }
                    continue;
                }
                (bool managed, string? managedError) = EditorPathSandbox.IsSameOrChildPath(Path.Combine(projectPath, "Assets"), path)
                    ? (false, null) : await gameData.TryDeleteManagedPathAsync(path, deletionValidation, progress);
                if (managed)
                {
                    if (managedError is not null)
                        errors.Add(managedError);
                    else
                        deleted.Add(path);
                    continue;
                }
                progress?.Report(new EditorOperationProgress("EDIT_OPERATION_DELETING", path));
                bool removed = await Task.Run(() =>
                {
                    if (Directory.Exists(path))
                        Directory.Delete(path, true);
                    else if (File.Exists(path))
                        File.Delete(path);
                    else
                        return false;
                    return true;
                });
                if (!removed)
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
            Exception? notificationError = await applyFileChangesAsync([], [], externalDeleted, progress, deleted);
            if (notificationError is not null)
                errors.Add(notificationError.Message);
        }
        else if (deleted.Count != 0)
            changed(new FileExplorerFilesChangedEventArgs([], [], deleted));
        if (!isVisibleDirectory(CurrentPath))
            await NavigateToAsync(CurrentRoot.Path);
        return new FileOperationResult(deleted.Count != 0, errors);
    }

    public FileOperationResult CreateDirectory(string name, string? targetDirectory = null)
    {
        if (IsReadOnly)
            return FileOperationResult.Empty;
        string? target = getTargetDirectory(targetDirectory);
        if (target is null)
            return FileOperationResult.Empty;
        if (EditorPathSandbox.IsSameOrChildPath(gameData.Worlds.MapPathPolicy.MapsRoot, target)
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
            gameData.AssetPaths.AssertCanWrite([newPath]);
            Directory.CreateDirectory(newPath);
            changed(new FileExplorerFilesChangedEventArgs([newPath], [], []));
            return new FileOperationResult(true, []);
        }
        catch (Exception exception)
        {
            return new FileOperationResult(false, [$"{name}: {exception.Message}"]);
        }
    }

    public async Task<FileOperationResult> RenameSelectedAsync(string newName, IProgress<EditorOperationProgress>? progress = null)
    {
        if (SelectedEntry is null)
            return FileOperationResult.Empty;
        string oldPath = SelectedEntry.FullPath;
        if (!isEditablePath(oldPath))
            return FileOperationResult.Empty;
        string directory = Path.GetDirectoryName(oldPath)!;
        string trimmedName = newName.Trim();
        if (!tryGetChildPath(directory, trimmedName, out string newPath))
            return new FileOperationResult(false, [LocaleService.Get("RENAME_FAILED") + Environment.NewLine + newName]);
        if (string.Equals(oldPath, newPath, StringComparison.Ordinal))
            return FileOperationResult.Empty;
        bool caseOnlyRename = isCaseOnlyPathChange(oldPath, newPath);
        if (!caseOnlyRename && pathExists(newPath))
            return new FileOperationResult(false, [LocaleService.Get("FILE_ALREADY_EXISTS").Replace("{name}", trimmedName)]);
        (bool managed, string? managedError) = await tryMoveManagedPathAsync(oldPath, newPath, progress);
        if (managed)
        {
            if (managedError is not null)
                return new FileOperationResult(false, [managedError]);
            changed(new FileExplorerFilesChangedEventArgs([], [(oldPath, newPath)], []));
            SelectedEntry = Entries.FirstOrDefault(entry => pathsEqual(entry.FullPath, newPath));
            return new FileOperationResult(true, []);
        }
        (bool allowed, string? transferError) = await canTransferAsync(oldPath, directory, newPath);
        if (!allowed)
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
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_MOVING", oldPath));
            await Task.Run(() => movePath(oldPath, newPath));
            Exception? notificationError = await applyFileChangesAsync([], [(oldPath, newPath)], [], progress);
            SelectedEntry = Entries.FirstOrDefault(entry => string.Equals(entry.FullPath, newPath, StringComparison.OrdinalIgnoreCase));
            return new FileOperationResult(
                true,
                notificationError is null ? [] : [notificationError.Message]);
        }
        catch (Exception exception)
        {
            await Task.Run(() => rollbackMoves([(oldPath, newPath)]));
            return new FileOperationResult(false, [$"{trimmedName}: {exception.Message}"]);
        }
    }

    public async Task<FileOperationResult> MoveAsync(IEnumerable<string> paths, string targetDirectory, IProgress<EditorOperationProgress>? progress = null)
    {
        string? target = getTargetDirectory(targetDirectory);
        if (target is null)
            return FileOperationResult.Empty;
        List<string> errors = [];
        List<(string OldPath, string NewPath)> moved = [];
        List<(string OldPath, string NewPath)> managedMoved = [];
        foreach (string source in normalizeTopLevelPaths(paths).Where(isEditablePath))
        {
            string destination = Path.Combine(target, Path.GetFileName(source));
            (bool managed, string? managedError) = await tryMoveManagedPathAsync(source, destination, progress);
            if (managed)
            {
                if (managedError is not null)
                    errors.Add(managedError);
                else
                    managedMoved.Add((source, destination));
                continue;
            }
            (bool allowed, string? error) = await canTransferAsync(source, target, destination);
            if (!allowed)
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
                progress?.Report(new EditorOperationProgress("EDIT_OPERATION_MOVING", source));
                await Task.Run(() => movePath(source, destination));
                moved.Add((source, destination));
            }
            catch (Exception exception)
            {
                errors.Add($"{Path.GetFileName(source)}: {exception.Message}");
            }
        }
        try
        {
            Exception? notificationError = await applyFileChangesAsync([], moved, [], progress);
            if (notificationError is not null)
                errors.Add(notificationError.Message);
        }
        catch (Exception exception)
        {
            await Task.Run(() => rollbackMoves(moved));
            errors.Add(exception.Message);
            moved.Clear();
        }
        if (managedMoved.Count != 0)
            changed(new FileExplorerFilesChangedEventArgs([], managedMoved, []));
        return new FileOperationResult(moved.Count != 0 || managedMoved.Count != 0, errors);
    }

}
