using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public static class MaterialAssetSchema
{
    internal static bool ContentEquals(JsonObject? left, JsonObject? right)
    {
        if (ReferenceEquals(left, right))
            return true;
        if (left is null || right is null)
            return false;
        int leftCount = left.Count - (left.ContainsKey("editor") ? 1 : 0);
        int rightCount = right.Count - (right.ContainsKey("editor") ? 1 : 0);
        if (leftCount != rightCount)
            return false;
        foreach (KeyValuePair<string, JsonNode?> entry in left)
        {
            if (entry.Key == "editor")
                continue;
            if (!right.TryGetPropertyValue(entry.Key, out JsonNode? value) || !JsonNode.DeepEquals(entry.Value, value))
                return false;
        }
        return true;
    }

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
