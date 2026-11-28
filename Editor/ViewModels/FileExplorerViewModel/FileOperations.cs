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
        if (clipboard.Cut)
            return await moveBatchAsync(clipboard.Paths.ToArray(), target, true, progress);
        List<string> errors = [];
        List<string> externalAdded = [];
        List<string> managedAdded = [];
        FileCopyBatch copies = new(externalAdded, errors, progress);
        foreach (string source in clipboard.Paths.Where(path => pathExists(path) && isEditablePath(path)).ToArray())
        {
            string destination = Path.Combine(target, Path.GetFileName(source));
            if (pathExists(destination))
                destination = getDuplicatePath(source, target);
            bool independentFile = !isVisibleDirectory(source) && !isManagedCopySource(source);
            if (!copies.IsEmpty && (!independentFile || copies.ContainsDestination(destination)))
            {
                await copies.FlushAsync();
                destination = Path.Combine(target, Path.GetFileName(source));
                if (pathExists(destination))
                    destination = getDuplicatePath(source, target);
            }
            (bool copied, string? copyError) = !independentFile
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
                {
                    await copies.FlushAsync();
                    errors.Add(error);
                }
                continue;
            }
            if (independentFile)
            {
                copies.Add(source, destination);
                continue;
            }
            try
            {
                progress?.Report(new EditorOperationProgress("COPY", source));
                await Task.Run(() => copyPath(source, destination));
                externalAdded.Add(destination);
            }
            catch (Exception exception)
            {
                errors.Add($"{Path.GetFileName(source)}: {exception.Message}");
            }
        }
        await copies.FlushAsync();
        Exception? notificationError;
        try
        {
            notificationError = await applyFileChangesAsync(externalAdded, [], [], progress);
        }
        catch (Exception exception)
        {
            errors.Add(exception.Message);
            notificationError = await applyFileChangesAsync(externalAdded, [], [], progress);
        }
        if (notificationError is not null)
            errors.Add(notificationError.Message);
        if (managedAdded.Count != 0)
            changed(new FileExplorerFilesChangedEventArgs(managedAdded, [], []));
        return new FileOperationResult(externalAdded.Count != 0 || managedAdded.Count != 0, errors);
    }

    public async Task<FileOperationResult> DuplicateAsync(IEnumerable<string> paths, IProgress<EditorOperationProgress>? progress = null)
    {
        List<string> errors = [];
        List<string> externalAdded = [];
        List<string> managedAdded = [];
        FileCopyBatch copies = new(externalAdded, errors, progress);
        foreach (string source in normalizeTopLevelPaths(paths)
            .Where(path => isEditablePath(path) && pathExists(path) && !isVisibleDirectory(path)))
        {
            if (EditorPathSandbox.IsSameOrChildPath(gameData.Worlds.MapPathPolicy.MapsRoot, source))
            {
                await copies.FlushAsync();
                errors.Add($"{Path.GetFileName(source)}: {LocaleService.Get("DUPLICATE_FAILED")}");
                continue;
            }
            string destination = getDuplicatePath(source);
            bool managed = isManagedCopySource(source);
            if (!copies.IsEmpty && (managed || copies.ContainsDestination(destination)))
            {
                await copies.FlushAsync();
                destination = getDuplicatePath(source);
            }
            (bool copied, string? copyError) = managed ? await tryCopyManagedPathAsync(source, destination, progress) : (false, null);
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
                copies.Add(source, destination);
            }
            catch (Exception exception)
            {
                await copies.FlushAsync();
                errors.Add($"{Path.GetFileName(source)}: {exception.Message}");
            }
        }
        await copies.FlushAsync();
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

    private sealed class FileCopyBatch(List<string> added, List<string> errors, IProgress<EditorOperationProgress>? progress)
    {
        private readonly List<(string Source, string Destination)> files = [];
        private readonly HashSet<string> destinations = new(StringComparer.OrdinalIgnoreCase);

        internal bool IsEmpty => files.Count == 0;
        internal bool ContainsDestination(string path) => destinations.Contains(destinationKey(path));

        internal void Add(string source, string destination)
        {
            files.Add((source, destination));
            destinations.Add(destinationKey(destination));
        }

        private static string destinationKey(string path) => OperatingSystem.IsMacOS() ? path.Normalize() : path;

        internal async Task FlushAsync()
        {
            if (IsEmpty)
                return;
            string?[] failures = await Task.Run(() => EditorValidationBatch.RunFiles(files, copy =>
            {
                try
                {
                    if (progress is not null)
                        lock (files)
                            progress.Report(new EditorOperationProgress("COPY", copy.Source));
                    File.Copy(copy.Source, copy.Destination);
                    return (string?)null;
                }
                catch (Exception exception)
                {
                    return $"{Path.GetFileName(copy.Source)}: {exception.Message}";
                }
            }));
            for (int index = 0; index < files.Count; index++)
                if (failures[index] is string message)
                    errors.Add(message);
                else
                    added.Add(files[index].Destination);
            files.Clear();
            destinations.Clear();
        }
    }

    public async Task<FileOperationResult> DeleteAsync(IEnumerable<string> paths, IProgress<EditorOperationProgress>? progress = null)
    {
        IReadOnlyList<string> selected = normalizeTopLevelPaths(paths);
        if (gameData.GetBuiltInPathError(selected) is string deletionError)
            return new FileOperationResult(false, [deletionError]);
        string[] normalizedPaths = selected.Where(isEditablePath).ToArray();
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
        string assetsRoot = Path.Combine(projectPath, "Assets");
        HashSet<string> assetPaths = normalizedPaths.Where(path => EditorPathSandbox.IsSameOrChildPath(assetsRoot, path))
            .ToHashSet(gameData.AssetPaths.FileSystemComparer);
        using ProjectResourceDeletionValidation deletionValidation = new(gameData.ProjectPath);
        FileExplorerDeletionValidation validation;
        try
        {
            if (assetPaths.Count != 0)
                gameData.AssetPaths.AssertCanWrite(assetPaths);
            validation = await FileExplorerDeletionValidation.PrepareAsync(gameData, normalizedPaths, assetPaths,
                deletionValidation, progress);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or InvalidOperationException)
        {
            return new FileOperationResult(false, [exception.Message]);
        }
        if (validation.Errors.Count != 0)
            return new FileOperationResult(false, validation.Errors);
        List<string> errors = [];
        List<string> deleted = [];
        List<string> externalDeleted = [];
        normalizedPaths = normalizedPaths.OrderByDescending(assetPaths.Contains).ToArray();
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
                bool asset = assetPaths.Contains(path);
                (bool managed, string? managedError) = asset
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
                if (asset)
                {
                    if (!await validation.TryRecycleAssetAsync(path))
                    {
                        errors.Add(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
                        break;
                    }
                }
                else
                {
                    await SystemTrashService.RunAsync(trash =>
                    {
                        gameData.BuiltInResources.AssertCanChangeOnDisk(path);
                        return trash.MoveToTrash(path);
                    }, default);
                }
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
        if (gameData.GetBuiltInPathError([oldPath]) is string moveError)
            return new FileOperationResult(false, [moveError]);
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
            await Task.Run(() =>
            {
                gameData.BuiltInResources.AssertCanChangeOnDisk(oldPath);
                movePath(oldPath, newPath);
            });
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
}
