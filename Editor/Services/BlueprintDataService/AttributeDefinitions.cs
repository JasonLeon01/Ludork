using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class BlueprintDataService
{
    private LuaMetadataService? attributeMetadata;

    public bool AddBlueprintAttribute(string key, string name, JsonObject? definition, JsonNode? value)
    {
        key = ProjectDataStore.normalizeJsonKey(key);
        if (!blueprintDocuments.TryGetValue(key, out JsonObject? current))
            return false;
        JsonObject next = (JsonObject)current.DeepClone();
        JsonObject attrs = next["attrs"] as JsonObject ?? [];
        if (attrs.ContainsKey(name))
            return false;
        attrs[name] = value?.DeepClone();
        next["attrs"] = attrs;
        if (definition is not null)
        {
            JsonObject definitions = next["attrDefs"] as JsonObject ?? [];
            if (definitions.ContainsKey(name))
                return false;
            definitions[name] = definition.DeepClone();
            next["attrDefs"] = definitions;
        }
        return UpdateBlueprint(key, next);
    }

    public bool RemoveBlueprintAttribute(string key, string name)
    {
        key = ProjectDataStore.normalizeJsonKey(key);
        if (!blueprintDocuments.TryGetValue(key, out JsonObject? current))
            return false;
        JsonObject next = (JsonObject)current.DeepClone();
        bool changed = next["attrs"] is JsonObject attrs && attrs.Remove(name);
        if (next["attrDefs"] is JsonObject definitions)
        {
            changed |= definitions.Remove(name);
            if (definitions.Count == 0)
                next.Remove("attrDefs");
        }
        return changed && UpdateBlueprint(key, next);
    }

    private void validateAttributeChange(string key, JsonObject candidate)
    {
        attributeMetadata ??= new LuaMetadataService(store.ProjectPath, strictReads: true);
        using BlueprintClassResolver resolver = new(attributeMetadata, reference => reference == key
            ? candidate : blueprintDocuments.TryGetValue(reference, out JsonObject? document) ? document : null);
        resolver.ResolveBlueprint(candidate, key);
        HashSet<string> affected = new(StringComparer.Ordinal) { key };
        bool added;
        do
        {
            added = false;
            foreach (KeyValuePair<string, JsonObject> document in blueprintDocuments)
            {
                string? parent = document.Value["parent"] is JsonValue value && value.TryGetValue(out string? text) ? text : null;
                if (BlueprintReference.IsReference(parent) && affected.Contains(BlueprintReference.NormalizeKey(parent))
                    && affected.Add(document.Key))
                    added = true;
            }
        } while (added);
        foreach (string dependent in affected.Where(reference => reference != key))
            resolver.Resolve(BlueprintReference.ToReference(dependent));
        if (blueprintDocuments.TryGetValue(key, out JsonObject? previous)
            && (!JsonNode.DeepEquals(previous["attrDefs"], candidate["attrDefs"])
                || !JsonNode.DeepEquals(previous["parent"], candidate["parent"])
                || !JsonNode.DeepEquals(previous["attrs"]?["scriptPath"], candidate["attrs"]?["scriptPath"])
                || !JsonNode.DeepEquals(previous["attrs"]?["scriptMixin"], candidate["attrs"]?["scriptMixin"])))
            store.ValidateBlueprintMapOverrides(key, candidate);
    }
}
