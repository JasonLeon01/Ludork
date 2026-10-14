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
    private static readonly StringComparer pathComparer = OperatingSystem.IsWindows()
        ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
    private readonly Dictionary<string, byte[]> writes = new(pathComparer);
    private readonly Dictionary<string, string> fileWrites = new(pathComparer);
    private readonly HashSet<string> deletions = new(pathComparer);
    private readonly HashSet<string> directoryDeletions = new(pathComparer);
    private readonly List<(string Source, string Destination)> moves = [];
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
    public void DeleteDirectory(string path) => directoryDeletions.Add(Path.GetFullPath(path));
    public void EnsureDirectoryCase(string path) => directorySpellings.Add(Path.GetFullPath(path));

    public void MoveDirectory(string source, string destination)
    {
        if (!string.Equals(source, destination, StringComparison.Ordinal))
            moves.Add((Path.GetFullPath(source), Path.GetFullPath(destination)));
    }

    public SaveResult Execute(IProgress<EditorOperationProgress>? progress = null, CancellationToken cancellationToken = default)
    {
        string staging = Path.Combine(Path.GetTempPath(), "LudorkSave-" + Guid.NewGuid().ToString("N"));
        Dictionary<string, string> prepared = new(pathComparer);
        Dictionary<string, string?> originals = new(pathComparer);
        string[] writePaths = writes.Keys.Concat(fileWrites.Keys).ToArray();
        Dictionary<string, string> originalPaths = new(pathComparer);
        HashSet<string> replacedPaths = new(pathComparer);
        List<string> changedFiles = [];
        List<(string Source, string Destination)> completedMoves = [];
        List<(string Source, string Backup)> removedDirectories = [];
        List<string> createdDirectories = [];
        List<string> errors = [];
        string currentPath = staging;
        bool committed = false;
        bool rollbackFailed = false;
        bool cancelled = false;
        int completed = 0;
        int total = writePaths.Length * 2 + moves.Count * 2 + directoryDeletions.Count + deletions.Count;
        void report(string path)
        {
            cancellationToken.ThrowIfCancellationRequested();
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_WRITING", path, completed++, total));
        }
        try
        {
            Directory.CreateDirectory(staging);
            foreach (KeyValuePair<string, byte[]> write in writes)
            {
                currentPath = write.Key;
                report(currentPath);
                string temporary = Path.Combine(staging, "write-" + prepared.Count);
                File.WriteAllBytes(temporary, write.Value);
                prepared.Add(write.Key, temporary);
            }
            foreach (KeyValuePair<string, string> write in fileWrites)
            {
                currentPath = write.Key;
                report(currentPath);
                string temporary = Path.Combine(staging, "write-" + prepared.Count);
                File.Copy(write.Value, temporary);
                prepared.Add(write.Key, temporary);
            }
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
            foreach (string directory in directoryDeletions.OrderBy(path => path.Length))
            {
                currentPath = directory;
                report(currentPath);
                if (!Directory.Exists(directory))
                    continue;
                string backup = createSiblingPath(directory);
                Directory.Move(directory, backup);
                removedDirectories.Add((directory, backup));
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
            foreach (string path in writePaths.Concat(deletions).Distinct(pathComparer))
            {
                currentPath = path;
                cancellationToken.ThrowIfCancellationRequested();
                string? backup = File.Exists(path) ? Path.Combine(staging, "original-" + originals.Count) : null;
                if (backup is not null)
                    File.Copy(path, backup);
                originals[path] = backup;
            }
            foreach (KeyValuePair<string, string> write in prepared)
            {
                currentPath = write.Key;
                report(currentPath);
                ensureDirectory(Path.GetDirectoryName(write.Key)!, createdDirectories);
                replaceFile(write.Value, write.Key, errors);
                changedFiles.Add(write.Key);
            }
            foreach (string path in deletions.Where(path => !prepared.ContainsKey(path) && !replacedPaths.Contains(path)))
            {
                currentPath = path;
                report(currentPath);
                if (!File.Exists(path))
                    continue;
                FilePersistence.DeleteFile(path);
                changedFiles.Add(path);
            }
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_WRITING", Completed: total, Total: total));
            cancellationToken.ThrowIfCancellationRequested();
            committed = true;
        }
        catch (Exception exception) when (isFileException(exception) || exception is OperationCanceledException)
        {
            cancelled = exception is OperationCanceledException;
            errors.Insert(0, $"{currentPath}: {exception.Message}");
            foreach (string path in changedFiles.AsEnumerable().Reverse().Distinct(pathComparer))
            {
                try
                {
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
            foreach ((string source, string backup) in removedDirectories.AsEnumerable().Reverse())
            {
                try
                {
                    Directory.Move(backup, source);
                }
                catch (Exception restoreError) when (isFileException(restoreError))
                {
                    rollbackFailed = true;
                    errors.Add($"Rollback failed for {source}; backup retained at {backup}: {restoreError.Message}");
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
            if (committed)
            {
                foreach ((string _, string backup) in removedDirectories)
                    cleanup(backup, () => Directory.Delete(backup, true), errors);
            }
            foreach (string directory in createdDirectories.AsEnumerable().Reverse())
            {
                cleanup(directory, () =>
                {
                    if (Directory.Exists(directory) && !Directory.EnumerateFileSystemEntries(directory).Any())
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
        return new SaveResult(committed, string.Join(Environment.NewLine, errors));
    }

    private static void moveDirectory(string source, string destination,
        ICollection<(string Source, string Destination)> completed)
    {
        FilePersistence.MoveDirectory(source, destination);
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
