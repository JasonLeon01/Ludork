using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading.Tasks;
using Ludork.Models;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    private void loadAll()
    {
        invalidLoadPaths.Clear();
        pendingWorldDirectoryMoves.Clear();
        mapActorTagIndexes.Clear();
        mapAccessOrder.Clear();
        mapLoadedBytes.Clear();
        nextMapAccessOrder = 0;
        foreach (KeyValuePair<string, DataSection> pair in sections)
        {
            pair.Value.Data.Clear();
            if (!pair.Value.Persist || pair.Key is "Maps" or "WorldMaps")
                continue;
            string root = Path.Combine(ProjectPath, "Data", pair.Key);
            if (!Directory.Exists(root))
                continue;
            foreach (string path in Directory.EnumerateFiles(root, "*.json", SearchOption.AllDirectories)
                         .Where(path => !DataConfig.isAnimationCache(path))
                         .OrderBy(path => path, StringComparer.Ordinal))
            {
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
                    pair.Value.Data[key] = data;
                }
                catch (JsonException)
                {
                    invalidLoadPaths.Add(Path.GetRelativePath(ProjectPath, path));
                }
            }
        }
        loadMapsAndWorldMaps();
        originData = cloneAllData();
        generalDataGenerationPending = !File.Exists(generalEnums.RuntimePath) || !File.Exists(generalEnums.StubPath)
            || !File.Exists(generalEnums.TypesRuntimePath) || !File.Exists(generalEnums.TypesStubPath);
        isModified = generalDataGenerationPending;
        clearHistoryGesture();
        InitializeDocuments();
        UndoRedoStateChanged?.Invoke(this, EventArgs.Empty);
    }

    private string? getDataKey(string absolutePath)
    {
        string fullPath = Path.GetFullPath(absolutePath);
        if (tryGetMapsRelativePath(fullPath, out string mapsRelative))
        {
            if (string.Equals(Path.GetFileName(fullPath), "_world.json", StringComparison.OrdinalIgnoreCase))
                return Path.GetDirectoryName(mapsRelative)?.Replace('\\', '/');
            return Path.ChangeExtension(mapsRelative, null)?.Replace('\\', '/');
        }
        foreach (KeyValuePair<string, DataSection> pair in sections)
        {
            if (!pair.Value.Persist || pair.Key is "Maps" or "WorldMaps")
                continue;
            string root = Path.Combine(ProjectPath, "Data", pair.Key);
            if (fullPath.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                return Path.ChangeExtension(Path.GetRelativePath(root, fullPath), null)?.Replace('\\', '/');
        }
        return null;
    }

    private bool applyExternalDelete(string path)
    {
        if (!tryGetDataLocation(path, out string sectionName, out string relativePath))
            return false;
        if (hasDataFileExtension(sectionName, path))
        {
            string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
            bool removed = removeDataKey(sectionName, key);
            return removeDataPrefix(sectionName, normalizeDataKey(relativePath)) || removed;
        }
        return removeDataPrefix(sectionName, normalizeDataKey(relativePath));
    }

    private bool applyExternalAdd(string path)
    {
        if (Directory.Exists(path))
        {
            bool changed = false;
            foreach (string filePath in Directory.EnumerateFiles(path, "*.json", SearchOption.AllDirectories)
                         .Where(filePath => !DataConfig.isAnimationCache(filePath)))
            {
                changed |= applyExternalAdd(filePath);
            }
            return changed;
        }
        if (!File.Exists(path)
            || DataConfig.isAnimationCache(path)
            || !tryGetDataLocation(path, out string sectionName, out string relativePath)
            || !hasDataFileExtension(sectionName, path))
        {
            return false;
        }
        JsonObject? data = readExternalDataFile(path, sections[sectionName]);
        if (data is null)
            return false;
        string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
        installExternalDocument(sectionName, key, data);
        return true;
    }

    private JsonObject? readExternalDataFile(string path, DataSection section)
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

    private bool removeDataKey(string sectionName, string key)
    {
        if (Documents.Find(sectionName, key) is EditorDocument document)
            Documents.Remove(document);
        bool changed = sections[sectionName].Data.Remove(key);
        changed |= originData[sectionName].Remove(key);
        return changed;
    }

    private bool removeDataPrefix(string sectionName, string prefix)
    {
        foreach (EditorDocument document in Documents.All.Where(document => document.Section == sectionName
                     && keyMatchesPrefix(document.Key, prefix)).ToArray())
            Documents.Remove(document);
        bool changed = removeDataPrefix(sections[sectionName].Data, prefix);
        changed |= removeDataPrefix(originData[sectionName], prefix);
        return changed;
    }

    private static bool removeDataPrefix(Dictionary<string, JsonObject> data, string prefix)
    {
        string[] keys = data.Keys.Where(key => keyMatchesPrefix(key, prefix)).ToArray();
        foreach (string key in keys)
            data.Remove(key);
        return keys.Length != 0;
    }

    private void installExternalDocument(string sectionName, string key, JsonObject data)
    {
        EditorDocument? existing = Documents.Find(sectionName, key);
        if (existing?.IsModified == true)
            throw new InvalidOperationException($"The file has unsaved editor changes: {existing.Path}");
        if (existing is not null)
            Documents.Remove(existing);
        sections[sectionName].Data[key] = data;
        originData[sectionName][key] = (JsonObject)data.DeepClone();
        RegisterLoadedDocument(sectionName, key);
    }

    private bool tryGetDataLocation(string path, out string sectionName, out string relativePath)
    {
        string fullPath = Path.GetFullPath(path);
        if (tryGetMapsRelativePath(fullPath, out string mapsRelative))
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
        foreach (KeyValuePair<string, DataSection> pair in sections)
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

    private static bool keyMatchesPrefix(string key, string prefix)
    {
        return prefix.Length == 0
            || string.Equals(key, prefix, StringComparison.Ordinal)
            || key.StartsWith(prefix + "/", StringComparison.Ordinal);
    }


}

