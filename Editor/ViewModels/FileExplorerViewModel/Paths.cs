using System.Text.Json.Nodes;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using Ludork.Models;
using Ludork.Plugin.Abstractions;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace Ludork.ViewModels;

public sealed partial class FileExplorerViewModel
{
    private void changed(FileExplorerFilesChangedEventArgs changes)
    {
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

    private async Task<Exception?> applyFileChangesAsync(
        IReadOnlyList<string> added,
        IReadOnlyList<(string OldPath, string NewPath)> moved,
        IReadOnlyList<string> deleted,
        IProgress<EditorOperationProgress>? progress,
        IReadOnlyList<string>? changedDeleted = null)
    {
        if (added.Count == 0 && moved.Count == 0 && deleted.Count == 0)
            return null;
        Exception? notificationError = await gameData.ApplyExternalFileChangesAsync(added, moved, deleted, progress);
        try
        {
            changed(new FileExplorerFilesChangedEventArgs(
                added.ToArray(),
                moved.ToArray(),
                changedDeleted?.ToArray() ?? deleted.ToArray()));
            return notificationError;
        }
        catch (Exception exception)
        {
            return notificationError is null ? exception : new AggregateException(notificationError, exception);
        }
    }

    private void refreshBreadcrumbs()
    {
        BreadcrumbItems.Clear();
        FileExplorerRootViewModel root = CurrentRoot;
        BreadcrumbItems.Add(new FileExplorerBreadcrumbViewModel(root.Label, root.Path));
        string relative = Path.GetRelativePath(root.Path, CurrentPath);
        if (relative is "." or "")
            return;
        string current = root.Path;
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
        return getContentRoot(path) is not null;
    }

    private FileExplorerRootViewModel? getContentRoot(string path)
    {
        return ContentRoots.FirstOrDefault(root => EditorPathSandbox.IsSameOrChildPath(root.Path, path));
    }

    private bool isContentRoot(string path)
    {
        string fullPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
        return ContentRoots.Any(root => PathComparer.Equals(root.Path, fullPath));
    }

    private bool isEditablePath(string path)
    {
        return isUnderRoot(path) && !isContentRoot(path);
    }

    private string? getTargetDirectory(string? targetDirectory)
    {
        string target = string.IsNullOrWhiteSpace(targetDirectory) ? CurrentPath : Path.GetFullPath(targetDirectory);
        return isUnderRoot(target) && isVisibleDirectory(target) ? target : null;
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

    private async Task<(bool Allowed, string? Error)> canTransferAsync(string source, string targetDirectory, string destination)
    {
        if (!isEditablePath(source) || !isUnderRoot(targetDirectory))
            return (false, null);
        if (string.Equals(Path.GetFullPath(source), Path.GetFullPath(destination), StringComparison.Ordinal))
            return (false, null);
        try
        {
            gameData.AssetPaths.AssertCanWrite([source, destination]);
        }
        catch (InvalidOperationException exception)
        {
            return (false, exception.Message);
        }
        string project = projectPath;
        (bool exists, bool directory, bool destinationExists, bool mapAllowed) = await Task.Run(() =>
        {
            bool sourceDirectory = Directory.Exists(source);
            return (File.Exists(source) || sourceDirectory, sourceDirectory,
                File.Exists(destination) || Directory.Exists(destination),
                new WorldMapPathPolicy(project).CanTransferPath(source, destination));
        });
        if (!exists)
            return (false, null);
        if (directory && EditorPathSandbox.IsSameOrChildPath(source, targetDirectory))
            return (false, $"{Path.GetFileName(source)}: {LocaleService.Get("MOVE_FILE_FAILED")}");
        if (!isCaseOnlyPathChange(source, destination) && destinationExists
            || gameData.ContainsDataPath(destination, directory))
            return (false, LocaleService.Get("FILE_ALREADY_EXISTS").Replace("{name}", Path.GetFileName(destination)));
        if (!mapAllowed)
            return (false, $"{Path.GetFileName(source)}: {LocaleService.Get("MOVE_FILE_FAILED")}");
        if (!directory && !Path.GetExtension(source).Equals(DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase))
            return (true, null);
        ProjectOperationSnapshot snapshot = await gameData.CaptureOperationSnapshotAsync();
        IReadOnlyList<(string SourcePath, string DestinationPath)> uiAssetMoves;
        try
        {
            uiAssetMoves = await Task.Run(() =>
            {
                using ProjectDataStore captured = snapshot.CreateStore();
                return getUiAssetTransferPaths(captured, source, destination);
            });
        }
        catch (Exception exception) when (EditorPathSandbox.IsPathFailure(exception))
        {
            return (false, exception.Message);
        }
        if (gameData.Documents.Revision != snapshot.Revision)
            throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        if (uiAssetMoves.Count != 0)
        {
            string uiAssetsRoot = Path.Combine(projectPath, "Data", "UI");
            if (!EditorPathSandbox.IsSameOrChildPath(uiAssetsRoot, source)
                || !EditorPathSandbox.IsSameOrChildPath(uiAssetsRoot, destination)
                || uiAssetMoves.Any(move =>
                    !tryGetUiAssetKeyForPath(move.SourcePath, out _)
                    || !tryGetUiAssetKeyForPath(move.DestinationPath, out _)))
            {
                return (false, $"{Path.GetFileName(source)}: {LocaleService.Get("MOVE_FILE_FAILED")}");
            }
        }
        return (true, null);
    }

    private bool tryGetUiAssetKeyForPath(string path, out string key)
    {
        string root = Path.GetFullPath(Path.Combine(projectPath, "Data", "UI"));
        string relative = Path.GetRelativePath(root, Path.GetFullPath(path));
        key = string.Empty;
        if (!EditorPathSandbox.IsSameOrChildPath(root, path) || !string.Equals(Path.GetExtension(relative), DataConfig.DataFileExtension, StringComparison.Ordinal))
            return false;
        key = UiAssetSchema.NormalizeAssetKey(Path.ChangeExtension(relative, null)!.Replace('\\', '/'));
        return key.Length != 0 && string.Equals(
            key.Replace('/', Path.DirectorySeparatorChar) + DataConfig.DataFileExtension, relative, StringComparison.Ordinal);
    }

    private static IReadOnlyList<(string SourcePath, string DestinationPath)> getUiAssetTransferPaths(
        ProjectDataStore captured,
        string source,
        string destination)
    {
        string uiAssetsRoot = Path.Combine(captured.ProjectPath, "Data", "UI");
        bool sourceIsFile = File.Exists(source);
        List<(string SourcePath, string DestinationPath)> result = [];
        foreach (string dataKey in captured.UiAssets.GetUiAssetKeysForMove())
        {
            string logicalKey = UiAssetSchema.NormalizeAssetKey(dataKey);
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
            if (!Directory.Exists(source) || !EditorPathSandbox.IsSameOrChildPath(source, assetPath))
                continue;
            string relative = Path.GetRelativePath(source, assetPath);
            result.Add((assetPath, Path.Combine(destination, relative)));
        }
        IEnumerable<string> diskFiles = sourceIsFile
            ? [source]
            : Directory.Exists(source)
                ? enumerateDirectoryForTransfer(source).OfType<FileInfo>().Select(file => file.FullName)
                : [];
        HashSet<string> known = result.Select(move => move.SourcePath).ToHashSet(ProjectDiskSnapshot.PathComparer);
        string[] candidates = diskFiles.Where(path => !known.Contains(path)
            && Path.GetExtension(path).Equals(DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase)).ToArray();
        Func<DataFileInfo?>[] reads = candidates.Select(captured.prepareDataFileRead).ToArray();
        bool[] uiAssets = EditorValidationBatch.RunFiles(reads, read => read()?.Type == UiAssetSchema.UiAssetType);
        for (int index = 0; index < candidates.Length; index++)
        {
            if (!uiAssets[index])
                continue;
            string diskFile = candidates[index];
            string destinationPath = sourceIsFile
                ? destination
                : Path.Combine(destination, Path.GetRelativePath(source, diskFile));
            result.Add((diskFile, destinationPath));
        }
        return result;
    }

    private IReadOnlyList<string> normalizeTopLevelPaths(IEnumerable<string> paths)
    {
        string[] selected = paths
            .Where(path => !string.IsNullOrWhiteSpace(path))
            .Select(path => Path.TrimEndingDirectorySeparator(Path.GetFullPath(path)))
            .Distinct(gameData.AssetPaths.FileSystemComparer)
            .OrderBy(path => path.Length)
            .ToArray();
        HashSet<string> selectedPaths = selected.ToHashSet(gameData.AssetPaths.FileSystemComparer);
        return selected.Where(path =>
        {
            for (string? parent = Path.GetDirectoryName(path); parent is not null; parent = Path.GetDirectoryName(parent))
                if (selectedPaths.Contains(parent))
                    return false;
            return true;
        }).ToArray();
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
                moveDirectory(source, temporary);
                try
                {
                    moveDirectory(temporary, target);
                }
                catch
                {
                    moveDirectory(temporary, source);
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
            moveDirectory(source, target);
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

    private string getDuplicatePath(string source, string? targetDirectory = null)
    {
        string directory = targetDirectory ?? Path.GetDirectoryName(source)!;
        bool directorySource = isVisibleDirectory(source);
        string extension = directorySource ? string.Empty : Path.GetExtension(source);
        string stem = directorySource ? Path.GetFileName(source) : Path.GetFileNameWithoutExtension(source);
        string candidate = Path.Combine(directory, stem + "_copy" + extension);
        int suffix = 2;
        while (pathExists(candidate))
            candidate = Path.Combine(directory, stem + "_copy" + suffix++ + extension);
        return candidate;
    }

    public IReadOnlyList<string> GetSiblingNames(FileExplorerEntryViewModel entry)
    {
        string directory = Path.GetDirectoryName(entry.FullPath)!;
        HashSet<string> paths = gameData.AssetPaths.DirectoryExists(directory)
            ? gameData.AssetPaths.EnumerateEntries(directory).Select(item => item.Path).ToHashSet(PathComparer)
            : new HashSet<string>(PathComparer);
        foreach (EditorDocument document in gameData.Documents.All)
        {
            if (!document.Exists)
                continue;
            foreach (string path in gameData.Documents.GetPaths(document))
            {
                if (!EditorPathSandbox.IsSameOrChildPath(directory, path))
                    continue;
                string relative = Path.GetRelativePath(directory, path);
                string first = relative.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)[0];
                if (first is not ("" or "."))
                    paths.Add(Path.Combine(directory, first));
            }
        }
        paths.Remove(entry.FullPath);
        return paths.Select(path => Path.GetFileName(path)).ToArray();
    }

    private static void copyDirectory(string source, string target)
    {
        string staging = Path.Combine(Path.GetDirectoryName(target)!, $".ludork-copy-{Guid.NewGuid():N}");
        List<(string Source, string Target)> files = [];
        Directory.CreateDirectory(staging);
        try
        {
            foreach (FileSystemInfo entry in enumerateDirectoryForTransfer(source))
            {
                string destination = Path.Combine(staging, Path.GetRelativePath(source, entry.FullName));
                if (entry is DirectoryInfo)
                    Directory.CreateDirectory(destination);
                else
                    files.Add((entry.FullName, destination));
            }
            EditorValidationBatch.ForEachFile(files, file => File.Copy(file.Source, file.Target));
            moveDirectory(staging, target);
        }
        catch
        {
            EditorValidationBatch.ForEachFile(files, file => FilePersistence.DeleteFile(file.Target));
            Directory.Delete(staging, true);
            throw;
        }
    }

    private static IEnumerable<FileSystemInfo> enumerateDirectoryForTransfer(string path)
    {
        Stack<FileSystemInfo> pending = new();
        pending.Push(new DirectoryInfo(path));
        while (pending.TryPop(out FileSystemInfo? entry))
        {
            if (EditorPathSandbox.IsLink(entry))
                throw new IOException($"The asset directory contains a symbolic link: {entry.FullName}");
            yield return entry;
            if (entry is DirectoryInfo directory)
                foreach (FileSystemInfo child in directory.EnumerateFileSystemInfos())
                    pending.Push(child);
        }
    }

    private static void moveDirectory(string source, string target)
    {
        if (!OperatingSystem.IsMacOS())
        {
            Directory.Move(source, target);
            return;
        }
        const uint renameExclusive = 0x00000004;
        if (renameDirectoryOnMacOS(source, target, renameExclusive) != 0)
            throw new IOException($"{target}: {Marshal.GetLastPInvokeErrorMessage()}");
    }

    [DllImport("/usr/lib/libSystem.B.dylib", EntryPoint = "renamex_np", CharSet = CharSet.Ansi, SetLastError = true)]
    private static extern int renameDirectoryOnMacOS(string source, string destination, uint flags);
}
