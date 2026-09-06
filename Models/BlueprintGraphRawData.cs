using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

internal static class BlueprintGraphRawData
{
    public static JsonObject CloneWithout(
        JsonObject source,
        string first,
        string second,
        string? third = null,
        string? fourth = null,
        string? fifth = null)
    {
        JsonObject result = [];
        foreach (KeyValuePair<string, JsonNode?> pair in source)
        {
            if (string.Equals(pair.Key, first, StringComparison.Ordinal)
                || string.Equals(pair.Key, second, StringComparison.Ordinal)
                || third is not null && string.Equals(pair.Key, third, StringComparison.Ordinal)
                || fourth is not null && string.Equals(pair.Key, fourth, StringComparison.Ordinal)
                || fifth is not null && string.Equals(pair.Key, fifth, StringComparison.Ordinal))
            {
                continue;
            }
            result[pair.Key] = pair.Value?.DeepClone();
        }
        return result;
    }
}
