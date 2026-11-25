using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.ViewModels;

public sealed partial class FileExplorerViewModel
{
    public Task<FileOperationResult> MoveAsync(IEnumerable<string> paths, string targetDirectory,
        IProgress<EditorOperationProgress>? progress = null)
        => moveBatchAsync(paths, targetDirectory, false, progress);

    private async Task<FileOperationResult> moveBatchAsync(IEnumerable<string> paths, string targetDirectory,
        bool cut, IProgress<EditorOperationProgress>? progress)
    {
        string? target = getTargetDirectory(targetDirectory);
        if (target is null || IsReadOnly)
            return FileOperationResult.Empty;
        string[] sources = normalizeTopLevelPaths(paths).ToArray();
        List<(string OldPath, string NewPath)> assets = [];
        List<(string OldPath, string NewPath)> resources = [];
        List<(string OldPath, string NewPath)> files = [];
        List<string> errors = [];
        HashSet<string> duplicates = sources.GroupBy(path => OperatingSystem.IsMacOS()
                ? Path.GetFileName(path).Normalize() : Path.GetFileName(path), gameData.AssetPaths.FileSystemComparer)
            .Where(group => group.Count() > 1).SelectMany(group => group).ToHashSet(StringComparer.Ordinal);
        string assetRoot = Path.Combine(projectPath, "Assets");
        foreach (string source in sources)
        {
            string destination = Path.Combine(target, Path.GetFileName(source));
            if (!isEditablePath(source) || !pathExists(source) || EditorPathSandbox.IsSameOrChildPath(source, target))
            {
                errors.Add($"{source}: {LocaleService.Get("MOVE_FILE_FAILED")}");
                continue;
            }
            bool duplicate = duplicates.Contains(source);
            if (!duplicate && pathsEqual(source, destination))
                continue;
            bool asset = EditorPathSandbox.IsSameOrChildPath(assetRoot, source);
            if (duplicate || isMoveDestinationOccupied(source, destination, asset))
            {
                errors.Add($"{source}: {LocaleService.Get("FILE_ALREADY_EXISTS").Replace("{name}", destination)}");
                continue;
            }
            if (asset)
            {
                if (!EditorPathSandbox.IsSameOrChildPath(assetRoot, target))
                    errors.Add($"{source}: {LocaleService.Get("MOVE_FILE_FAILED")}");
                else
                    assets.Add((source, destination));
            }
            else if (gameData.IsManagedPath(source))
                resources.Add((source, destination));
            else
            {
                (bool allowed, string? error) = await canTransferAsync(source, target, destination);
                if (allowed)
                    files.Add((source, destination));
                else
                    errors.Add(error ?? $"{source}: {LocaleService.Get("MOVE_FILE_FAILED")}");
            }
        }
        (string OldPath, string NewPath)[] moves = [.. assets, .. resources, .. files];
        if (errors.Count != 0 || moves.Length == 0)
            return new FileOperationResult(false, errors);
        List<(string OldPath, string NewPath)> transferred = [];
        bool committed = false;
        try
        {
            changing(new FileExplorerFilesChangedEventArgs([], moves, []));
            if (files.Count != 0)
            {
                if (resources.Count != 0 || assets.Count != 0)
                    await ProjectSnapshotReader.ReadAsync(gameData, store => store.MoveExplorerResources(assets, resources), progress);
                await Task.Run(() =>
                {
                    foreach ((string source, string destination) in files)
                    {
                        progress?.Report(new EditorOperationProgress("EDIT_OPERATION_MOVING", source));
                        movePath(source, destination);
                        transferred.Add((source, destination));
                    }
                });
            }
            Exception? notification = await gameData.ApplyExplorerMovesAsync(assets, resources, files, progress);
            committed = true;
            if (notification is not null)
                errors.Add(notification.Message);
        }
        catch (Exception exception)
        {
            errors.Add(exception.Message);
            bool rollbackFailed = false;
            await Task.Run(() =>
            {
                for (int index = transferred.Count - 1; index >= 0; index--)
                {
                    (string source, string destination) = transferred[index];
                    try
                    {
                        movePath(destination, source);
                        transferred.RemoveAt(index);
                    }
                    catch (Exception rollbackError)
                    {
                        rollbackFailed = true;
                        if (Path.Exists(destination))
                            errors.Add(LocaleService.Get("FILE_EXPLORER_MOVE_ROLLBACK_FAILED").Replace("{target}", destination)
                                .Replace("{source}", source).Replace("{error}", rollbackError.Message));
                        else
                        {
                            errors.Add($"{destination} → {source}: {rollbackError.Message}");
                            transferred.RemoveAt(index);
                        }
                    }
                }
            });
            if (cut && rollbackFailed)
            {
                clipboardPaths.RemoveAll(path => !pathExists(path) || transferred.Any(move => pathsEqual(path, move.OldPath)));
                clipboardCut = clipboardPaths.Count != 0;
            }
            if (transferred.Count == 0)
                return new FileOperationResult(false, errors);
            try
            {
                Exception? notification = await gameData.ApplyExternalFileChangesAsync([], transferred, [], progress);
                if (notification is not null)
                    errors.Add(notification.Message);
            }
            catch (Exception syncError)
            {
                errors.Add(syncError.Message);
            }
            moves = transferred.ToArray();
        }
        if (cut && committed)
        {
            clipboardPaths.Clear();
            clipboardCut = false;
        }
        try
        {
            changed(new FileExplorerFilesChangedEventArgs([], moves, []));
        }
        catch (Exception notification)
        {
            errors.Add(notification.Message);
        }
        return new FileOperationResult(true, errors);
    }

    private bool isMoveDestinationOccupied(string source, string destination, bool asset)
    {
        bool visible = pathExists(destination);
        if (!visible && !Path.Exists(destination))
            return false;
        if (asset)
            return visible || !gameData.AssetPaths.FileSystemComparer.Equals(gameData.AssetPaths.ResolveReadPath(source), destination);
        EditorDocument? document = gameData.GetDocumentByPath(source);
        string? saved = document?.Section == "WorldMaps" ? Path.GetDirectoryName(document.SavedPath) : document?.SavedPath;
        return saved is null || !gameData.AssetPaths.FileSystemComparer.Equals(saved, destination);
    }
}
