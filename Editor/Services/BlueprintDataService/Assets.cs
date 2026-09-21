using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class BlueprintDataService
{
    public IReadOnlyList<string> GetModifiedBlueprintKeys()
    {
        return store.Documents.ModifiedDocuments.Where(document => document.Section == "Blueprints" && document.Exists)
            .Select(document => document.Key)
            .OrderBy(key => key, StringComparer.Ordinal)
            .ToArray();
    }

    public bool CreateBlueprint(string key, JsonObject blueprint)
    {
        string normalizedKey = ProjectDataStore.normalizeJsonKey(key);
        EditorDocumentCollection data = blueprintDocuments;
        if (!store.canCreateDocument("Blueprints", normalizedKey))
            return false;
        blueprintDocuments.RecordChange(key);
        data[normalizedKey] = (JsonObject)blueprint.DeepClone();
        store.refreshModifiedState();
        return true;
    }

    public bool RenameBlueprint(string oldKey, string newKey)
    {
        return store.renameDataEntry("Blueprints", oldKey, newKey);
    }

    public bool UpdateBlueprint(string key, JsonObject blueprint)
    {
        string normalizedKey = ProjectDataStore.normalizeJsonKey(key);
        EditorDocumentCollection data = blueprintDocuments;
        if (!data.TryGetValue(normalizedKey, out JsonObject? current))
            return false;
        JsonObject value = (JsonObject)blueprint.DeepClone();
        value.Remove("type");
        if (JsonNode.DeepEquals(current, value))
            return false;
        blueprintDocuments.RecordChange(key);
        data[normalizedKey] = value;
        store.refreshModifiedState();
        return true;
    }

    public bool DeleteBlueprint(string key)
    {
        return store.deleteDataEntry("Blueprints", key);
    }

    public bool CreateCommonFunction(string name, JsonObject? commonFunction = null)
    {
        string key = ProjectDataStore.normalizeJsonKey(name);
        EditorDocumentCollection data = commonFunctionDocuments;
        if (!store.canCreateDocument("CommonFunctions", key))
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
        commonFunctionDocuments.RecordChange(key);
        data[key] = value;
        store.refreshModifiedState();
        return true;
    }

    public bool UpdateCommonFunction(string name, JsonObject commonFunction)
    {
        string key = ProjectDataStore.normalizeJsonKey(name);
        EditorDocumentCollection data = commonFunctionDocuments;
        if (!data.TryGetValue(key, out JsonObject? current)
            || JsonNode.DeepEquals(current, commonFunction))
        {
            return false;
        }
        commonFunctionDocuments.RecordChange(name);
        data[key] = (JsonObject)commonFunction.DeepClone();
        store.refreshModifiedState();
        return true;
    }

    public bool RenameCommonFunction(string oldName, string newName)
    {
        return store.renameDataEntry("CommonFunctions", oldName, newName);
    }

    public bool DeleteCommonFunction(string name)
    {
        return store.deleteDataEntry("CommonFunctions", name);
    }

    public string? CopyCommonFunction(string name)
    {
        string key = ProjectDataStore.normalizeJsonKey(name);
        if (!commonFunctionDocuments.TryGetValue(key, out JsonObject? source))
            return null;
        string copyName = key + " (copy)";
        if (!store.canCreateDocument("CommonFunctions", copyName))
        {
            int index = 1;
            while (!store.canCreateDocument("CommonFunctions", $"{copyName}_{index}"))
                index += 1;
            copyName = $"{copyName}_{index}";
        }
        return CreateCommonFunction(copyName, source) ? copyName : null;
    }

}
