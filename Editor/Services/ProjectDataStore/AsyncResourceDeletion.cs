using Ludork.Models;
using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    public async Task<bool> DeleteDocumentResourceAsync(string section, string key,
        IProgress<EditorOperationProgress>? progress = null)
    {
        using ProjectResourceDeletionValidation validation = new(ProjectPath);
        return await deleteDocumentResourceAsync(section, normalizeJsonKey(key), validation, progress);
    }

    public async Task<(bool Handled, string? Error)> TryDeleteManagedPathAsync(string path,
        IProgress<EditorOperationProgress>? progress = null)
    {
        using ProjectResourceDeletionValidation validation = new(ProjectPath);
        return await TryDeleteManagedPathAsync(path, validation, progress);
    }

    internal async Task<(bool Handled, string? Error)> TryDeleteManagedPathAsync(string path,
        ProjectResourceDeletionValidation validation, IProgress<EditorOperationProgress>? progress = null)
    {
        EditorDocument? document = Documents.FindByPath(path);
        string? section = document?.Section;
        string? key = document?.Key;
        string? worldKey = sections["WorldMaps"].Keys.FirstOrDefault(value => pathsEqual(Worlds.getWorldDirectory(value), path));
        if (worldKey is not null)
        {
            section = "WorldMaps";
            key = worldKey;
        }
        else if (document is null && tryGetDataLocation(path, out string dataSection, out string relative)
            && hasDataFileExtension(dataSection, path))
        {
            string dataKey = normalizeJsonKey(relative);
            if (dataSection == "Maps" ? Maps.containsMapKey(dataKey) : sections[dataSection].ContainsKey(dataKey))
            {
                section = dataSection;
                key = dataKey;
            }
        }
        if (section is null || key is null)
            return IsManagedPath(path) ? (true, "Delete managed resource files or complete worlds individually.") : (false, null);
        try
        {
            bool deleted = await deleteDocumentResourceAsync(section, key, validation, progress);
            return (true, deleted ? null : "The resource could not be deleted.");
        }
        catch (Exception exception) when (exception is IOException or InvalidOperationException or UnauthorizedAccessException)
        {
            return (true, exception.Message);
        }
    }

    private async Task<bool> deleteDocumentResourceAsync(string section, string key,
        ProjectResourceDeletionValidation validation, IProgress<EditorOperationProgress>? progress)
    {
        progress?.Report(new EditorOperationProgress("EDIT_OPERATION_PREPARING"));
        ProjectOperationSnapshot snapshot = await CaptureOperationSnapshotAsync(progress);
        ReferenceInputFiles inputs = await Task.Run(() => ReferenceInputFiles.Capture(ProjectPath,
            snapshot.Documents.SelectMany(document => new[] { document.Current.Path, document.Saved.Path })));
        ReferenceIndexSnapshotBuilder.Result references = await ReferenceIndex.GetCurrentAsync(progress);
        PreparedResourceDeletion? deletion = await Task.Run(() =>
        {
            if (!inputs.IsCurrent() || !references.Files.IsCurrent())
                throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            using ProjectDataStore working = snapshot.CreateStore();
            working.ReferenceIndex.UseOperationSnapshot(references);
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_VALIDATING"));
            return working.prepareDocumentDeletion(section, key, validation, progress);
        });
        if (deletion is null)
            return false;
        bool filesCurrent = await Task.Run(() => inputs.IsCurrent() && references.Files.IsCurrent() && validation.IsCurrent());
        if (IsDisposed || Documents.Revision != snapshot.Revision || !filesCurrent)
            throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        SaveResult result = await Task.Run(() => deletion.Files.Execute(progress,
            canWrite: () => !IsDisposed && Documents.Revision == snapshot.Revision
                && inputs.IsCurrent() && references.Files.IsCurrent() && validation.IsCurrent()));
        if (!result.Success)
            throw new IOException(result.Details);
        using (EditorDocumentNotificationBatch notifications = Documents.BeginNotificationBatch())
        {
            EditorUiBatch batch = new();
            int completed = 0;
            foreach (PreparedResourceDeletion.Target target in deletion.Targets)
            {
                applyDocumentDeletion(target);
                progress?.Report(new EditorOperationProgress("EDIT_OPERATION_APPLYING", target.Path, ++completed, deletion.Targets.Count));
                await batch.YieldIfNeededAsync();
            }
            completeDocumentDeletion(deletion, notifications);
        }
        await Task.Run(() => validation.AcceptDeleted(deletion.SavedPaths));
        return true;
    }
}
