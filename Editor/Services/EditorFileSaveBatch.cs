using Ludork.Plugin.Abstractions;
using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security;
using System.Threading;

namespace Ludork.Services;

internal sealed class EditorFileSaveBatch
{
    internal static event Action<IReadOnlyDictionary<string, string?>>? FilesCommitted;
    private static readonly object failedPath = new();
    private static readonly StringComparer pathComparer = OperatingSystem.IsWindows()
        ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
    private readonly Dictionary<string, byte[]> writes = new(pathComparer);
    private readonly Dictionary<string, string> fileWrites = new(pathComparer);
    private readonly HashSet<string> deletions = new(pathComparer);
    private string? recyclePath;
    private readonly List<(string Source, string Destination)> moves = [];
    private readonly List<(string Source, string Destination)> fileMoves = [];
    private readonly HashSet<string> directories = new(StringComparer.Ordinal);
    private readonly HashSet<string> emptyDirectoryDeletions = new(StringComparer.Ordinal);
    private readonly HashSet<string> directorySpellings = new(StringComparer.Ordinal);

    public void Write(string path, byte[] content)
    {
        string fullPath = Path.GetFullPath(path);
        fileWrites.Remove(fullPath);
        writes[fullPath] = content;
    }

    public void WriteFromFile(string path, string source)
    {
        string fullPath = Path.GetFullPath(path);
        writes.Remove(fullPath);
        fileWrites[fullPath] = Path.GetFullPath(source);
    }
    public void Delete(string path) => deletions.Add(Path.GetFullPath(path));
    public void Recycle(string path)
    {
        string fullPath = Path.GetFullPath(path);
        if (recyclePath is not null && !pathComparer.Equals(recyclePath, fullPath))
            throw new InvalidOperationException("A resource deletion can recycle only one file or directory.");
        recyclePath = fullPath;
    }
    public void EnsureDirectoryCase(string path) => directorySpellings.Add(Path.GetFullPath(path));
    public void EnsureDirectory(string path) => directories.Add(Path.GetFullPath(path));
    public void DeleteEmptyDirectory(string path) => emptyDirectoryDeletions.Add(Path.GetFullPath(path));

    public void MoveFile(string source, string destination)
    {
        if (!string.Equals(source, destination, StringComparison.Ordinal))
            fileMoves.Add((Path.GetFullPath(source), Path.GetFullPath(destination)));
    }

    public void MoveDirectory(string source, string destination)
    {
        if (!string.Equals(source, destination, StringComparison.Ordinal))
            moves.Add((Path.GetFullPath(source), Path.GetFullPath(destination)));
    }

