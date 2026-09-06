using System;
using System.Text.Json.Nodes;

namespace Ludork.Models;

internal static class LuaMetadataValueDefaults
{
    public static JsonNode? Create(
        LuaMetadataType type,
        Func<string, JsonNode?> createUnknownDefault)
    {
        if (type.Kind == LuaMetadataTypeKind.List)
            return new JsonArray();
        if (type.Kind == LuaMetadataTypeKind.Dictionary
            || type.Kind == LuaMetadataTypeKind.Table)
        {
            return new JsonObject();
        }
        if (type.Kind == LuaMetadataTypeKind.Tuple)
        {
            JsonArray result = [];
            foreach (LuaMetadataType itemType in type.Arguments)
                result.Add(Create(itemType, createUnknownDefault));
            return result;
        }
        return tryCreateNamedDefault(type.Name, out JsonNode? value)
            ? value
            : createUnknownDefault(type.Name);
    }

    private static bool tryCreateNamedDefault(string typeName, out JsonNode? value)
    {
        string type = typeName.Trim();
        if (string.Equals(type, "bool", StringComparison.OrdinalIgnoreCase))
        {
            value = JsonValue.Create(false);
            return true;
        }
        if (string.Equals(type, "int", StringComparison.OrdinalIgnoreCase))
        {
            value = JsonValue.Create(0);
            return true;
        }
        if (string.Equals(type, "float", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "number", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "double", StringComparison.OrdinalIgnoreCase))
        {
            value = JsonValue.Create(0.0);
            return true;
        }
        if (string.Equals(type, "string", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "function", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "event", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "any", StringComparison.OrdinalIgnoreCase))
        {
            value = JsonValue.Create(string.Empty);
            return true;
        }
        if (string.Equals(type, "nil", StringComparison.OrdinalIgnoreCase))
        {
            value = null;
            return true;
        }
        if (string.Equals(type, "Pair", StringComparison.OrdinalIgnoreCase)
            || type.EndsWith("Vector2f", StringComparison.OrdinalIgnoreCase))
        {
            value = new JsonArray(0.0, 0.0);
            return true;
        }
        if (type.EndsWith("Vector2i", StringComparison.OrdinalIgnoreCase)
            || type.EndsWith("Vector2u", StringComparison.OrdinalIgnoreCase))
        {
            value = new JsonArray(0, 0);
            return true;
        }
        if (type.EndsWith("Vector3f", StringComparison.OrdinalIgnoreCase))
        {
            value = new JsonArray(0.0, 0.0, 0.0);
            return true;
        }
        if (type.EndsWith("Vector3i", StringComparison.OrdinalIgnoreCase)
            || type.EndsWith("Vector3u", StringComparison.OrdinalIgnoreCase))
        {
            value = new JsonArray(0, 0, 0);
            return true;
        }
        if (type.EndsWith("Color", StringComparison.OrdinalIgnoreCase)
            || type.EndsWith("Colour", StringComparison.OrdinalIgnoreCase))
        {
            value = new JsonArray(255, 255, 255, 255);
            return true;
        }
        if (type.EndsWith("IntRect", StringComparison.OrdinalIgnoreCase))
        {
            value = new JsonArray(new JsonArray(0, 0, 0, 0));
            return true;
        }
        if (type.EndsWith("FloatRect", StringComparison.OrdinalIgnoreCase))
        {
            value = new JsonArray(0.0, 0.0, 0.0, 0.0);
            return true;
        }
        value = null;
        return false;
    }
}
