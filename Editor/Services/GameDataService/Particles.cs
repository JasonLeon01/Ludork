using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    public bool CreateParticle(string key, string name)
    {
        key = normalizeDataKey(key);
        if (string.IsNullOrWhiteSpace(key) || !canCreateDocument("Particles", key))
            return false;
        RecordDocumentSnapshot("Particles", key);
        sections["Particles"].Data[key] = new JsonObject
        {
            ["name"] = name,
            ["simulationRate"] = 60,
            ["seed"] = 1,
            ["tracks"] = new JsonArray(),
        };
        refreshModifiedState();
        return true;
    }

    public bool UpdateParticle(string key, JsonObject particle)
    {
        key = normalizeDataKey(key);
        if (!sections["Particles"].Data.TryGetValue(key, out JsonObject? current))
            return false;
        JsonObject copy = (JsonObject)particle.DeepClone();
        copy.Remove("type");
        if (JsonNode.DeepEquals(current, copy))
            return false;
        RecordDocumentSnapshot("Particles", key);
        sections["Particles"].Data[key] = copy;
        refreshModifiedState();
        return true;
    }
}
