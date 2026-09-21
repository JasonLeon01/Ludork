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
        invalidLoadPaths.Clear();
        Worlds.ClearPendingDirectoryMoves();
        Maps.ResetCacheMetadata();
        foreach (KeyValuePair<string, EditorDocumentCollection> pair in sections)
        {
            pair.Value.Clear();
            if (!pair.Value.Persist || pair.Key is "Maps" or "WorldMaps")
                continue;
            string root = Path.Combine(ProjectPath, "Data", pair.Key);
            if (!Directory.Exists(root))
                continue;
            foreach (string path in Directory.EnumerateFiles(root, "*.json", SearchOption.AllDirectories)
                         .Where(path => !DataConfig.isAnimationCache(path))
                         .OrderBy(path => path, StringComparer.Ordinal))
            {
                reportDataRead(path);
                if (pair.Key == "UI" && !hasDataFileExtension(pair.Key, path))
                {
                    invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                    continue;
                }
                try
                {
                    if (JsonNode.Parse(File.ReadAllText(path)) is not JsonObject data)
                    {
                        if (pair.Key is "TextConfigs" or "UI")
                            invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                        continue;
                    }
                    string? type = data["type"] is JsonValue typeValue
                        && typeValue.TryGetValue<string>(out string? parsedType)
                            ? parsedType
                            : null;
                    if (!pair.Value.AcceptsType(type))
                    {
                        if (pair.Key is "TextConfigs" or "UI")
                            invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                        continue;
                    }
                    if (!pair.Value.PreserveType)
                        data.Remove("type");
                    string relativePath = Path.GetRelativePath(root, path);
                    string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
                    if (pair.Key == "Particles" && ParticleAssetSchema.Validate(data, key).Count != 0)
                    {
                        invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                        continue;
                    }
                    pair.Value[key] = data;
                }
                catch (JsonException)
                {
                    invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                }
            }
        }
        Worlds.loadMapsAndWorldMaps();
        originData = cloneAllData();
        generalDataGenerationPending = !File.Exists(generalEnums.RuntimePath) || !File.Exists(generalEnums.StubPath)
            || !File.Exists(generalEnums.TypesRuntimePath) || !File.Exists(generalEnums.TypesStubPath);
        isModified = generalDataGenerationPending;
        clearHistoryGesture();
        InitializeDocuments();
        UndoRedoStateChanged?.Invoke(this, EventArgs.Empty);
    }

    internal string? getDataKey(string absolutePath)
    {
        string fullPath = Path.GetFullPath(absolutePath);
        if (Maps.tryGetMapsRelativePath(fullPath, out string mapsRelative))
        {
            if (string.Equals(Path.GetFileName(fullPath), "_world.json", StringComparison.OrdinalIgnoreCase))
                return Path.GetDirectoryName(mapsRelative)?.Replace('\\', '/');
            return Path.ChangeExtension(mapsRelative, null)?.Replace('\\', '/');
        }
        foreach (KeyValuePair<string, EditorDocumentCollection> pair in sections)
        {
            if (!pair.Value.Persist || pair.Key is "Maps" or "WorldMaps")
                continue;
            string root = Path.Combine(ProjectPath, "Data", pair.Key);
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
            string root = Path.GetFullPath(Path.Combine(ProjectPath, "Data", pair.Key));
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
