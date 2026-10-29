using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal static class EditorJsonEdits
{
    internal static object[] Path(JsonObject root, JsonNode node)
    {
        List<object> path = [];
        while (!ReferenceEquals(node, root))
        {
            JsonNode parent = node.Parent ?? throw new InvalidOperationException("The edited value is no longer in the document.");
            path.Add(parent is JsonArray ? node.GetElementIndex() : node.GetPropertyName());
            node = parent;
        }
        path.Reverse();
        return path.ToArray();
    }

    internal static IReadOnlyList<JsonDataEdit> Properties(JsonObject root, JsonObject owner, params string[] properties)
    {
        object[] path = Path(root, owner);
        List<JsonDataEdit> edits = [];
        foreach (string property in properties)
        {
            bool exists = owner.TryGetPropertyValue(property, out JsonNode? value);
            edits.Add(new JsonDataEdit(exists ? JsonDataEdit.Operation.Set : JsonDataEdit.Operation.Remove,
                [.. path, property], value));
        }
        return edits;
    }
}