    public SaveResult Execute(IProgress<EditorOperationProgress>? progress = null, CancellationToken cancellationToken = default,
        Func<bool>? canWrite = null)
    {
        string staging = Path.Combine(Path.GetTempPath(), "LudorkSave-" + Guid.NewGuid().ToString("N"));
        Dictionary<string, string> prepared = new(pathComparer);
        Dictionary<string, string?> originals = new(pathComparer);
        string[] writePaths = writes.Keys.Concat(fileWrites.Keys).ToArray();
        string[] affectedPaths = writePaths.Concat(deletions).Distinct(pathComparer).ToArray();
        Dictionary<string, string> originalPaths = new(pathComparer);
        HashSet<string> replacedPaths = new(pathComparer);
        List<string> changedFiles = [];
        List<(string Source, string Destination)> completedMoves = [];
        List<(string Source, string Destination)> completedFileMoves = [];
        List<string> removedEmptyDirectories = [];
        List<string> createdDirectories = [];
        List<string> errors = [];
        Dictionary<string, string?> fingerprints = new(pathComparer);
        Dictionary<string, string?> expectedFiles = new(pathComparer);
        Dictionary<string, string> recycleInputs = new(pathComparer);
        string currentPath = staging;
        bool committed = false;
        bool rollbackFailed = false;
        bool cancelled = false;
        int completed = 0;
        int total = writePaths.Length * 2 + (moves.Count + fileMoves.Count) * 2
            + deletions.Count + directories.Count + emptyDirectoryDeletions.Count + (recyclePath is null ? 0 : 1);
        void report(string path)
        {
            cancellationToken.ThrowIfCancellationRequested();
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_WRITING", path, completed++, total));
        }
        try
        {
            if (recyclePath is not null)
            {
                currentPath = recyclePath;
                recycleInputs = captureRecycleInputs(recyclePath, cancellationToken);
            }
            Directory.CreateDirectory(staging);
            string[] temporaryPaths = prepareFiles(writePaths, (path, index) =>
            {
                lock (prepared)
                    report(path);
                string temporary = Path.Combine(staging, "write-" + index);
                if (writes.TryGetValue(path, out byte[]? bytes))
                    File.WriteAllBytes(temporary, bytes);
                else
                    File.Copy(fileWrites[path], temporary);
                return temporary;
            }, cancellationToken);
            for (int index = 0; index < writePaths.Length; index++)
                prepared.Add(writePaths[index], temporaryPaths[index]);
            Dictionary<string, string?> expectedMoveFiles = ProjectDiskSnapshot.ReadFingerprints(
                fileMoves.Select(move => move.Source), cancellationToken);
            if (FilesCommitted is not null || canWrite is not null)
            {
                foreach ((string source, _) in fileMoves)
                    fingerprints[source] = null;
                foreach ((string source, string destination) in fileMoves)
                    fingerprints[destination] = expectedMoveFiles[source];
                (string Source, string Destination)[] movedFiles = moves.Where(move => Directory.Exists(move.Source))
                    .SelectMany(move => Directory.EnumerateFiles(move.Source, "*", SearchOption.AllDirectories)
                        .Select(file => (file, Path.Combine(move.Destination, Path.GetRelativePath(move.Source, file))))).ToArray();
                Dictionary<string, string?> movedHashes = ProjectDiskSnapshot.ReadFingerprints(
                    movedFiles.Select(file => file.Source), cancellationToken);
                foreach ((string source, string destination) in movedFiles)
                {
                    fingerprints[source] = null;
                    fingerprints[destination] = movedHashes[source];
                }
                foreach (KeyValuePair<string, string> entry in recycleInputs.Where(entry => entry.Value != "directory"))
                    fingerprints[entry.Key] = null;
                foreach (string path in deletions)
                    fingerprints[path] = null;
                Dictionary<string, string?> preparedHashes = ProjectDiskSnapshot.ReadFingerprints(prepared.Values, cancellationToken);
                foreach ((string path, string temporary) in prepared)
                    fingerprints[path] = preparedHashes[temporary];
            }
            if (canWrite is not null)
            {
                string?[] hashes = EditorValidationBatch.RunFiles(affectedPaths, path =>
                {
                    string sourcePath = path;
                    foreach ((string source, string destination) in moves)
                        if (ProjectDiskSnapshot.IsWithin(path, destination))
                        {
                            sourcePath = Path.Combine(source, Path.GetRelativePath(destination, path));
                            break;
                        }
                    return ProjectDiskSnapshot.ReadFingerprint(sourcePath);
                }, cancellationToken);
                for (int index = 0; index < affectedPaths.Length; index++)
                    expectedFiles.Add(affectedPaths[index], hashes[index]);
            }
            if (canWrite is not null && !canWrite())
                throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            foreach (string directory in directorySpellings.OrderBy(path => path.Length))
            {
                cancellationToken.ThrowIfCancellationRequested();
                string parent = Path.GetDirectoryName(directory)!;
                if (!Directory.Exists(parent))
                    continue;
                string name = Path.GetFileName(directory);
                string? existing = Directory.EnumerateDirectories(parent).FirstOrDefault(path =>
                    Path.GetFileName(path).Equals(name, StringComparison.OrdinalIgnoreCase));
                if (existing is null || Path.GetFileName(existing) == name)
                    continue;
                currentPath = directory;
                string intermediate = createSiblingPath(existing);
                moveDirectory(existing, intermediate, completedMoves);
                moveDirectory(intermediate, directory, completedMoves);
            }
            (string Source, string Destination)[] existingMoves = moves
                .Where(move => Directory.Exists(move.Source)).ToArray();
            HashSet<string> moveSources = existingMoves.Select(move => move.Source).ToHashSet(pathComparer);
            HashSet<string> moveDestinations = new(pathComparer);
            if (moveSources.Count != existingMoves.Length)
                throw new IOException("A save directory cannot be moved more than once.");
            foreach ((string source, string destination) in existingMoves)
            {
                currentPath = destination;
                if (!moveDestinations.Add(destination))
                    throw new IOException($"More than one save directory targets: {destination}");
                if ((Directory.Exists(destination) || File.Exists(destination))
                    && !moveSources.Contains(destination)
                    && !FileSystemPathIdentity.RefersToSameEntry(source, destination))
                    throw new IOException($"The save destination already exists: {destination}");
            }
            List<(string Intermediate, string Destination)> stagedMoves = [];
            foreach ((string source, string destination) in existingMoves)
            {
                currentPath = source;
                report(currentPath);
                string intermediate = createSiblingPath(source);
                moveDirectory(source, intermediate, completedMoves);
                stagedMoves.Add((intermediate, destination));
            }
            foreach ((string intermediate, string destination) in stagedMoves)
            {
                currentPath = destination;
                report(currentPath);
                ensureDirectory(Path.GetDirectoryName(destination)!, createdDirectories);
                moveDirectory(intermediate, destination, completedMoves);
            }
            HashSet<string> fileMoveSources = fileMoves.Select(move => move.Source).ToHashSet(pathComparer);
            HashSet<string> fileMoveDestinations = new(pathComparer);
            if (fileMoveSources.Count != fileMoves.Count)
                throw new IOException("A save file cannot be moved more than once.");
            foreach ((string source, string destination) in fileMoves)
            {
                currentPath = source;
                if (!File.Exists(source))
                    throw new FileNotFoundException("The moved asset no longer exists.", source);
                if (!fileMoveDestinations.Add(destination))
                    throw new IOException($"More than one save file targets: {destination}");
                if ((Directory.Exists(destination) || File.Exists(destination))
                    && !fileMoveSources.Contains(destination)
                    && !FileSystemPathIdentity.RefersToSameEntry(source, destination))
                    throw new IOException($"The save destination already exists: {destination}");
            }
            List<(string Intermediate, string Destination)> stagedFileMoves = [];
            foreach ((string source, string destination) in fileMoves)
            {
                currentPath = source;
                report(source);
                if (canWrite is not null && ProjectDiskSnapshot.ReadFingerprint(source) != expectedMoveFiles[source])
                    throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED") + Environment.NewLine + source);
                string intermediate = createSiblingPath(source);
                moveFile(source, intermediate, completedFileMoves);
                stagedFileMoves.Add((intermediate, destination));
            }
            foreach (string directory in directories.OrderBy(path => path.Length))
            {
                currentPath = directory;
                report(directory);
                ensureDirectory(directory, createdDirectories);
            }
            foreach ((string intermediate, string destination) in stagedFileMoves)
            {
                currentPath = destination;
                report(destination);
                ensureDirectory(Path.GetDirectoryName(destination)!, createdDirectories);
                moveFile(intermediate, destination, completedFileMoves);
            }
            foreach (string path in deletions)
            {
                string? replacement = writePaths.FirstOrDefault(destination =>
                    FileSystemPathIdentity.RefersToSameEntry(path, destination));
                if (replacement is null)
                    continue;
                replacedPaths.Add(path);
                originalPaths[replacement] = path;
            }
            string?[] backups = prepareFiles(affectedPaths, (path, index) =>
            {
                verifyFile(path);
                string? backup = File.Exists(path) ? Path.Combine(staging, "original-" + index) : null;
                if (backup is not null)
                    File.Copy(path, backup);
                return backup;
            }, cancellationToken);
            for (int index = 0; index < affectedPaths.Length; index++)
                originals.Add(affectedPaths[index], backups[index]);
            foreach (KeyValuePair<string, string> write in prepared)
            {
                currentPath = write.Key;
                report(currentPath);
                verifyFile(write.Key);
                ensureDirectory(Path.GetDirectoryName(write.Key)!, createdDirectories);
                replaceFile(write.Value, write.Key, errors);
                changedFiles.Add(write.Key);
            }
            foreach (string path in deletions.Where(path => !prepared.ContainsKey(path) && !replacedPaths.Contains(path)))
            {
                currentPath = path;
                report(currentPath);
                verifyFile(path);
                if (!File.Exists(path))
                    continue;
                FilePersistence.DeleteFile(path);
                changedFiles.Add(path);
            }
            foreach (string directory in emptyDirectoryDeletions.OrderByDescending(path => path.Length))
            {
                currentPath = directory;
                report(directory);
                if (!Directory.Exists(directory) || directories.Contains(directory)
                    || directories.Any(target => FileSystemPathIdentity.RefersToSameEntry(directory, target)))
                    continue;
                if (Directory.EnumerateFileSystemEntries(directory).Any())
                    throw new IOException($"The moved asset directory is no longer empty: {directory}");
                Directory.Delete(directory);
                removedEmptyDirectories.Add(directory);
            }
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_WRITING", Completed: total, Total: total));
            cancellationToken.ThrowIfCancellationRequested();
            if (recyclePath is not null)
            {
                currentPath = recyclePath;
                progress?.Report(new EditorOperationProgress("EDIT_OPERATION_DELETING", recyclePath));
                if (!ProjectDiskSnapshot.Equal(recycleInputs, captureRecycleInputs(recyclePath, cancellationToken)))
                    throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED") + Environment.NewLine + recyclePath);
                cancellationToken.ThrowIfCancellationRequested();
                if (recycleInputs.Count != 0)
                    SystemTrashService.MoveToTrash(recyclePath);
            }
            committed = true;
        }
        catch (Exception exception) when (isFileException(exception) || exception is OperationCanceledException)
        {
            cancelled = exception is OperationCanceledException;
            errors.Insert(0, $"{exception.Data[failedPath] as string ?? currentPath}: {exception.Message}");
            foreach (string path in changedFiles.AsEnumerable().Reverse().Distinct(pathComparer))
            {
                try
                {
                    if (canWrite is not null && ProjectDiskSnapshot.ReadFingerprint(path) != fingerprints.GetValueOrDefault(path))
                        throw new IOException($"The file changed after saving and was preserved: {path}");
                    string? original = originals[path];
                    if (original is null)
                    {
                        if (File.Exists(path))
                            FilePersistence.DeleteFile(path);
                    }
                    else
                    {
                        replaceFile(original, originalPaths.GetValueOrDefault(path, path), errors);
                    }
                }
                catch (Exception restoreError) when (isFileException(restoreError))
                {
                    rollbackFailed = true;
                    errors.Add($"Rollback failed for {path}: {restoreError.Message}");
                }
            }
            foreach (string directory in removedEmptyDirectories.AsEnumerable().Reverse())
            {
                try
                {
                    Directory.CreateDirectory(directory);
                }
                catch (Exception restoreError) when (isFileException(restoreError))
                {
                    rollbackFailed = true;
                    errors.Add($"Rollback failed for {directory}: {restoreError.Message}");
                }
            }
            foreach ((string source, string destination) in completedFileMoves.AsEnumerable().Reverse())
            {
                try
                {
                    FilePersistence.MoveFile(destination, source);
                }
                catch (Exception restoreError) when (isFileException(restoreError))
                {
                    rollbackFailed = true;
                    errors.Add($"Rollback failed for {source}: {restoreError.Message}");
                }
            }
            foreach ((string source, string destination) in completedMoves.AsEnumerable().Reverse())
            {
                try
                {
                    Directory.Move(destination, source);
                }
                catch (Exception restoreError) when (isFileException(restoreError))
                {
                    rollbackFailed = true;
                    errors.Add($"Rollback failed for {source}: {restoreError.Message}");
                }
            }
        }
        finally
        {
            foreach (string directory in createdDirectories.AsEnumerable().Reverse())
            {
                cleanup(directory, () =>
                {
                    if ((!committed || !directories.Contains(directory))
                        && Directory.Exists(directory) && !Directory.EnumerateFileSystemEntries(directory).Any())
                        Directory.Delete(directory);
                }, errors);
            }
            if (rollbackFailed)
                errors.Add($"Recovery files retained at {staging}");
            else
                cleanup(staging, () =>
                {
                    if (Directory.Exists(staging))
                        Directory.Delete(staging, true);
                }, errors);
        }
        if (cancelled && !rollbackFailed)
            cancellationToken.ThrowIfCancellationRequested();
        if (committed)
            FilesCommitted?.Invoke(fingerprints);
        return new SaveResult(committed, string.Join(Environment.NewLine, errors));

        void verifyFile(string path)
        {
            if (canWrite is not null && ProjectDiskSnapshot.ReadFingerprint(path) != expectedFiles.GetValueOrDefault(path))
                throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED") + Environment.NewLine + path);
        }
    }

