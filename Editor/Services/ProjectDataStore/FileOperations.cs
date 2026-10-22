using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    public IReadOnlyList<string> GetUnsavedDocumentPaths()
    {
        IEnumerable<string> paths = Documents.ModifiedDocuments.Select(document => document.Path);
        if (projectEnumGenerationPending)
            paths = paths.Concat(generalEnums.GetOutputPaths(GetProjectEnumCatalog()));
        return paths.Distinct(StringComparer.Ordinal).OrderBy(path => path, StringComparer.Ordinal).ToArray();
    }

    public bool IsManagedPath(string path)
    {
        return Documents.FindByPath(path) is not null
            || tryGetDataLocation(path, out string section, out string relative)
                && (section == "Maps" ? Maps.containsMapKey(normalizeJsonKey(relative))
                    : sections[section].ContainsKey(normalizeJsonKey(relative)))
            || Documents.All.Any(document => pathsEqual(document.SavedPath, path)
                || isDocumentInsidePath(document.Path, path))
            || sections["WorldMaps"].Keys.Any(key => pathsEqual(Worlds.getWorldDirectory(key), path));
    }

    public bool TryCopyManagedPath(string sourcePath, string destinationPath, out string? error)
    {
        error = null;
        EditorDocument? source = GetDocumentByPath(sourcePath);
        string? worldKey = sections["WorldMaps"].Keys.FirstOrDefault(key => pathsEqual(Worlds.getWorldDirectory(key), sourcePath));
        if (worldKey is not null)
            source = GetDocument("WorldMaps", worldKey);
        if (source is null)
        {
            if (!IsManagedPath(sourcePath))
                return false;
            error = "Copy managed resource files or complete worlds individually.";
            return true;
        }
        try
        {
            string destination = source.Section == "WorldMaps" ? Path.Combine(destinationPath, "_world.json") : destinationPath;
            string? key = getDataKey(destination);
            if (key is null || !tryGetDataLocation(destination, out string section, out _)
                || section != source.Section || !hasDataFileExtension(section, destination)
                || source.Section == "Maps" && !pathsEqual(Path.GetDirectoryName(source.Path)!, Path.GetDirectoryName(destination)!))
                throw new InvalidOperationException("The resource cannot be copied to this path.");
            List<(string Section, string Key, JsonObject Data)> copies = [(source.Section, key, source.Data!)];
            if (source.Section == "WorldMaps")
            {
                foreach (string child in Worlds.getWorldChildren(source.Key).ToArray())
                {
                    JsonObject data = GetDocument("Maps", child)?.Data
                        ?? throw new InvalidDataException($"The world child could not be read: {child}");
                    copies.Add(("Maps", key + child[source.Key.Length..], data));
                }
            }
            else if (source.Section == "UI")
                copies[0] = (source.Section, key, UiAssetSchema.CloneForCopy(source.Data!, Path.GetFileName(key)));
            foreach ((string copySection, string copyKey, JsonObject _) in copies)
            {
                if (GetDocument(copySection, copyKey) is not null || File.Exists(getSectionDataPath(copySection, copyKey)))
                    throw new InvalidOperationException($"The destination already exists: {getSectionDataPath(copySection, copyKey)}");
            }
            using EditorDocumentTransaction transaction = Documents.BeginTransaction([]);
            foreach ((string copySection, string copyKey, JsonObject data) in copies)
            {
                RecordDocumentSnapshot(copySection, copyKey);
                sections[copySection][copyKey] = data;
                updateDocumentCatalog(copySection, copyKey, copyKey, data);
            }
            CompleteDocumentChanges();
            transaction.Commit();
            refreshModifiedState();
            if (source.Section == "UI")
                UiAssets.NotifyUiAssetsChanged();
        }
        catch (Exception exception) when (exception is IOException or InvalidOperationException or ArgumentException)
        {
            error = exception.Message;
        }
        return true;
    }

    internal static bool isDocumentInsidePath(string documentPath, string directory)
    {
        return documentPath.StartsWith(Path.TrimEndingDirectorySeparator(Path.GetFullPath(directory)) + Path.DirectorySeparatorChar,
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
    }

    public bool TryRenameManagedPath(string oldPath, string newPath, out string? error)
    {
        error = null;
        EditorDocument? document = GetDocumentByPath(oldPath);
        string? worldKey = sections["WorldMaps"].Keys.FirstOrDefault(key => pathsEqual(Worlds.getWorldDirectory(key), oldPath));
        if (worldKey is not null)
            document = GetDocument("WorldMaps", worldKey);
        if (document is null)
        {
            if (!IsManagedPath(oldPath))
                return false;
            error = "Rename managed resource files individually.";
            return true;
        }
        try
        {
            string destination = document.Section == "WorldMaps" ? Path.Combine(newPath, "_world.json") : newPath;
            string? key = getDataKey(destination);
            if (key is null || !tryGetDataLocation(destination, out string section, out _)
                || document.Section != section || !hasDataFileExtension(section, destination)
                || document.Section == "Maps" && !pathsEqual(Path.GetDirectoryName(document.Path)!, Path.GetDirectoryName(destination)!)
                || !RenameDocumentResource(document.Section, document.Key, key))
                error = "The resource cannot be renamed to this path.";
        }
        catch (Exception exception) when (exception is IOException or InvalidOperationException or ArgumentException)
        {
            error = exception.Message;
        }
        return true;
    }

    public bool RenameDocumentResource(string section, string oldKey, string newKey)
    {
        oldKey = normalizeJsonKey(oldKey);
        newKey = normalizeJsonKey(newKey);
        if (newKey.Length == 0 || newKey.Split('/').Any(part => part is ".." or "." || part.Length == 0))
            return false;
        EditorDocument? document = GetDocument(section, oldKey);
        if (document?.Data is not JsonObject data)
            return false;
        return oldKey == newKey || commitResourceChange(section, oldKey, newKey, data);
    }

    public bool TryDeleteManagedPath(string path, out string? error)
    {
        using ProjectResourceDeletionValidation validation = new(ProjectPath);
        return TryDeleteManagedPath(path, validation, out error);
    }

    internal bool TryDeleteManagedPath(string path, ProjectResourceDeletionValidation validation, out string? error)
    {
        error = null;
        EditorDocument? document = GetDocumentByPath(path);
        string? worldKey = sections["WorldMaps"].Keys.FirstOrDefault(key => pathsEqual(Worlds.getWorldDirectory(key), path));
        if (worldKey is not null)
            document = GetDocument("WorldMaps", worldKey);
        if (document is null)
        {
            if (!IsManagedPath(path))
                return false;
            error = "Delete managed resource files or complete worlds individually.";
            return true;
        }
        try
        {
            if (!deleteDocumentResource(document.Section, document.Key, validation))
                error = "The resource could not be deleted.";
        }
        catch (Exception exception) when (exception is IOException or InvalidOperationException or UnauthorizedAccessException)
        {
            error = exception.Message;
        }
        return true;
    }

    public bool DeleteDocumentResource(string section, string key)
    {
        using ProjectResourceDeletionValidation validation = new(ProjectPath);
        return deleteDocumentResource(section, key, validation);
    }

    private bool deleteDocumentResource(string section, string key, ProjectResourceDeletionValidation validation)
    {
        PreparedResourceDeletion? deletion = prepareDocumentDeletion(section, key, validation, null);
        if (deletion is null)
            return false;
        SaveResult result = deletion.Files.Execute(canWrite: validation.IsCurrent);
        if (!result.Success)
            throw new IOException(result.Details);
        using EditorDocumentNotificationBatch notifications = Documents.BeginNotificationBatch();
        foreach (PreparedResourceDeletion.Target target in deletion.Targets)
            applyDocumentDeletion(target);
        completeDocumentDeletion(deletion, notifications);
        validation.AcceptDeleted(deletion.SavedPaths);
        return true;
    }

    private PreparedResourceDeletion? prepareDocumentDeletion(string section, string key,
        ProjectResourceDeletionValidation validation, IProgress<EditorOperationProgress>? progress)
    {
        EditorDocument? document = GetDocument(section, key);
        if (document is null)
            return null;
        List<EditorDocument> targets = [document];
        if (section == "WorldMaps")
        {
            foreach (string child in Worlds.getWorldChildren(key).ToArray())
                targets.Add(GetDocument("Maps", child) ?? throw new InvalidDataException($"Cannot read world child: {child}"));
        }
        assertDocumentsUnreferenced(this, targets.Select(target => target.Path).ToArray());
        string[] savedPaths = targets.Where(target => target.SavedState.InternalData is not null)
            .Select(target => target.SavedPath).ToArray();
        if (savedPaths.Length != 0)
            validation.AssertUnreferenced(savedPaths, progress);
        return prepareDocumentDeletionFiles(section, document, targets, savedPaths);
    }

    private PreparedResourceDeletion prepareDocumentDeletionFiles(string section, EditorDocument document,
        IReadOnlyList<EditorDocument> targets, string[] savedPaths)
    {
        EditorFileSaveBatch batch = new();
        string? deletedDirectory = section == "WorldMaps" && document.SavedState.InternalData is not null
            ? Path.GetDirectoryName(document.SavedPath) : null;
        if (deletedDirectory is not null)
            batch.DeleteDirectory(deletedDirectory);
        foreach (string savedPath in savedPaths)
            if (deletedDirectory is null || !isDocumentInsidePath(savedPath, deletedDirectory))
                batch.Delete(savedPath);
        if (section is "General" or "Animations" or "Particles")
        {
            Dictionary<string, JsonObject> savedGeneral = originData["General"]
                .Where(pair => section != "General" || pair.Key != document.SavedState.Key)
                .ToDictionary(pair => pair.Key, pair => pair.Value, StringComparer.Ordinal);
            ProjectEnumCatalog catalog = new(savedGeneral,
                originData["Animations"].Keys.Where(value => section != "Animations" || value != document.SavedState.Key),
                originData["Particles"].Keys.Where(value => section != "Particles" || value != document.SavedState.Key));
            generalEnums.AddToSaveBatch(batch, savedGeneral, catalog);
        }
        return new PreparedResourceDeletion(section, targets.Select(target => new PreparedResourceDeletion.Target(
            target.Section, target.Key, target.Path, target.SavedState.Key, target.SavedPath)).ToArray(), savedPaths, batch);
    }

    private void applyDocumentDeletion(PreparedResourceDeletion.Target target)
    {
        deletedDocumentPaths.Add(target.Path);
        deletedDocumentPaths.Add(target.SavedPath);
        sections[target.Section].Remove(target.Key);
        originData[target.Section].Remove(target.SavedKey);
        updateDocumentCatalog(target.Section, target.Key, target.Key, null);
        if (Documents.Find(target.Section, target.Key) is EditorDocument document)
            Documents.Remove(document);
    }

    private void completeDocumentDeletion(PreparedResourceDeletion deletion, EditorDocumentNotificationBatch notifications)
    {
        refreshModifiedState();
        notifications.Commit();
        NotifyDataRestored();
        if (deletion.Section == "UI")
            UiAssets.NotifyUiAssetsChanged();
    }

    internal static void assertDocumentsUnreferenced(ProjectDataStore data, IReadOnlyList<string> paths)
    {
        ReferenceIndexService references = data.ReferenceIndex;
        HashSet<string> deleting = paths.Select(path => Path.GetFullPath(path)).ToHashSet(StringComparer.OrdinalIgnoreCase);
        ReferenceImpact impact = references.GetImpactForPaths(paths);
        ReferenceRecord[] incoming = impact.Incoming.Where(reference => !deleting.Contains(references.GetNodePath(reference.Source))).ToArray();
        if (incoming.Length != 0)
            throw new InvalidOperationException(LocaleService.Get("DOCUMENT_DELETE_REFERENCED") + Environment.NewLine
                + string.Join(Environment.NewLine, incoming.Select(reference => references.GetNodePath(reference.Source) + ": " + reference.Path)));
    }

}
