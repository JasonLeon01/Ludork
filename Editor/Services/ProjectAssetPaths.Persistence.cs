using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Ludork.Services;

public sealed partial class ProjectAssetPaths
{
    internal void AddToSaveBatch(EditorFileSaveBatch batch)
    {
        ValidatePendingInputs();
        EditorDocument[] changed = documents().Where(document => document.IsModified).ToArray();
        foreach (EditorDocument document in changed.Where(isDirectory).OrderBy(document => document.Path.Length))
        {
            batch.EnsureDirectory(document.Path);
            if (FileSystemPathIdentity.RefersToSameEntry(document.SavedPath, document.Path))
                batch.EnsureDirectoryCase(document.Path);
        }
        foreach (EditorDocument document in changed.Where(document => !isDirectory(document)))
            batch.MoveFile(document.SavedPath, document.Path);
        foreach (EditorDocument document in changed.Where(isDirectory).OrderByDescending(document => document.SavedPath.Length))
            if (!FileSystemPathIdentity.RefersToSameEntry(document.SavedPath, document.Path))
                batch.DeleteEmptyDirectory(document.SavedPath);
    }

    internal void ValidatePendingInputs()
    {
        CaptureInputs().Validate();
    }

    internal ProjectAssetPathInputs CaptureInputs()
    {
        EditorDocument[] pending = documents().Where(document => document.IsModified).ToArray();
        return new ProjectAssetPathInputs(documents().Where(document => pending.Any(changed =>
                document.Id == changed.Id || isDirectory(changed) && isChild(changed.SavedPath, document.SavedPath)))
            .Select(document => document.SavedState).ToArray());
    }

    internal void ForgetExternalPaths(IEnumerable<string> paths)
    {
        string[] changed = paths.Select(path => Path.TrimEndingDirectorySeparator(Path.GetFullPath(path))).ToArray();
        foreach (EditorDocument document in documents().Where(document => !document.IsModified
                     && changed.Any(path => document.Path == path || isChild(path, document.Path))).ToArray())
        {
            if (!isDirectory(document) && !File.Exists(document.SavedPath))
            {
                store.deletedDocumentPaths.Add(document.Path);
                foreach (DocumentHistoryEntry entry in document.UndoEntries.Concat(document.RedoEntries))
                {
                    store.deletedDocumentPaths.Add(entry.Before.Path);
                    store.deletedDocumentPaths.Add(entry.After.Path);
                }
            }
            store.Documents.Remove(document);
        }
    }
}