    private static T[] prepareFiles<T>(string[] paths, Func<string, int, T> prepare, CancellationToken token)
    {
        return EditorValidationBatch.RunFiles(Enumerable.Range(0, paths.Length).ToArray(), index =>
        {
            string path = paths[index];
            try
            {
                return prepare(path, index);
            }
            catch (Exception exception) when (isFileException(exception) || exception is OperationCanceledException)
            {
                exception.Data[failedPath] = path;
                throw;
            }
        }, token);
    }

    private static Dictionary<string, string> captureRecycleInputs(string path, CancellationToken token)
    {
        Dictionary<string, string> inputs = new(pathComparer);
        try
        {
            File.GetAttributes(path);
        }
        catch (FileNotFoundException) { return inputs; }
        catch (DirectoryNotFoundException) { return inputs; }
        Stack<string> pending = new();
        List<string> files = [];
        pending.Push(path);
        while (pending.TryPop(out string? current))
        {
            token.ThrowIfCancellationRequested();
            FileAttributes attributes = File.GetAttributes(current);
            if ((attributes & FileAttributes.ReparsePoint) != 0)
                inputs[current] = "link:" + new FileInfo(current).LinkTarget;
            else if ((attributes & FileAttributes.Directory) != 0)
            {
                inputs[current] = "directory";
                foreach (string child in Directory.EnumerateFileSystemEntries(current))
                    pending.Push(child);
            }
            else
                files.Add(current);
        }
        foreach ((string file, string? hash) in ProjectDiskSnapshot.ReadFingerprints(files, token))
            inputs.Add(file, hash ?? throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED") + Environment.NewLine + file));
        return inputs;
    }

    private static void moveDirectory(string source, string destination,
        ICollection<(string Source, string Destination)> completed)
    {
        FilePersistence.MoveDirectory(source, destination);
        completed.Add((source, destination));
    }

    private static void moveFile(string source, string destination,
        ICollection<(string Source, string Destination)> completed)
    {
        FilePersistence.MoveFile(source, destination);
        completed.Add((source, destination));
    }

    private static string createSiblingPath(string path)
    {
        return Path.Combine(Path.GetDirectoryName(path)!, "." + Path.GetFileName(path)
            + ".ludork-save-" + Guid.NewGuid().ToString("N"));
    }

    private static void ensureDirectory(string path, ICollection<string> created)
    {
        if (Directory.Exists(path))
            return;
        string? parent = Path.GetDirectoryName(path);
        if (parent is not null)
            ensureDirectory(parent, created);
        Directory.CreateDirectory(path);
        created.Add(path);
    }

    private static void replaceFile(string source, string destination, ICollection<string> errors)
    {
        string temporary = createSiblingPath(destination);
        try
        {
            using (FileStream input = new(source, FileMode.Open, FileAccess.Read, FileShare.Read))
                FilePersistence.WriteDurable(temporary, input.CopyTo);
            FilePersistence.MoveFile(temporary, destination, true);
        }
        finally
        {
            cleanup(temporary, () =>
            {
                if (File.Exists(temporary))
                    FilePersistence.DeleteFile(temporary);
            }, errors);
        }
    }

    private static void cleanup(string path, Action action, ICollection<string> errors)
    {
        try
        {
            action();
        }
        catch (Exception exception) when (isFileException(exception))
        {
            errors.Add($"Cleanup failed for {path}: {exception.Message}");
        }
    }

    private static bool isFileException(Exception exception)
        => exception is IOException or UnauthorizedAccessException or SecurityException;
}
