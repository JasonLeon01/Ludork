using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class UiAssetDataService
{
    public IReadOnlyList<string> GetModifiedUiKeys()
    {
        return store.Documents.ModifiedDocuments.Where(document => document.Section == "UI" && document.Exists)
            .Select(document => document.Key)
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public IReadOnlyList<string> GetUiAssetKeysForMove()
    {
        return uiDocuments.Keys
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public bool CreateUiAsset(string key, JsonObject? asset = null)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        EditorDocumentCollection data = uiDocuments;
        if (!store.canCreateDocument("UI", dataKey))
            return false;
        JsonObject value;
        if (asset is null)
        {
            (int width, int height) = store.Configs.getGameSize();
            value = UiAssetSchema.CreateDefaultAsset(normalizedKey, width, height);
        }
        else
        {
            value = (JsonObject)asset.DeepClone();
            value["type"] = UiAssetSchema.UiAssetType;
        }
        uiDocuments.RecordChange(dataKey);
        data[dataKey] = value;
        store.refreshModifiedState();
        NotifyUiAssetsChanged();
        return true;
    }

    public bool UpdateUiAsset(string key, JsonObject asset)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        EditorDocumentCollection data = uiDocuments;
        if (!data.TryGetValue(dataKey, out JsonObject? current)
            || !ProjectDataStore.isUiDataType(current, UiAssetSchema.UiAssetType))
        {
            return false;
        }
        JsonObject value = (JsonObject)asset.DeepClone();
        value["type"] = UiAssetSchema.UiAssetType;
        if (JsonNode.DeepEquals(current, value))
            return false;
        uiDocuments.RecordChange(dataKey);
        data[dataKey] = value;
        store.refreshModifiedState();
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
            || !uiDocuments.TryGetValue(oldDataKey, out JsonObject? source)
            || !ProjectDataStore.isUiDataType(source, UiAssetSchema.UiAssetType))
        {
            return false;
        }
        return store.renameDataEntry("UI", oldDataKey, newDataKey);
    }

    public bool DeleteUiAsset(string key)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        return normalizedKey.Length != 0
            && uiDocuments.TryGetValue(dataKey, out JsonObject? source)
            && ProjectDataStore.isUiDataType(source, UiAssetSchema.UiAssetType)
            && store.deleteDataEntry("UI", dataKey);
    }

    public string? CopyUiAsset(string key)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        string dataKey = UiAssetSchema.ToAssetDataKey(normalizedKey);
        EditorDocumentCollection data = uiDocuments;
        if (!data.TryGetValue(dataKey, out JsonObject? source)
            || !ProjectDataStore.isUiDataType(source, UiAssetSchema.UiAssetType))
        {
            return null;
        }
        string copyKey = normalizedKey + " (copy)";
        if (!store.canCreateDocument("UI", UiAssetSchema.ToAssetDataKey(copyKey)))
        {
            int index = 1;
            while (!store.canCreateDocument("UI", UiAssetSchema.ToAssetDataKey($"{copyKey}_{index}")))
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
        EditorDocumentCollection data = uiDocuments;
        if (normalizedSourceKey.Length == 0
            || normalizedDestinationKey.Length == 0
            || !store.canCreateDocument("UI", destinationDataKey)
            || !data.TryGetValue(sourceDataKey, out JsonObject? source)
            || !ProjectDataStore.isUiDataType(source, UiAssetSchema.UiAssetType))
        {
            return false;
        }
        JsonObject copy = UiAssetSchema.CloneForCopy(
            source,
            Path.GetFileName(normalizedDestinationKey));
        return CreateUiAsset(normalizedDestinationKey, copy);
    }

}
