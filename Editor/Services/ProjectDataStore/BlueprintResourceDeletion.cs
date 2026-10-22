using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    internal bool IsBlueprintResourcePath(string path)
    {
        EditorDocument? document = Documents.FindByPath(path);
        if (document is not null)
            return document.Section == "Blueprints";
        return tryGetDataLocation(path, out string section, out string relative)
            && section == "Blueprints" && hasDataFileExtension(section, path)
            && sections[section].ContainsKey(normalizeJsonKey(relative));
    }

    internal async Task<BlueprintDeletionBatchResult> DeleteBlueprintResourcesAsync(IReadOnlyList<string> paths,
        IProgress<EditorOperationProgress>? progress = null)
    {
        List<BlueprintDeletionBatchResult.Item> results = [];
        List<PreparedResourceDeletion> completed = [];
        string? error = null;
        if (paths.Count == 0)
            return new BlueprintDeletionBatchResult(results, error);
        try
        {
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_PREPARING"));
            ProjectOperationSnapshot snapshot = await CaptureOperationSnapshotAsync(progress);
            ReferenceInputFiles inputs = await Task.Run(() => ReferenceInputFiles.Capture(ProjectPath,
                snapshot.Documents.SelectMany(document => new[] { document.Current.Path, document.Saved.Path }), includeData: true));
            ReferenceIndexSnapshotBuilder.Result references = await ReferenceIndex.GetCurrentAsync(progress);
            Dictionary<string, ReferenceRecord[]> incoming = indexBlueprintDeletionReferences(references.Snapshot);
            using ProjectDataStore working = await Task.Run(snapshot.CreateStore);
            using ProjectResourceDeletionValidation validation = new(ProjectPath);
            ReferenceIndexSnapshot? diskReferences = null;
            Dictionary<string, ReferenceRecord[]>? diskIncoming = null;
            HashSet<string> deletedPaths = new(StringComparer.OrdinalIgnoreCase);
            HashSet<string> deletedSavedPaths = new(StringComparer.OrdinalIgnoreCase);
            bool isCurrent() => !IsDisposed && Documents.Revision == snapshot.Revision && inputs.IsCurrent();
            for (int index = 0; index < paths.Count; index++)
            {
                string path = paths[index];
                progress?.Report(new EditorOperationProgress("EDIT_OPERATION_VALIDATING", path, index, paths.Count));
                if (!await Task.Run(isCurrent))
                    throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
                EditorDocument? document = working.GetDocumentByPath(path);
                if (document is null || document.Section != "Blueprints" || deletedPaths.Contains(document.Path))
                {
                    results.Add(new BlueprintDeletionBatchResult.Item(path, false, "The resource could not be deleted."));
                    continue;
                }
                string? referenceError = getBlueprintDeletionReferenceError(references.Snapshot,
                    incoming, document.Path, deletedPaths);
                string[] savedPaths = document.SavedState.InternalData is null ? [] : [document.SavedPath];
                if (referenceError is null && savedPaths.Length != 0)
                {
                    if (diskReferences is null)
                    {
                        diskReferences = await Task.Run(() => validation.CaptureSnapshot(progress));
                        diskIncoming = indexBlueprintDeletionReferences(diskReferences);
                    }
                    referenceError = getBlueprintDeletionReferenceError(diskReferences,
                        diskIncoming!, document.SavedPath, deletedSavedPaths);
                }
                if (referenceError is not null)
                {
                    results.Add(new BlueprintDeletionBatchResult.Item(path, false, referenceError));
                    continue;
                }
                PreparedResourceDeletion deletion = working.prepareDocumentDeletionFiles("Blueprints", document, [document], savedPaths);
                progress?.Report(new EditorOperationProgress("EDIT_OPERATION_DELETING", path, index, paths.Count));
                SaveResult result = await Task.Run(() => deletion.Files.Execute(canWrite: isCurrent));
                if (!result.Success)
                {
                    results.Add(new BlueprintDeletionBatchResult.Item(path, false, result.Details));
                    continue;
                }
                completed.Add(deletion);
                results.Add(new BlueprintDeletionBatchResult.Item(path, true, null));
                deletedPaths.Add(document.Path);
                deletedSavedPaths.UnionWith(savedPaths);
                inputs.AcceptDeleted(savedPaths);
                progress?.Report(new EditorOperationProgress("EDIT_OPERATION_DELETING", path, index + 1, paths.Count));
            }
            if (!await Task.Run(isCurrent))
                throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        }
        catch (Exception exception) when (exception is IOException or InvalidOperationException or UnauthorizedAccessException)
        {
            error = exception.Message;
        }
        finally
        {
            if (completed.Count != 0)
            {
                using EditorDocumentNotificationBatch notifications = Documents.BeginNotificationBatch();
                EditorUiBatch batch = new();
                int applied = 0;
                foreach (PreparedResourceDeletion deletion in completed)
                {
                    foreach (PreparedResourceDeletion.Target target in deletion.Targets)
                        applyDocumentDeletion(target);
                    progress?.Report(new EditorOperationProgress("EDIT_OPERATION_APPLYING",
                        deletion.Targets[0].Path, ++applied, completed.Count));
                    await batch.YieldIfNeededAsync();
                }
                completeDocumentDeletion(completed[^1], notifications);
            }
        }
        return new BlueprintDeletionBatchResult(results, error);
    }

    private static Dictionary<string, ReferenceRecord[]> indexBlueprintDeletionReferences(ReferenceIndexSnapshot references)
    {
        return references.References.Where(reference => references.GetNode(reference.Target)?.Type == "blueprint")
            .GroupBy(reference => references.GetNodePath(reference.Target), getPathComparer())
            .ToDictionary(group => group.Key, group => group
                .OrderBy(reference => references.GetNode(reference.Source)?.Type, StringComparer.Ordinal)
                .ThenBy(reference => references.GetNode(reference.Source)?.Key, StringComparer.Ordinal)
                .ThenBy(reference => reference.Path, StringComparer.Ordinal).ToArray(), getPathComparer());
    }

    private static string? getBlueprintDeletionReferenceError(ReferenceIndexSnapshot references,
        IReadOnlyDictionary<string, ReferenceRecord[]> incomingByPath, string path, IReadOnlySet<string> deletedPaths)
    {
        ReferenceRecord[] incoming = (incomingByPath.GetValueOrDefault(path) ?? [])
            .Where(reference => !string.Equals(references.GetNodePath(reference.Source), path, StringComparison.OrdinalIgnoreCase)
                && !deletedPaths.Contains(references.GetNodePath(reference.Source)))
            .ToArray();
        return incoming.Length == 0 ? null : LocaleService.Get("DOCUMENT_DELETE_REFERENCED") + Environment.NewLine
            + string.Join(Environment.NewLine, incoming.Select(reference => references.GetNodePath(reference.Source) + ": " + reference.Path));
    }
}
