using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    internal void loadAll()
    {
        BreakHistoryGesture();
        lock (invalidLoadLock)
        {
            invalidLoadPaths.Clear();
            invalidLoadErrors.Clear();
        }
        Worlds.ClearPendingDirectoryMoves();
        Maps.ResetCacheMetadata();
        List<(string Section, string Root, string Path)> files = [];
        foreach ((string section, EditorDocumentCollection collection) in sections)
        {
            collection.Clear();
            if (!collection.Persist || section is "Maps" or "WorldMaps")
                continue;
            string root = getSectionRoot(section);
            if (Directory.Exists(root))
                files.AddRange(Directory.EnumerateFiles(root, "*.json", SearchOption.AllDirectories)
                    .Where(path => !DataConfig.isAnimationCache(path))
                    .Order(StringComparer.Ordinal).Select(path => (section, root, path)));
        }
        (string Key, JsonObject? Data, string? Error)[] results = readLoadFiles(files,
            input => input.Path, (input, file) => prepareSectionFile(input.Section, input.Root, file));
        for (int index = 0; index < files.Count; index++)
        {
            (string key, JsonObject? data, string? error) = results[index];
            if (data is not null)
                sections[files[index].Section][key] = data;
            else if (error == string.Empty)
                addInvalidLoadPath(files[index].Path);
            else if (error is not null)
                rejectDataFile(files[index].Path, error);
        }
        rejectInvalidBlueprints();
        Worlds.loadMapsAndWorldMaps();
        MapOrder.load();
        originData = cloneAllData();
        projectEnumGenerationPending = generalEnums.NeedsGeneration(GetProjectEnumCatalog());
        isModified = projectEnumGenerationPending;
        InitializeDocuments();
        UndoRedoStateChanged?.Invoke(this, EventArgs.Empty);
    }

    private (string Key, JsonObject? Data, string? Error) prepareSectionFile(
        string section, string root, ProjectLoadFile file)
    {
        string key = Path.ChangeExtension(Path.GetRelativePath(root, file.Path), null)!.Replace('\\', '/');
        if (section == "UI" && !hasDataFileExtension(section, file.Path))
            return (key, null, string.Empty);
        bool reportTypeError = section is "TextConfigs" or "UI" or "Subtitles" or "Blueprints" or "Materials";
        try
        {
            if (ProjectLoadFile.ParseObject(file.ReadContent()) is not JsonObject data)
                return (key, null, reportTypeError ? "Data must be a JSON object" : null);
            EditorDocumentCollection collection = sections[section];
            string? type = data["type"] is JsonValue value && value.TryGetValue(out string? parsedType)
                ? parsedType : null;
            if (!collection.AcceptsType(type))
                return (key, null, reportTypeError ? "Unexpected data file type" : null);
            if (!collection.PreserveType)
                data.Remove("type");
            if (section == "Particles" && ParticleAssetSchema.Validate(data, key).Count != 0)
                return (key, null, string.Empty);
            return (key, data, null);
        }
        catch (JsonException exception)
        {
            return (key, null, exception.Message);
        }
    }

    internal string? getDataKey(string absolutePath)
    {
        string fullPath = Path.GetFullPath(absolutePath);
        if (Maps.tryGetMapsRelativePath(fullPath, out string mapsRelative))
        {
            if (MapOrderService.IsOrderFile(fullPath))
                return null;
            if (string.Equals(Path.GetFileName(fullPath), "_world.json", StringComparison.OrdinalIgnoreCase))
                return Path.GetDirectoryName(mapsRelative)?.Replace('\\', '/');
            return Path.ChangeExtension(mapsRelative, null)?.Replace('\\', '/');
        }
        foreach (KeyValuePair<string, EditorDocumentCollection> pair in sections)
        {
            if (!pair.Value.Persist || pair.Key is "Maps" or "WorldMaps")
                continue;
            string root = getSectionRoot(pair.Key);
            if (fullPath.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                return Path.ChangeExtension(Path.GetRelativePath(root, fullPath), null)?.Replace('\\', '/');
        }
        return null;
    }

    internal JsonObject? readExternalDataFile(string path, EditorDocumentCollection section)
    {
        try
        {
            if (JsonNode.Parse(File.ReadAllText(path)) is not JsonObject data)
                return null;
            string? type = data["type"] is JsonValue typeValue
                && typeValue.TryGetValue<string>(out string? parsedType)
                    ? parsedType
                    : null;
            if (!section.AcceptsType(type))
                return null;
            if (!section.PreserveType)
                data.Remove("type");
            return data;
        }
        catch (JsonException)
        {
            return null;
        }
        catch (IOException)
        {
            return null;
        }
        catch (UnauthorizedAccessException)
        {
            return null;
        }
    }

    internal bool removeDataKey(string sectionName, string key)
    {
        if (Documents.Find(sectionName, key) is EditorDocument document)
            Documents.Remove(document);
        else
            updateDocumentCatalog(sectionName, key, key, null);
        bool changed = sections[sectionName].Remove(key);
        changed |= originData[sectionName].Remove(key);
        return changed;
    }

    internal void installExternalDocument(string sectionName, string key, JsonObject data)
    {
        removeDataKey(sectionName, key);
        EditorDocument document = RegisterLoadedDocument(sectionName, key);
        Documents.Restore(document, new EditorDocumentState(sectionName, key, document.Path, data));
        Documents.MarkSaved(document);
        originData[sectionName][key] = (JsonObject)data.DeepClone();
    }

    internal bool tryGetDataLocation(string path, out string sectionName, out string relativePath)
    {
        string fullPath = Path.GetFullPath(path);
        if (Maps.tryGetMapsRelativePath(fullPath, out string mapsRelative))
        {
            if (MapOrderService.IsOrderFile(fullPath))
            {
                sectionName = string.Empty;
                relativePath = string.Empty;
                return false;
            }
            if (string.Equals(Path.GetFileName(fullPath), "_world.json", StringComparison.OrdinalIgnoreCase))
            {
                sectionName = "WorldMaps";
                relativePath = Path.GetDirectoryName(mapsRelative) ?? string.Empty;
            }
            else
            {
                sectionName = "Maps";
                relativePath = mapsRelative;
            }
            return true;
        }
        foreach (KeyValuePair<string, EditorDocumentCollection> pair in sections)
        {
            if (!pair.Value.Persist || pair.Key is "Maps" or "WorldMaps")
                continue;
            string root = Path.GetFullPath(getSectionRoot(pair.Key));
            string relative = Path.GetRelativePath(root, fullPath);
            if (Path.IsPathRooted(relative)
                || relative == ".."
                || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal))
            {
                continue;
            }
            sectionName = pair.Key;
            relativePath = relative == "." ? string.Empty : relative;
            return true;
        }
        sectionName = string.Empty;
        relativePath = string.Empty;
        return false;
    }

    internal static bool keyMatchesPrefix(string key, string prefix)
    {
        return prefix.Length == 0
            || string.Equals(key, prefix, StringComparison.Ordinal)
            || key.StartsWith(prefix + "/", StringComparison.Ordinal);
    }

}
