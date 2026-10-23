using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectAssetPaths
{
    private bool move(IReadOnlyList<(string OldPath, string NewPath)> moves, EditorDocument? replaySource = null)
    {
        (string OldPath, string NewPath)[] roots = moves.Select(move => (
                Path.TrimEndingDirectorySeparator(Path.GetFullPath(move.OldPath)),
                Path.TrimEndingDirectorySeparator(Path.GetFullPath(move.NewPath))))
            .Where(move => move.Item1 != move.Item2).Distinct().ToArray();
        if (roots.Length == 0)
            return false;
        using EditorDocumentTransaction transaction = store.Documents.BeginTransaction([]);
        Dictionary<EditorDocument, (string Path, EditorDocument Source)> changes = [];
        foreach ((string oldPath, string newPath) in roots)
        {
            if (!contains(oldPath) || !contains(newPath))
                throw new InvalidOperationException("Assets can only be moved within the project Assets directory.");
            if (roots.Any(other => other.OldPath != oldPath && isChild(other.OldPath, oldPath)))
                throw new InvalidOperationException("A move cannot include both a directory and one of its children.");
            bool directory = DirectoryExists(oldPath);
            if (!directory && !FileExists(oldPath))
                throw new FileNotFoundException("The asset no longer exists.", oldPath);
            if (directory && newPath.StartsWith(oldPath + Path.DirectorySeparatorChar,
                    fileSystemComparer == StringComparer.OrdinalIgnoreCase ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal))
                throw new InvalidOperationException("A directory cannot be moved into itself.");
            if (!DirectoryExists(Path.GetDirectoryName(newPath)!))
                throw new DirectoryNotFoundException(Path.GetDirectoryName(newPath));
            EditorDocument source = ensureDocument(oldPath, directory);
            changes.Add(source, (newPath, source));
            if (directory)
            {
                foreach (ProjectAssetPathEntry entry in enumerateTree(oldPath))
                {
                    EditorDocument document = ensureDocument(entry.Path, entry.IsDirectory);
                    changes.Add(document, (Path.Combine(newPath, Path.GetRelativePath(oldPath, entry.Path)), source));
                }
            }
        }
        HashSet<string> destinations = new(fileSystemComparer);
        HashSet<string> sources = changes.Keys.Select(document => document.Path).ToHashSet(StringComparer.Ordinal);
        foreach ((EditorDocument document, (string destination, EditorDocument _)) in changes)
        {
            if (!destinations.Add(destination))
                throw new IOException($"More than one asset targets the same path: {destination}");
            if (documents().Any(existing => fileSystemComparer.Equals(existing.Path, destination) && !changes.ContainsKey(existing)))
                throw new IOException($"The asset destination already exists: {destination}");
            if ((FileExists(destination) || DirectoryExists(destination)) && !sources.Contains(destination)
                && !FileSystemPathIdentity.RefersToSameEntry(ResolveReadPath(document.Path), ResolveReadPath(destination)))
                throw new IOException($"The asset destination already exists: {destination}");
            if (!EditorPathSandbox.TryResolve(root, destination, out _, allowMissing: true))
                throw new IOException($"The asset path is outside the project or contains a symbolic link: {destination}");
        }
        Dictionary<string, string> replacements = new(StringComparer.Ordinal);
        foreach ((EditorDocument document, (string destination, EditorDocument _)) in changes)
        {
            if (isDirectory(document))
                continue;
            string oldReference = toReference(document.Path);
            string newReference = toReference(destination);
            if (!GameAssetPath.IsCanonical(oldReference))
                continue;
            if (!GameAssetPath.IsCanonical(newReference))
                throw new InvalidOperationException("An asset file must retain a file extension.");
            replacements.Add(oldReference, newReference);
        }
        IReadOnlyList<ReferenceRewrite> rewrites = store.ReferenceIndex.PrepareAssetPathRewrites(replacements);
        store.ApplyReferenceRewrites(rewrites);
        foreach ((EditorDocument document, (string destination, EditorDocument source)) in changes)
        {
            if (ReferenceEquals(document, replaySource))
            {
                store.Documents.Restore(document, new EditorDocumentState(Section, key(destination), destination, document.InternalData));
                continue;
            }
            HistoryMarker? marker = ReferenceEquals(document, source) ? null
                : new HistoryMarker(LocaleService.Get("DOCUMENT_HISTORY_REFERENCE_BARRIER")
                    .Replace("{source}", source.Path, StringComparison.Ordinal), source.Id, source.Path, true);
            store.Documents.Capture(document, $"Move {document.Path} to {destination}", marker);
            store.Documents.Commit(document, document.InternalData, key(destination), destination);
        }
        store.SetRenamedReferences(store.RenamedReferences.WithAssetReferences(
            replacements.SelectMany(replacement => new[] { replacement.Key, replacement.Value })));
        transaction.Commit();
        return true;
    }

    private IEnumerable<ProjectAssetPathEntry> enumerateTree(string directory)
    {
        string physical = ResolveReadPath(directory);
        if (Directory.Exists(physical))
            foreach (FileSystemInfo entry in new DirectoryInfo(physical).EnumerateFileSystemInfos())
                if (EditorPathSandbox.IsLink(entry))
                    throw new IOException($"The asset directory contains a symbolic link: {entry.FullName}");
        foreach (ProjectAssetPathEntry entry in EnumerateEntries(directory))
        {
            yield return entry;
            if (entry.IsDirectory)
                foreach (ProjectAssetPathEntry child in enumerateTree(entry.Path))
                    yield return child;
        }
    }

    private EditorDocument ensureDocument(string path, bool directory)
    {
        EditorDocument? document = GetDocumentByPath(path);
        if (document is not null)
        {
            if (!document.IsModified && !directory)
            {
                FileInfo existingFile = new(document.SavedPath);
                if (!existingFile.Exists)
                    throw new FileNotFoundException("The asset no longer exists.", document.SavedPath);
                JsonObject current = document.Data!;
                current["length"] = existingFile.Length;
                current["modified"] = existingFile.LastWriteTimeUtc.Ticks;
                if (!JsonNode.DeepEquals(current, document.InternalData))
                {
                    store.Documents.Restore(document, new EditorDocumentState(Section, document.Key, document.Path, current));
                    store.Documents.MarkSaved(document);
                }
            }
            return document;
        }
        string physical = ResolveReadPath(path);
        FileSystemInfo entry = directory ? new DirectoryInfo(physical) : new FileInfo(physical);
        if (!entry.Exists || EditorPathSandbox.IsLink(entry))
            throw new IOException($"The asset is unavailable or is a symbolic link: {path}");
        JsonObject metadata = new() { ["directory"] = directory };
        if (entry is FileInfo file)
        {
            metadata["length"] = file.Length;
            metadata["modified"] = file.LastWriteTimeUtc.Ticks;
        }
        document = store.Documents.Register(Section, key(path), path, metadata);
        if (physical != path)
        {
            document.SavedState = new EditorDocumentState(Section, key(physical), physical, metadata);
            document.UpdateModified();
        }
        Attach(document);
        return document;
    }

    internal void Attach(EditorDocument document)
    {
        document.HistoryRestorer = (entry, undo) => restoreHistory(document, entry, undo);
    }

    internal bool IsMovedReferenceUnavailable(string reference)
    {
        return store.RenamedReferences.ContainsAsset(reference) && !TryResolveAsset(reference, out _);
    }

    private HistoryResult restoreHistory(EditorDocument document, DocumentHistoryEntry entry, bool undo)
    {
        EditorDocumentState destination = undo ? entry.Before : entry.After;
        try
        {
            bool changed = move([(document.Path, destination.Path)], document);
            return new HistoryResult(changed) { Changes = changed ? [document.Path] : [] };
        }
        catch (Exception exception) when (exception is IOException or InvalidOperationException or ArgumentException)
        {
            return new HistoryResult(false, exception.Message);
        }
    }
}
