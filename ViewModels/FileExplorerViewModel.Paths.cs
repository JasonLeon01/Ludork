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
        IReadOnlyDictionary<string, string> mapReplacements =
            referenceIndex.CreateMapMoveReplacements(moved);
        gameData.ApplyExternalFileChanges(added, moved, deleted);
        referenceIndex.RewriteMapReferences(mapReplacements, false);
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
        if (!gameData.MapPathPolicy.CanTransferPath(source, destination))
        {
            error = $"{Path.GetFileName(source)}: {LocaleService.Get("MOVE_FILE_FAILED")}";
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

