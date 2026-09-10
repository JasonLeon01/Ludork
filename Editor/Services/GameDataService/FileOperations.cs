using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    public IReadOnlyList<string> GetUnsavedDocumentPaths()
    {
        IEnumerable<string> paths = Documents.ModifiedDocuments.Select(document => document.Path);
        if (generalDataGenerationPending)
            paths = paths.Append(generalEnums.RuntimePath);
        return paths.Distinct(StringComparer.Ordinal).OrderBy(path => path, StringComparer.Ordinal).ToArray();
    }

    public bool IsManagedPath(string path)
    {
        return GetDocumentByPath(path) is not null
            || Documents.All.Any(document => pathsEqual(document.SavedPath, path)
                || isDocumentInsidePath(document.Path, path))
            || sections["WorldMaps"].Data.Keys.Any(key => pathsEqual(getWorldDirectory(key), path));
    }

    public bool TryCopyManagedPath(string sourcePath, string destinationPath, out string? error)
    {
        error = null;
        EditorDocument? source = GetDocumentByPath(sourcePath);
        string? worldKey = sections["WorldMaps"].Data.Keys.FirstOrDefault(key => pathsEqual(getWorldDirectory(key), sourcePath));
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
                foreach (string child in getWorldChildren(source.Key).ToArray())
                {
                    JsonObject data = GetDocument("Maps", child)?.Data
                        ?? throw new InvalidDataException($"The world child could not be read: {child}");
                    copies.Add(("Maps", key + child[source.Key.Length..], data));
                }
            }
            else if (source.Section == "UI")
                copies[0] = (source.Section, key, UiAssetSchema.CloneForCopy(source.Data!, Path.GetFileName(UiAssetSchema.ToLogicalAssetKey(key))));
            foreach ((string copySection, string copyKey, JsonObject _) in copies)
            {
                if (GetDocument(copySection, copyKey) is not null || File.Exists(getSectionDataPath(copySection, copyKey)))
                    throw new InvalidOperationException($"The destination already exists: {getSectionDataPath(copySection, copyKey)}");
            }
            using EditorDocumentTransaction transaction = Documents.BeginTransaction([]);
            foreach ((string copySection, string copyKey, JsonObject data) in copies)
            {
                RecordDocumentSnapshot(copySection, copyKey);
                sections[copySection].Data[copyKey] = data;
                updateDocumentCatalog(copySection, copyKey, copyKey, data);
            }
            CompleteDocumentChanges();
            transaction.Commit();
            refreshModifiedState();
            if (source.Section == "UI")
                NotifyUiAssetsChanged();
        }
        catch (Exception exception) when (exception is IOException or InvalidOperationException or ArgumentException)
        {
            error = exception.Message;
        }
        return true;
    }

    private static bool isDocumentInsidePath(string documentPath, string directory)
    {
        return documentPath.StartsWith(Path.TrimEndingDirectorySeparator(Path.GetFullPath(directory)) + Path.DirectorySeparatorChar,
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
    }

    public bool TryRenameManagedPath(string oldPath, string newPath, out string? error)
    {
        error = null;
        EditorDocument? document = GetDocumentByPath(oldPath);
        string? worldKey = sections["WorldMaps"].Data.Keys.FirstOrDefault(key => pathsEqual(getWorldDirectory(key), oldPath));
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
        error = null;
        EditorDocument? document = GetDocumentByPath(path);
        string? worldKey = sections["WorldMaps"].Data.Keys.FirstOrDefault(key => pathsEqual(getWorldDirectory(key), path));
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
            if (!DeleteDocumentResource(document.Section, document.Key))
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
        EditorDocument? document = GetDocument(section, key);
        if (document is null)
            return false;
        List<EditorDocument> targets = [document];
        if (section == "WorldMaps")
        {
            foreach (string child in getWorldChildren(key).ToArray())
                targets.Add(GetDocument("Maps", child) ?? throw new InvalidDataException($"Cannot read world child: {child}"));
        }
        assertDocumentsUnreferenced(this, targets.Select(target => target.Path).ToArray());
        string[] savedPaths = targets.Where(target => target.SavedState.InternalData is not null)
            .Select(target => target.SavedPath).ToArray();
        if (savedPaths.Length != 0)
        {
            using GameDataService diskData = new(ProjectPath);
            assertDocumentsUnreferenced(diskData, savedPaths);
        }
        EditorFileSaveBatch batch = new();
        if (section == "WorldMaps" && document.SavedState.InternalData is not null)
            batch.DeleteDirectory(Path.GetDirectoryName(document.SavedPath)!);
        foreach (string savedPath in savedPaths)
            batch.Delete(savedPath);
        if (section == "General")
        {
            Dictionary<string, JsonObject> savedGeneral = originData["General"]
                .Where(pair => pair.Key != document.SavedState.Key)
                .ToDictionary(pair => pair.Key, pair => pair.Value, StringComparer.Ordinal);
            foreach (KeyValuePair<string, byte[]> output in generalEnums.PrepareOutputs(savedGeneral))
                batch.Write(output.Key, output.Value);
        }
        SaveResult result = batch.Execute();
        if (!result.Success)
            throw new IOException(result.Details);
        using EditorDocumentNotificationBatch notifications = Documents.BeginNotificationBatch();
        foreach (EditorDocument target in targets)
        {
            deletedDocumentPaths.Add(target.Path);
            deletedDocumentPaths.Add(target.SavedPath);
            sections[target.Section].Data.Remove(target.Key);
            originData[target.Section].Remove(target.SavedState.Key);
            updateDocumentCatalog(target.Section, target.Key, target.Key, null);
            Documents.Remove(target);
        }
        refreshModifiedState();
        notifications.Commit();
        NotifyDataRestored();
        if (section == "UI")
            NotifyUiAssetsChanged();
        return true;
    }

    private static void assertDocumentsUnreferenced(GameDataService data, IReadOnlyList<string> paths)
    {
        LuaMetadataService metadata = new(data.ProjectPath);
        using BlueprintClassResolver resolver = new(data, metadata);
        using ReferenceIndexService references = new(data, metadata, resolver);
        HashSet<string> deleting = paths.Select(path => Path.GetFullPath(path)).ToHashSet(StringComparer.OrdinalIgnoreCase);
        ReferenceImpact impact = references.GetImpactForPaths(paths);
        ReferenceRecord[] incoming = impact.Incoming.Where(reference => !deleting.Contains(references.GetNodePath(reference.Source))).ToArray();
        if (incoming.Length != 0)
            throw new InvalidOperationException(LocaleService.Get("DOCUMENT_DELETE_REFERENCED") + Environment.NewLine
                + string.Join(Environment.NewLine, incoming.Select(reference => references.GetNodePath(reference.Source) + ": " + reference.Path)));
    }
}
