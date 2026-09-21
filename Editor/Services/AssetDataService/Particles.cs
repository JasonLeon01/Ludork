using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class AssetDataService
{
    public bool CreateParticle(string key, string name)
    {
        key = ProjectDataStore.normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key) || !store.canCreateDocument("Particles", key))
            return false;
        particleDocuments.RecordChange(key);
        particleDocuments[key] = new JsonObject
        {
            ["name"] = name,
            ["simulationRate"] = 60,
            ["seed"] = 1,
            ["tracks"] = new JsonArray(),
        };
        store.refreshModifiedState();
        return true;
    }

    public bool UpdateParticle(string key, JsonObject particle)
    {
        key = ProjectDataStore.normalizeDataKey(key);
        if (!particleDocuments.TryGetValue(key, out JsonObject? current))
            return false;
        JsonObject copy = (JsonObject)particle.DeepClone();
        copy.Remove("type");
        if (JsonNode.DeepEquals(current, copy))
            return false;
        particleDocuments.RecordChange(key);
        particleDocuments[key] = copy;
        store.refreshModifiedState();
        return true;
    }

}
