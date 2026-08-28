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
    public void Reload()
    {
        loadAll();
        NotifyAllMapPreviewsChanged();
        DataChanged?.Invoke(this, EventArgs.Empty);
        UiAssetsChanged?.Invoke(this, EventArgs.Empty);
        DataReloaded?.Invoke(this, EventArgs.Empty);
    }

    public IReadOnlyList<string> GetModifiedBlueprintKeys()
    {
        IReadOnlyDictionary<string, JsonObject> current = sections["Blueprints"].Data;
        IReadOnlyDictionary<string, JsonObject> origin = originData["Blueprints"];
        return current.Keys
            .Where(key => !origin.TryGetValue(key, out JsonObject? originValue)
                || !current.TryGetValue(key, out JsonObject? currentValue)
                || !nodesEqual(currentValue, originValue))
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public IReadOnlyList<string> GetModifiedUiKeys()
    {
        IReadOnlyDictionary<string, JsonObject> current = sections["UI"].Data;
        IReadOnlyDictionary<string, JsonObject> origin = originData["UI"];
        return current.Keys
            .Union(origin.Keys, StringComparer.Ordinal)
            .Where(key => !origin.TryGetValue(key, out JsonObject? originValue)
                || !current.TryGetValue(key, out JsonObject? currentValue)
                || !nodesEqual(currentValue, originValue))
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public IReadOnlyList<string> GetUiAssetKeysForMove()
    {
        return collectUiAssetKeysAcrossHistory()
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public bool CreateUiAsset(string key, JsonObject? asset = null)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        Dictionary<string, JsonObject> data = sections["UI"].Data;
        if (dataKey.Length == 0 || data.ContainsKey(dataKey))
            return false;
        JsonObject value;
        if (asset is null)
        {
            (int width, int height) = getGameSize();
            value = UiAssetSchema.CreateDefaultAsset(normalizedKey, width, height);
        }
        else
        {
            value = (JsonObject)asset.DeepClone();
            value["type"] = UiAssetSchema.UiAssetType;
        }
        RecordSnapshot();
        data[dataKey] = value;
        refreshModifiedState();
        UiAssetsChanged?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool UpdateUiAsset(string key, JsonObject asset)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        Dictionary<string, JsonObject> data = sections["UI"].Data;
        if (!data.TryGetValue(dataKey, out JsonObject? current)
            || !isUiDataType(current, UiAssetSchema.UiAssetType))
        {
            return false;
        }
        JsonObject value = (JsonObject)asset.DeepClone();
        value["type"] = UiAssetSchema.UiAssetType;
        if (JsonNode.DeepEquals(current, value))
            return false;
        RecordSnapshot();
        data[dataKey] = value;
        refreshModifiedState();
        UiAssetsChanged?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool RenameUiAsset(string oldKey, string newKey)
    {
        string normalizedOldKey = UiAssetSchema.NormalizeAssetKey(oldKey);
        string normalizedNewKey = UiAssetSchema.NormalizeAssetKey(newKey);
        string oldDataKey = UiAssetSchema.ToAssetDataKey(normalizedOldKey);
        string newDataKey = UiAssetSchema.ToAssetDataKey(normalizedNewKey);
        if (normalizedOldKey.Length == 0
            || normalizedNewKey.Length == 0
            || !sections["UI"].Data.TryGetValue(oldDataKey, out JsonObject? source)
            || !isUiDataType(source, UiAssetSchema.UiAssetType))
        {
            return false;
        }
        return renameDataEntry("UI", oldDataKey, newDataKey);
    }

    public bool DeleteUiAsset(string key)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        return normalizedKey.Length != 0
            && sections["UI"].Data.TryGetValue(dataKey, out JsonObject? source)
            && isUiDataType(source, UiAssetSchema.UiAssetType)
            && deleteDataEntry("UI", dataKey);
    }

    public string? CopyUiAsset(string key)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        Dictionary<string, JsonObject> data = sections["UI"].Data;
        if (!data.TryGetValue(dataKey, out JsonObject? source)
            || !isUiDataType(source, UiAssetSchema.UiAssetType))
        {
            return null;
        }
        string copyKey = normalizedKey + " (copy)";
        if (data.ContainsKey(UiAssetSchema.ToAssetDataKey(copyKey)))
        {
            int index = 1;
            while (data.ContainsKey(UiAssetSchema.ToAssetDataKey($"{copyKey}_{index}")))
                index += 1;
            copyKey = $"{copyKey}_{index}";
        }
        return CopyUiAsset(normalizedKey, copyKey) ? copyKey : null;
    }

    public bool CopyUiAsset(string sourceKey, string destinationKey)
    {
        string normalizedSourceKey = UiAssetSchema.NormalizeAssetKey(sourceKey);
        string normalizedDestinationKey = UiAssetSchema.NormalizeAssetKey(destinationKey);
        string sourceDataKey = UiAssetSchema.ToAssetDataKey(normalizedSourceKey);
        string destinationDataKey = UiAssetSchema.ToAssetDataKey(normalizedDestinationKey);
        Dictionary<string, JsonObject> data = sections["UI"].Data;
        if (normalizedSourceKey.Length == 0
            || normalizedDestinationKey.Length == 0
            || data.ContainsKey(destinationDataKey)
            || !data.TryGetValue(sourceDataKey, out JsonObject? source)
            || !isUiDataType(source, UiAssetSchema.UiAssetType))
        {
            return false;
        }
        JsonObject copy = UiAssetSchema.CloneForCopy(
            source,
            Path.GetFileName(normalizedDestinationKey));
        return CreateUiAsset(normalizedDestinationKey, copy);
    }

    public bool CreateBlueprint(string key, JsonObject blueprint)
    {
        string normalizedKey = normalizeJsonKey(key);
        Dictionary<string, JsonObject> data = sections["Blueprints"].Data;
        if (normalizedKey.Length == 0 || data.ContainsKey(normalizedKey))
            return false;
        RecordSnapshot();
        data[normalizedKey] = (JsonObject)blueprint.DeepClone();
        refreshModifiedState();
        return true;
    }

    public bool RenameBlueprint(string oldKey, string newKey)
    {
        return renameDataEntry("Blueprints", oldKey, newKey);
    }

    public bool UpdateBlueprint(string key, JsonObject blueprint)
    {
        string normalizedKey = normalizeJsonKey(key);
        Dictionary<string, JsonObject> data = sections["Blueprints"].Data;
        if (!data.TryGetValue(normalizedKey, out JsonObject? current))
            return false;
        JsonObject value = (JsonObject)blueprint.DeepClone();
        value.Remove("type");
        if (JsonNode.DeepEquals(current, value))
            return false;
        RecordSnapshot();
        data[normalizedKey] = value;
        refreshModifiedState();
        return true;
    }

    public bool DeleteBlueprint(string key)
    {
        return deleteDataEntry("Blueprints", key);
    }

    public bool CreateCommonFunction(string name, JsonObject? commonFunction = null)
    {
        string key = normalizeJsonKey(name);
        Dictionary<string, JsonObject> data = sections["CommonFunctions"].Data;
        if (key.Length == 0 || data.ContainsKey(key))
            return false;
        JsonObject value = commonFunction is null
            ? new JsonObject
            {
                ["parent"] = null,
                ["nodeGraph"] = new JsonObject
                {
                    ["common"] = new JsonObject
                    {
                        ["nodes"] = new JsonArray(),
                        ["links"] = new JsonArray(),
                    },
                },
                ["startNodes"] = new JsonObject(),
            }
            : (JsonObject)commonFunction.DeepClone();
        RecordSnapshot();
        data[key] = value;
        refreshModifiedState();
        return true;
    }

    public bool UpdateCommonFunction(string name, JsonObject commonFunction)
    {
        string key = normalizeJsonKey(name);
        Dictionary<string, JsonObject> data = sections["CommonFunctions"].Data;
        if (!data.TryGetValue(key, out JsonObject? current)
            || JsonNode.DeepEquals(current, commonFunction))
        {
            return false;
        }
        RecordSnapshot();
        data[key] = (JsonObject)commonFunction.DeepClone();
        refreshModifiedState();
        return true;
    }

    public bool RenameCommonFunction(string oldName, string newName)
    {
        return renameDataEntry("CommonFunctions", oldName, newName);
    }

    public bool DeleteCommonFunction(string name)
    {
        return deleteDataEntry("CommonFunctions", name);
    }

    public string? CopyCommonFunction(string name)
    {
        string key = normalizeJsonKey(name);
        if (!sections["CommonFunctions"].Data.TryGetValue(key, out JsonObject? source))
            return null;
        string copyName = key + " (copy)";
        if (sections["CommonFunctions"].Data.ContainsKey(copyName))
        {
            int index = 1;
            while (sections["CommonFunctions"].Data.ContainsKey($"{copyName}_{index}"))
                index += 1;
            copyName = $"{copyName}_{index}";
        }
        return CreateCommonFunction(copyName, source) ? copyName : null;
    }

    public bool RemoveDataPaths(IEnumerable<string> paths)
    {
        string[] deletedPaths = paths.ToArray();
        bool changed = deletedPaths.Any(isDataPath);
        ApplyExternalFileChanges([], [], deletedPaths);
        return changed;
    }

    public bool RenameDataPath(string oldPath, string newPath)
    {
        if (!isDataPath(oldPath) || !isDataPath(newPath))
            return false;
        ApplyExternalFileChanges([], [(oldPath, newPath)], []);
        return true;
    }

    public void ApplyExternalFileChanges(
        IReadOnlyList<string> addedPaths,
        IReadOnlyList<(string OldPath, string NewPath)> movedPaths,
        IReadOnlyList<string> deletedPaths)
    {
        Dictionary<string, string> uiAssetMoves = createUiAssetMoveMap(movedPaths);
        Dictionary<string, Dictionary<string, JsonObject>> currentBefore = cloneAllData();
        Dictionary<string, Dictionary<string, JsonObject>> originBefore = cloneData(originData);
        IReadOnlyList<Dictionary<string, Dictionary<string, JsonObject>>> undoBefore = cloneHistory(undoStack);
        IReadOnlyList<Dictionary<string, Dictionary<string, JsonObject>>> redoBefore = cloneHistory(redoStack);
        Dictionary<string, string> uiFilesBefore =
            new Dictionary<string, string>(StringComparer.Ordinal);
        try
        {
            bool changed = false;
            foreach ((string oldPath, string newPath) in movedPaths)
                changed |= applyExternalMove(oldPath, newPath);
            remapUiAssetReferences(uiAssetMoves);
            if (uiAssetMoves.Count != 0)
            {
                writeUiDiskAssetReferences(uiAssetMoves, uiFilesBefore);
                changed = true;
            }
            foreach (string deletedPath in deletedPaths)
                changed |= applyExternalDelete(deletedPath);
            foreach (string addedPath in addedPaths)
                changed |= applyExternalAdd(addedPath);
            if (!changed)
                return;
            refreshModifiedState();
            if (!sectionEqual(currentBefore["UI"], sections["UI"].Data))
                UiAssetsChanged?.Invoke(this, EventArgs.Empty);
            UndoRedoStateChanged?.Invoke(this, EventArgs.Empty);
        }
        catch (Exception exception)
        {
            originData = cloneData(originBefore);
            restoreHistory(undoStack, undoBefore);
            restoreHistory(redoStack, redoBefore);
            restoreSnapshot(currentBefore, false);
            if (uiFilesBefore.Count != 0)
            {
                try
                {
                    restoreUiOriginAssets(uiFilesBefore);
                }
                catch (IOException restoreException)
                {
                    throw new IOException(
                        exception.Message + Environment.NewLine + restoreException.Message,
                        new AggregateException(exception, restoreException));
                }
            }
            throw;
        }
    }

    public bool ContainsDataPath(string absolutePath, bool directory)
    {
        if (!tryGetDataLocation(absolutePath, out string sectionName, out string relativePath))
            return false;
        if (!directory)
        {
            if (!hasDataFileExtension(sectionName, absolutePath))
                return false;
            string key = Path.ChangeExtension(relativePath, null)!.Replace('\\', '/');
            if (sectionName == "Maps")
                return containsMapKey(key);
            if (sectionName == "WorldMaps")
                return sections["WorldMaps"].Data.ContainsKey(normalizeWorldKey(key));
            return sections[sectionName].Data.ContainsKey(key);
        }
        string prefix = normalizeDataKey(relativePath);
        if (sectionName == "Maps")
        {
            return getMapCatalogEntries().Any(entry =>
                entry.Kind != MapCatalogEntryKind.WorldMap
                && keyMatchesPrefix(entry.Key, prefix));
        }
        return sections[sectionName].Data.Keys.Any(key => keyMatchesPrefix(key, prefix));
    }

    public DataFileInfo? TryLoadDataFile(string absolutePath)
    {
        if (!File.Exists(absolutePath)
            || DataConfig.isAnimationCache(absolutePath)
            || !string.Equals(Path.GetExtension(absolutePath), DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase))
            return null;
        bool dataFile = tryGetDataLocation(
            absolutePath,
            out string sectionName,
            out _);
        DataSection? section = dataFile ? sections[sectionName] : null;
        bool textConfigFile = dataFile && sectionName == "TextConfigs";
        bool uiFile = dataFile && sectionName == "UI";
        if (uiFile && !hasDataFileExtension(sectionName, absolutePath))
            return new DataFileInfo("invalidUiData", getDataKey(absolutePath));
        try
        {
            if (JsonNode.Parse(File.ReadAllText(absolutePath)) is not JsonObject file)
            {
                return textConfigFile
                    ? new DataFileInfo("invalidTextConfig", getDataKey(absolutePath))
                    : uiFile
                        ? new DataFileInfo("invalidUiData", getDataKey(absolutePath))
                    : null;
            }
            string? type = file["type"] is JsonValue typeValue
                && typeValue.TryGetValue<string>(out string? parsedType)
                    ? parsedType
                    : null;
            if (section is not null && !section.AcceptsType(type))
            {
                return textConfigFile
                    ? new DataFileInfo("invalidTextConfig", getDataKey(absolutePath))
                    : uiFile
                        ? new DataFileInfo("invalidUiData", getDataKey(absolutePath))
                    : null;
            }
            string? resolvedType = string.IsNullOrWhiteSpace(type)
                ? section?.ExpectedType
                : type;
            if (string.IsNullOrWhiteSpace(resolvedType)
                && sectionName != "General")
                return null;
            return new DataFileInfo(resolvedType ?? "general", getDataKey(absolutePath));
        }
        catch (JsonException)
        {
            return textConfigFile
                ? new DataFileInfo("invalidTextConfig", getDataKey(absolutePath))
                : uiFile
                    ? new DataFileInfo("invalidUiData", getDataKey(absolutePath))
                : null;
        }
    }

}

