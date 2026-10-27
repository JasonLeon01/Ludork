using Ludork.Models;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class AssetDataService
{
    public bool CreateMaterial(string key, JsonObject template)
    {
        key = ProjectDataStore.normalizeDataKey(key);
        if (!store.canCreateDocument("Materials", key))
            return false;
        materialDocuments.RecordChange(key);
        materialDocuments[key] = (JsonObject)template.DeepClone();
        store.refreshModifiedState();
        return true;
    }

    public bool UpdateMaterial(string key, JsonObject value)
    {
        if (!materialDocuments.TryGetValue(key, out JsonObject? current) || JsonNode.DeepEquals(current, value))
            return false;
        materialDocuments.RecordChange(key);
        materialDocuments[key] = (JsonObject)value.DeepClone();
        store.refreshModifiedState();
        return true;
    }

    public JsonObject CaptureMaterials()
    {
        JsonObject result = new();
        foreach (KeyValuePair<string, JsonObject> entry in materialDocuments)
        {
            JsonObject asset = (JsonObject)entry.Value.DeepClone();
            asset["type"] = "material";
            result[GameDataPath.Materials + "/" + entry.Key + ".json"] = asset;
        }
        return result;
    }
}
