using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal sealed class FileExplorerDeletionValidation(ProjectDataStore gameData, long revision, ReferenceInputFiles inputs,
    IReadOnlyList<string> errors)
{
    internal IReadOnlyList<string> Errors => errors;

    internal static async Task<FileExplorerDeletionValidation> PrepareAsync(ProjectDataStore gameData, string[] paths,
        IReadOnlySet<string> assetPaths, ProjectResourceDeletionValidation savedData, IProgress<EditorOperationProgress>? progress)
    {
        ProjectOperationSnapshot project = await gameData.CaptureOperationSnapshotAsync(progress);
        FileExplorerDeletionValidation validation = await Task.Run(() =>
        {
            ReferenceInputFiles inputs = ReferenceInputFiles.Capture(project.ProjectPath, [], includeData: true, includeStubs: true);
            using ProjectDataStore working = project.CreateStore();
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_VALIDATING"));
            ReferenceImpact impact = working.ReferenceIndex.GetImpactForPaths(paths);
            ReferenceIndexSnapshot current = working.ReferenceIndex.CurrentSnapshot!;
            List<string> errors = [];
            if (assetPaths.Count != 0)
            {
                SortedSet<string> references = new(StringComparer.Ordinal);
                collectAssetReferences(current, assetPaths, references, project.ProjectPath);
                collectAssetReferences(savedData.CaptureSnapshot(progress), assetPaths, references, project.ProjectPath);
                if (references.Count != 0)
                    errors.Add(LocaleService.Get("DOCUMENT_DELETE_REFERENCED") + Environment.NewLine
                        + string.Join(Environment.NewLine, references));
            }
            string[] uiIncoming = impact.Incoming.Where(record => current.GetNode(record.Target)?.Type != "asset"
                    && (current.GetNode(record.Source)?.Type == "uiAsset" || current.GetNode(record.Target)?.Type == "uiAsset"))
                .Select(record => $"{current.GetNode(record.Source)?.Key ?? record.Source} [{record.Kind}] {record.Path}")
                .Distinct(StringComparer.Ordinal).ToArray();
            if (uiIncoming.Length != 0)
                errors.Add(LocaleService.Get("UI_ASSET_DELETE_REFERENCED")
                    .Replace("{references}", string.Join(Environment.NewLine, uiIncoming), StringComparison.Ordinal));
            if (!inputs.IsCurrent())
                throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            return new FileExplorerDeletionValidation(gameData, project.Revision, inputs, errors);
        });
        if (gameData.IsDisposed || gameData.Documents.Revision != project.Revision)
            throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        return validation;
    }

    private static void collectAssetReferences(ReferenceIndexSnapshot index, IReadOnlySet<string> assetPaths,
        SortedSet<string> references, string projectPath)
    {
        foreach (ReferenceNode node in index.Nodes)
        {
            if (node.Type != "asset")
                continue;
            string? path = index.GetNodePath(node.Id);
            while (!string.IsNullOrEmpty(path) && !assetPaths.Contains(path))
                path = Path.GetDirectoryName(path);
            if (string.IsNullOrEmpty(path))
                continue;
            foreach (ReferenceRecord record in index.GetIncoming(node.Id))
            {
                string source = index.GetNodePath(record.Source);
                source = source.Length == 0 ? record.Source : Path.GetRelativePath(projectPath, source).Replace('\\', '/');
                references.Add($"{node.Key} ← {source} [{record.Path}]");
            }
        }
    }

    internal Task<bool> TryRecycleAssetAsync(string path) => SystemTrashService.RunAsync(trash =>
    {
        try
        {
            if (!inputs.IsCurrent())
                return false;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            return false;
        }
        if (gameData.IsDisposed || gameData.Documents.Revision != revision)
            return false;
        gameData.BuiltInResources.AssertCanChangeOnDisk(path);
        trash.MoveToTrash(path);
        return true;
    }, CancellationToken.None);
}
