using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public static class MaterialAssetSchema
{
    public static void ValidateReference(JsonNode? value, string path, ICollection<string> errors)
    {
        if (value is null)
            return;
        if (value is not JsonValue scalar || !scalar.TryGetValue<string>(out string? text)
            || text.Length != 0 && !GameDataPath.TryGetMaterialKey(text, out _))
            errors.Add(path + " must reference a Data/Materials/*.json file");
    }

    public static IReadOnlyList<string> Validate(JsonObject asset, string key)
    {
        List<string> errors = [];
        string path = "Materials/" + key;
        if (asset["version"] is not JsonValue version || !version.TryGetValue<int>(out int number) || number != 1)
            errors.Add(path + ".version must be 1");
        if (asset["parameters"] is not JsonArray)
            errors.Add(path + ".parameters must be an array");
        if (asset["graph"] is not JsonObject graph || graph["nodes"] is not JsonArray)
            errors.Add(path + ".graph.nodes must be an array");
        return errors;
    }
}
