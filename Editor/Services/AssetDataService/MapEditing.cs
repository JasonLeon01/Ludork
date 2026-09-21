using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class AssetDataService
{
    public bool CreateAnimation(string key, string name)
    {
        key = ProjectDataStore.normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key) || key.EndsWith(".anim", StringComparison.OrdinalIgnoreCase)
            || !store.canCreateDocument("Animations", key))
            return false;
        animationDocuments.RecordChange(key);
        animationDocuments[key] = new JsonObject
        {
            ["name"] = name,
            ["frameRate"] = 30,
            ["assets"] = new JsonArray(),
            ["timeLines"] = new JsonArray(),
            ["timeTags"] = new JsonArray(),
        };
        store.refreshModifiedState();
        return true;
    }

    public bool UpdateAnimation(string key, JsonObject animation)
    {
        key = ProjectDataStore.normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key) || !animationDocuments.ContainsKey(key))
            return false;
        JsonObject copy = (JsonObject)animation.DeepClone();
        copy.Remove("type");
        if (ProjectDataStore.nodesEqual(animationDocuments[key], copy))
            return false;
        animationDocuments.RecordChange(key);
        animationDocuments[key] = copy;
        store.refreshModifiedState();
        return true;
    }

    public bool CreateCurve(string key, string name, string type = "curve")
    {
        key = ProjectDataStore.normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key)
            || !isCurveType(type)
            || !store.canCreateDocument("Curves", key))
        {
            return false;
        }
        int componentCount = curveComponentCount(type);
        curveDocuments.RecordChange(key);
        curveDocuments[key] = new JsonObject
        {
            ["type"] = type,
            ["name"] = name,
            ["defaultValue"] = createCurveValue(componentCount, 0.0),
            ["preInfinity"] = "constant",
            ["postInfinity"] = "constant",
            ["keys"] = new JsonArray
            {
                createCurveKey(0.0, createCurveValue(componentCount, 0.0)),
                createCurveKey(1.0, createCurveValue(componentCount, 1.0)),
            },
        };
        store.refreshModifiedState();
        return true;
    }

    public bool CreateTextConfig(string key, string type, string name)
    {
        key = ProjectDataStore.normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key)
            || !isTextConfigType(type)
            || !store.canCreateDocument("TextConfigs", key))
        {
            return false;
        }
        textConfigDocuments.RecordChange(key);
        textConfigDocuments[key] = type == "plainTextConfig"
            ? createPlainTextConfig(name)
            : createRichTextConfig(name);
        store.refreshModifiedState();
        return true;
    }

    public bool UpdateTextConfig(string key, JsonObject textConfig)
    {
        key = ProjectDataStore.normalizeDataKey(key);
        string? type = textConfig["type"]?.GetValue<string>();
        if (string.IsNullOrWhiteSpace(key)
            || !isTextConfigType(type)
            || !textConfigDocuments.ContainsKey(key))
        {
            return false;
        }
        JsonObject copy = (JsonObject)textConfig.DeepClone();
        if (ProjectDataStore.nodesEqual(textConfigDocuments[key], copy))
            return false;
        textConfigDocuments.RecordChange(key);
        textConfigDocuments[key] = copy;
        store.refreshModifiedState();
        return true;
    }

    public bool DeleteTextConfig(string key)
    {
        return DeleteTextConfigs([key]);
    }

    public bool DeleteTextConfigs(IEnumerable<string> keys)
    {
        bool changed = false;
        foreach (string key in keys.Select(ProjectDataStore.normalizeJsonKey).Distinct(StringComparer.Ordinal).ToArray())
            changed |= store.DeleteDocumentResource("TextConfigs", key);
        return changed;
    }

    public bool RenameTileset(string oldKey, string newKey)
    {
        return store.RenameDocumentResource("Tilesets", oldKey, newKey);
    }

    public bool CreateTileset(string key)
    {
        key = ProjectDataStore.normalizeDataKey(key);
        if (!store.canCreateDocument("Tilesets", key))
            return false;
        tilesetDocuments.RecordChange(key);
        tilesetDocuments[key] = new JsonObject
        {
            ["name"] = key,
            ["fileName"] = string.Empty,
            ["passable"] = new JsonArray(),
            ["materials"] = new JsonArray(),
            ["dir4"] = new JsonArray(),
        };
        store.refreshModifiedState();
        store.Maps.NotifyAllMapPreviewsChanged(false);
        return true;
    }

    public bool CreateAutoTile(string key)
    {
        key = ProjectDataStore.normalizeDataKey(key);
        if (!store.canCreateDocument("AutoTiles", key))
            return false;
        autoTileDocuments.RecordChange(key);
        autoTileDocuments[key] = new JsonObject
        {
            ["name"] = key,
            ["fileName"] = string.Empty,
            ["passable"] = true,
            ["material"] = createDefaultMaterial(),
        };
        store.refreshModifiedState();
        store.Maps.NotifyAllMapPreviewsChanged(false);
        return true;
    }

    public bool UpdateCurve(string key, JsonObject curve)
    {
        key = ProjectDataStore.normalizeDataKey(key);
        string? type = curve["type"]?.GetValue<string>();
        if (string.IsNullOrWhiteSpace(key)
            || !isCurveType(type)
            || !curveDocuments.ContainsKey(key))
        {
            return false;
        }
        JsonObject copy = (JsonObject)curve.DeepClone();
        if (ProjectDataStore.nodesEqual(curveDocuments[key], copy))
            return false;
        curveDocuments.RecordChange(key);
        curveDocuments[key] = copy;
        store.refreshModifiedState();
        return true;
    }

}
