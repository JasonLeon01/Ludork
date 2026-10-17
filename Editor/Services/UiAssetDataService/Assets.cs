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
        EditorDocumentCollection data = uiDocuments;
        if (!store.canCreateDocument("UI", normalizedKey))
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
        uiDocuments.RecordChange(normalizedKey);
        data[normalizedKey] = value;
        store.refreshModifiedState();
        NotifyUiAssetsChanged();
        return true;
    }

    public bool UpdateUiAsset(string key, JsonObject asset)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        EditorDocumentCollection data = uiDocuments;
        if (!data.TryGetValue(normalizedKey, out JsonObject? current)
            || !ProjectDataStore.isUiDataType(current, UiAssetSchema.UiAssetType))
        {
            return false;
        }
        JsonObject value = (JsonObject)asset.DeepClone();
        value["type"] = UiAssetSchema.UiAssetType;
        if (JsonNode.DeepEquals(current, value))
            return false;
        uiDocuments.RecordChange(normalizedKey);
        data[normalizedKey] = value;
        store.refreshModifiedState();
        NotifyUiAssetsChanged();
        return true;
    }

    public bool RenameUiAsset(string oldKey, string newKey)
    {
        string normalizedOldKey = UiAssetSchema.NormalizeAssetKey(oldKey);
        string normalizedNewKey = UiAssetSchema.NormalizeAssetKey(newKey);
        if (normalizedOldKey.Length == 0
            || normalizedNewKey.Length == 0
            || !uiDocuments.TryGetValue(normalizedOldKey, out JsonObject? source)
            || !ProjectDataStore.isUiDataType(source, UiAssetSchema.UiAssetType))
        {
            return false;
        }
        return store.renameDataEntry("UI", normalizedOldKey, normalizedNewKey);
    }

    public bool DeleteUiAsset(string key)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        return normalizedKey.Length != 0
            && uiDocuments.TryGetValue(normalizedKey, out JsonObject? source)
            && ProjectDataStore.isUiDataType(source, UiAssetSchema.UiAssetType)
            && store.deleteDataEntry("UI", normalizedKey);
    }

    public string? CopyUiAsset(string key)
    {
        string normalizedKey = UiAssetSchema.NormalizeAssetKey(key);
        EditorDocumentCollection data = uiDocuments;
        if (!data.TryGetValue(normalizedKey, out JsonObject? source)
            || !ProjectDataStore.isUiDataType(source, UiAssetSchema.UiAssetType))
        {
            return null;
        }
        string copyKey = normalizedKey + " (copy)";
        if (!store.canCreateDocument("UI", copyKey))
        {
            int index = 1;
            while (!store.canCreateDocument("UI", $"{copyKey}_{index}"))
                index += 1;
            copyKey = $"{copyKey}_{index}";
        }
        return CopyUiAsset(normalizedKey, copyKey) ? copyKey : null;
    }

    public bool CopyUiAsset(string sourceKey, string destinationKey)
    {
        string normalizedSourceKey = UiAssetSchema.NormalizeAssetKey(sourceKey);
        string normalizedDestinationKey = UiAssetSchema.NormalizeAssetKey(destinationKey);
        EditorDocumentCollection data = uiDocuments;
        if (normalizedSourceKey.Length == 0
            || normalizedDestinationKey.Length == 0
            || !store.canCreateDocument("UI", normalizedDestinationKey)
            || !data.TryGetValue(normalizedSourceKey, out JsonObject? source)
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
