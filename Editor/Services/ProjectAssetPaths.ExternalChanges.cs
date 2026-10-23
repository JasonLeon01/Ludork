using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ProjectAssetPaths
{
    internal IReadOnlyList<EditorDocument[]> GetExternalChangeGroups()
    {
        List<EditorDocument> remaining = documents().ToList();
        List<EditorDocument[]> groups = [];
        while (remaining.FirstOrDefault(document => document.IsModified) is EditorDocument source)
        {
            List<EditorDocument> group = [source];
            remaining.Remove(source);
            for (int index = 0; index < group.Count; index++)
            {
                EditorDocument member = group[index];
                EditorDocument[] related = remaining.Where(document => externalDocumentsOverlap(member, document)).ToArray();
                group.AddRange(related);
                foreach (EditorDocument document in related)
                    remaining.Remove(document);
            }
            groups.Add(group.ToArray());
        }
        return groups;
    }

    private bool externalDocumentsOverlap(EditorDocument left, EditorDocument right)
    {
        return new[] { left.Path, left.SavedPath }.Any(leftPath =>
            new[] { right.Path, right.SavedPath }.Any(rightPath => fileSystemComparer.Equals(leftPath, rightPath)
                || left.IsModified && isDirectory(left) && isFileSystemChild(leftPath, rightPath)
                || right.IsModified && isDirectory(right) && isFileSystemChild(rightPath, leftPath)));
    }

    private bool isFileSystemChild(string parent, string path)
        => path.StartsWith(Path.TrimEndingDirectorySeparator(parent) + Path.DirectorySeparatorChar,
            fileSystemComparer == StringComparer.OrdinalIgnoreCase ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);

    internal bool IsExternalChangeRelated(IReadOnlyList<EditorDocument> group, string path)
        => group.Any(document => overlapsFileSystemPath(document.Path, path)
            || overlapsFileSystemPath(document.SavedPath, path));

    internal Task<ProjectAssetExternalSnapshot> CaptureExternalChangesAsync(CancellationToken token)
    {
        ExternalPathState[][] groups = GetExternalChangeGroups().Select(group => group
            .Select(document => new ExternalPathState(document.Path, document.SavedState)).ToArray()).ToArray();
        return Task.Run(() =>
        {
            ProjectAssetDiskInputs inputs = new();
            Dictionary<string, ExternalFileChange> changes = new(fileSystemComparer);
            foreach (ExternalPathState[] group in groups)
            {
                Dictionary<string, EditorDocumentState> saved = group.ToDictionary(entry => entry.Saved.Path,
                    entry => entry.Saved, fileSystemComparer);
                foreach (ExternalPathState entry in group)
                {
                    token.ThrowIfCancellationRequested();
                    JsonObject? metadata = readExternalMetadata(entry.Saved.Path, inputs);
                    if (!JsonNode.DeepEquals(metadata, entry.Saved.InternalData))
                        changes[entry.Saved.Path] = new ExternalFileChange(entry.Saved.Path,
                            entry.Saved.InternalData?.ToJsonString(), metadata?.ToJsonString(), null);
                    if (metadata?["directory"]?.GetValue<bool>() == true)
                        captureExternalDirectoryChanges(entry.Saved.Path, saved, changes, inputs, token);
                    if (!saved.ContainsKey(entry.Path) && readExternalMetadata(entry.Path, inputs) is JsonObject target)
                        changes[entry.Path] = new ExternalFileChange(entry.Path, null, target.ToJsonString(), null);
                }
            }
            inputs.Validate();
            return new ProjectAssetExternalSnapshot(changes.Values.ToArray(), inputs);
        }, token);
    }

    private void captureExternalDirectoryChanges(string path, IReadOnlyDictionary<string, EditorDocumentState> saved,
        IDictionary<string, ExternalFileChange> changes, ProjectAssetDiskInputs inputs, CancellationToken token)
    {
        foreach (string child in inputs.ReadDirectory(path))
        {
            token.ThrowIfCancellationRequested();
            JsonObject metadata = readExternalMetadata(child, inputs)
                ?? throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            if (saved.ContainsKey(child))
                continue;
            changes[child] = new ExternalFileChange(child, null, metadata.ToJsonString(), null);
            if (metadata["directory"]?.GetValue<bool>() == true)
                captureExternalDirectoryChanges(child, saved, changes, inputs, token);
        }
    }

    internal PreparedAssetDiskChanges PrepareExternalChanges(IReadOnlyList<ExternalFileChange> externalChanges,
        IReadOnlySet<string> keepPaths)
    {
        ProjectAssetDiskInputs inputs = new();
        List<PreparedAssetPathChange> changes = [];
        Dictionary<string, string> replacements = new(StringComparer.Ordinal);
        HashSet<string> deleted = new(StringComparer.Ordinal);
        HashSet<Guid> handled = [];
        foreach (EditorDocument[] group in GetExternalChangeGroups())
        {
            if (!externalChanges.Any(change => IsExternalChangeRelated(group, change.Path)))
                continue;
            bool keep = group.Any(document => keepPaths.Contains(document.Path) || keepPaths.Contains(document.SavedPath));
            if (keep)
                prepareKeptExternalGroup(group, inputs, changes);
            else
            {
                foreach (EditorDocument document in group)
                {
                    if (!isDirectory(document) && document.Path != document.SavedPath
                        && GameAssetPath.IsCanonical(toReference(document.Path))
                        && GameAssetPath.IsCanonical(toReference(document.SavedPath)))
                        replacements.Add(toReference(document.Path), toReference(document.SavedPath));
                    JsonObject? metadata = readExternalMetadata(document.SavedPath, inputs);
                    if (metadata is null || metadata["directory"]?.GetValue<bool>() != isDirectory(document))
                    {
                        changes.Add(new PreparedAssetPathChange(document.Id, null, null, true));
                        deleted.Add(document.SavedPath);
                        deleted.Add(document.Path);
                        continue;
                    }
                    EditorDocumentState state = new(Section, key(document.SavedPath), document.SavedPath, metadata);
                    changes.Add(new PreparedAssetPathChange(document.Id, state, state, true));
                }
            }
            handled.UnionWith(group.Select(document => document.Id));
        }
        foreach (EditorDocument document in documents().Where(document => !handled.Contains(document.Id)
            && externalChanges.Any(change => overlapsFileSystemPath(change.Path, document.SavedPath))))
        {
            if (document.IsModified)
                throw new InvalidOperationException("The pending asset move was not included in the external change group.");
            JsonObject? metadata = readExternalMetadata(document.SavedPath, inputs);
            changes.Add(new PreparedAssetPathChange(document.Id, null, null, true));
            if (metadata is null)
            {
                deleted.Add(document.Path);
                foreach (DocumentHistoryEntry entry in document.UndoEntries.Concat(document.RedoEntries))
                {
                    deleted.Add(entry.Before.Path);
                    deleted.Add(entry.After.Path);
                }
            }
        }
        IReadOnlyList<ReferenceRewrite> rewrites = replacements.Count == 0 ? []
            : store.ReferenceIndex.PrepareAssetPathRewrites(replacements);
        inputs.Validate();
        return new PreparedAssetDiskChanges(changes, rewrites, deleted.ToArray(), inputs);
    }

    private void prepareKeptExternalGroup(IReadOnlyList<EditorDocument> group, ProjectAssetDiskInputs inputs,
        ICollection<PreparedAssetPathChange> changes)
    {
        Dictionary<string, EditorDocument> sources = group.ToDictionary(document => document.SavedPath, fileSystemComparer);
        Dictionary<string, (Guid Id, string Destination, JsonObject Metadata)> entries = new(fileSystemComparer);
        foreach (EditorDocument document in group)
        {
            JsonObject? metadata = readExternalMetadata(document.SavedPath, inputs);
            if (metadata is null)
                throw new InvalidDataException($"The asset source was deleted outside the editor: {document.SavedPath}. Use the disk version or restore the source before keeping the pending move.");
            if (metadata["directory"]?.GetValue<bool>() != isDirectory(document))
                throw new InvalidDataException($"The asset source changed its file type outside the editor: {document.SavedPath}. Use the disk version to cancel the pending move.");
            entries.Add(document.SavedPath, (document.Id, document.Path, metadata));
        }
        foreach (EditorDocument directory in group.Where(isDirectory)
            .Where(document => !group.Any(parent => isDirectory(parent) && isFileSystemChild(parent.SavedPath, document.SavedPath))))
            addExternalDirectoryEntries(directory.SavedPath, group, sources, entries, inputs);
        HashSet<string> destinations = new(fileSystemComparer);
        HashSet<Guid> groupIds = group.Select(document => document.Id).ToHashSet();
        foreach ((string source, (Guid id, string destination, JsonObject metadata)) in entries)
        {
            if (!destinations.Add(destination) || documents().Any(document => !groupIds.Contains(document.Id)
                && fileSystemComparer.Equals(document.Path, destination)))
                throw new InvalidDataException($"The asset destination is already in use: {destination}. Use the disk version or change the destination before keeping the pending move.");
            JsonObject? target = readExternalMetadata(destination, inputs);
            if (target is not null && !entries.ContainsKey(destination)
                && !FileSystemPathIdentity.RefersToSameEntry(source, destination))
                throw new InvalidDataException($"The asset destination was created outside the editor: {destination}. Use the disk version or remove the conflicting destination before keeping the pending move.");
            string parent = Path.GetDirectoryName(destination)!;
            if (!entries.Values.Any(entry => fileSystemComparer.Equals(entry.Destination, parent)
                    && entry.Metadata["directory"]?.GetValue<bool>() == true)
                && readExternalMetadata(parent, inputs)?["directory"]?.GetValue<bool>() != true)
                throw new InvalidDataException($"The asset destination directory no longer exists: {parent}. Use the disk version or restore the directory before keeping the pending move.");
            EditorDocumentState current = new(Section, key(destination), destination, metadata);
            EditorDocumentState saved = new(Section, key(source), source, metadata);
            changes.Add(new PreparedAssetPathChange(id, current, saved, false));
        }
    }

    private void addExternalDirectoryEntries(string path, IReadOnlyList<EditorDocument> group,
        IReadOnlyDictionary<string, EditorDocument> sources,
        IDictionary<string, (Guid Id, string Destination, JsonObject Metadata)> entries,
        ProjectAssetDiskInputs inputs)
    {
        foreach (string child in inputs.ReadDirectory(path))
        {
            JsonObject metadata = readExternalMetadata(child, inputs)
                ?? throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            if (!sources.ContainsKey(child) && !entries.ContainsKey(child))
            {
                EditorDocument owner = group.Where(document => isDirectory(document) && isFileSystemChild(document.SavedPath, child))
                    .OrderByDescending(document => document.SavedPath.Length).First();
                string destination = Path.Combine(owner.Path, Path.GetRelativePath(owner.SavedPath, child));
                entries.Add(child, (Guid.NewGuid(), destination, metadata));
            }
            if (metadata["directory"]?.GetValue<bool>() == true)
                addExternalDirectoryEntries(child, group, sources, entries, inputs);
        }
    }

    private JsonObject? readExternalMetadata(string path, ProjectAssetDiskInputs inputs)
    {
        if (!EditorPathSandbox.IsSameOrChildPath(root, path))
            throw new IOException($"The asset path is outside the project: {path}");
        Stack<string> parents = new();
        for (string? parent = Path.GetDirectoryName(path); parent is not null
            && EditorPathSandbox.IsSameOrChildPath(root, parent); parent = Path.GetDirectoryName(parent))
            parents.Push(parent);
        foreach (string parent in parents)
            inputs.Read(parent);
        return inputs.Read(path);
    }

    private sealed record ExternalPathState(string Path, EditorDocumentState Saved);

    internal void ApplyExternalChanges(PreparedAssetDiskChanges prepared)
    {
        Dictionary<Guid, EditorDocument> existing = documents().ToDictionary(document => document.Id);
        foreach (PreparedAssetPathChange change in prepared.Changes)
        {
            existing.TryGetValue(change.Id, out EditorDocument? document);
            if (change.Current is not EditorDocumentState current || change.Saved is not EditorDocumentState saved)
            {
                if (document is not null)
                {
                    store.Documents.Remove(document);
                    store.Documents.RequestDiskReload(document);
                }
                continue;
            }
            if (document is null)
            {
                document = store.Documents.Register(Section, current.Key, current.Path, current.Data, id: change.Id);
                Attach(document);
            }
            store.Documents.Restore(document, current);
            store.Documents.MarkSaved(document, saved);
            if (change.ClearHistory)
            {
                document.UndoEntries.Clear();
                document.RedoEntries.Clear();
            }
            store.Documents.RequestDiskReload(document);
        }
        store.SetRenamedReferences(store.RenamedReferences.WithAssetReferences(prepared.Changes
            .Where(change => change.Current is not null && change.Saved is not null
                && change.Current.Path != change.Saved.Path && change.Current.InternalData?["directory"]?.GetValue<bool>() != true)
            .SelectMany(change => new[] { toReference(change.Current!.Path), toReference(change.Saved!.Path) })
            .Where(GameAssetPath.IsCanonical)));
        store.deletedDocumentPaths.UnionWith(prepared.DeletedPaths);
    }
}
