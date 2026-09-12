using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using Ludork.Models;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    public void Reload()
    {
        using EditorDocumentNotificationBatch notifications = Documents.BeginNotificationBatch();
        loadAll();
        Documents.PublishReset();
        notifications.Commit();
        NotifyAllMapPreviewsChanged();
        NotifyUiAssetsChanged();
        DataReloaded?.Invoke(this, EventArgs.Empty);
    }

    public IReadOnlyList<string> GetModifiedBlueprintKeys()
    {
        return Documents.ModifiedDocuments.Where(document => document.Section == "Blueprints" && document.Exists)
            .Select(document => document.Key)
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public IReadOnlyList<string> GetModifiedUiKeys()
    {
        return Documents.ModifiedDocuments.Where(document => document.Section == "UI" && document.Exists)
            .Select(document => document.Key)
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public IReadOnlyList<string> GetUiAssetKeysForMove()
    {
        return sections["UI"].Data.Keys
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public bool CreateUiAsset(string key, JsonObject? asset = null)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        Dictionary<string, JsonObject> data = sections["UI"].Data;
        if (!canCreateDocument("UI", dataKey))
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
        RecordDocumentSnapshot("UI", dataKey);
        data[dataKey] = value;
        refreshModifiedState();
        NotifyUiAssetsChanged();
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
        RecordDocumentSnapshot("UI", dataKey);
        data[dataKey] = value;
        refreshModifiedState();
        NotifyUiAssetsChanged();
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
        if (!canCreateDocument("UI", UiAssetSchema.ToAssetDataKey(copyKey)))
        {
            int index = 1;
            while (!canCreateDocument("UI", UiAssetSchema.ToAssetDataKey($"{copyKey}_{index}")))
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
            || !canCreateDocument("UI", destinationDataKey)
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
        if (!canCreateDocument("Blueprints", normalizedKey))
            return false;
        RecordDocumentSnapshot("Blueprints", key);
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
        RecordDocumentSnapshot("Blueprints", key);
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
        if (!canCreateDocument("CommonFunctions", key))
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
        RecordDocumentSnapshot("CommonFunctions", key);
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
        RecordDocumentSnapshot("CommonFunctions", name);
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
        if (!canCreateDocument("CommonFunctions", copyName))
        {
            int index = 1;
            while (!canCreateDocument("CommonFunctions", $"{copyName}_{index}"))
                index += 1;
            copyName = $"{copyName}_{index}";
        }
        return CreateCommonFunction(copyName, source) ? copyName : null;
    }

    public void ApplyExternalFileChanges(
        IReadOnlyList<string> addedPaths,
        IReadOnlyList<(string OldPath, string NewPath)> movedPaths,
        IReadOnlyList<string> deletedPaths,
        Func<IReadOnlyList<ReferenceRewrite>>? prepareReferenceChanges = null)
    {
        if (addedPaths.Count == 0 && movedPaths.Count == 0 && deletedPaths.Count == 0
            && prepareReferenceChanges is null)
            return;
        foreach ((string oldPath, string newPath) in movedPaths)
        {
            if (IsManagedPath(oldPath))
                throw new InvalidOperationException("Managed resource moves must use the document rename command.");
        }
        bool externalChanges = addedPaths.Count != 0 || movedPaths.Count != 0 || deletedPaths.Count != 0;
        Dictionary<(string Section, string Key), JsonObject?> changes = prepareExternalChanges(addedPaths, movedPaths, deletedPaths);
        applyExternalChanges(changes, externalChanges, prepareReferenceChanges);
        if (addedPaths.Concat(deletedPaths).Any(path => path.Contains("UI", StringComparison.Ordinal)))
            NotifyUiAssetsChanged();
        if (externalChanges)
            DataReloaded?.Invoke(this, EventArgs.Empty);
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

    public DataFileInfo? TryLoadDataFile(string absolutePath) => prepareDataFileRead(absolutePath)();

    public Task<DataFileInfo?> TryLoadDataFileAsync(string absolutePath, CancellationToken cancellationToken = default)
    {
        Func<DataFileInfo?> read = prepareDataFileRead(absolutePath);
        return Task.Run(read, cancellationToken);
    }

    private Func<DataFileInfo?> prepareDataFileRead(string absolutePath)
    {
        if (DataConfig.isAnimationCache(absolutePath)
            || !string.Equals(Path.GetExtension(absolutePath), DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase))
            return () => null;
        EditorDocument? document = Documents.FindByPath(absolutePath);
        if (document?.InternalData is JsonObject current)
        {
            string? currentType = getString(current["type"]);
            DataFileInfo info = new(currentType ?? sections[document.Section].ExpectedType ?? "general", document.Key);
            return () => info;
        }
        bool dataFile = tryGetDataLocation(
            absolutePath,
            out string sectionName,
            out _);
        DataSection? section = dataFile ? sections[sectionName] : null;
        bool textConfigFile = dataFile && sectionName == "TextConfigs";
        bool uiFile = dataFile && sectionName == "UI";
        string? key = getDataKey(absolutePath);
        if (uiFile && !hasDataFileExtension(sectionName, absolutePath))
            return () => new DataFileInfo("invalidUiData", key);
        return () =>
        {
            if (!File.Exists(absolutePath))
                return null;
            try
            {
                if (JsonNode.Parse(File.ReadAllText(absolutePath)) is not JsonObject file)
                {
                    return textConfigFile
                        ? new DataFileInfo("invalidTextConfig", key)
                        : uiFile
                            ? new DataFileInfo("invalidUiData", key)
                        : null;
                }
                string? type = file["type"] is JsonValue typeValue
                    && typeValue.TryGetValue<string>(out string? parsedType)
                        ? parsedType
                        : null;
                if (section is not null && !section.AcceptsType(type))
                {
                    return textConfigFile
                        ? new DataFileInfo("invalidTextConfig", key)
                        : uiFile
                            ? new DataFileInfo("invalidUiData", key)
                        : null;
                }
                string? resolvedType = string.IsNullOrWhiteSpace(type)
                    ? section?.ExpectedType
                    : type;
                if (string.IsNullOrWhiteSpace(resolvedType)
                    && sectionName != "General")
                    return null;
                return new DataFileInfo(resolvedType ?? "general", key);
            }
            catch (JsonException)
            {
                return textConfigFile
                    ? new DataFileInfo("invalidTextConfig", key)
                    : uiFile
                        ? new DataFileInfo("invalidUiData", key)
                    : null;
            }
        };
    }

}
