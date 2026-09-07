using System;
using System.Text.Json.Nodes;

namespace Ludork.Models;

internal static class LuaMetadataValueDefaults
{
    public static JsonNode? Create(
        LuaMetadataType type,
        Func<string, JsonNode?> createUnknownDefault)
    {
        if (type.Kind == LuaMetadataTypeKind.Union)
        {
            foreach (LuaMetadataType branch in type.Arguments)
            {
                if (TryCreateLiteral(branch, out JsonNode? branchValue))
                    return WrapUnion(branch, branchValue);
            }
            return null;
        }
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

    public static JsonObject WrapUnion(LuaMetadataType branch, JsonNode? value)
    {
        return new JsonObject { ["$type"] = branch.ToSchema(), ["$value"] = value?.DeepClone() };
    }

    public static bool TryCreateLiteral(LuaMetadataType type, out JsonNode? value)
    {
        value = null;
        if (type.Kind == LuaMetadataTypeKind.Named)
            return type.Name is not "function" and not "event" and not "any"
                && tryCreateNamedDefault(type.Name, out value);
        if (type.Kind == LuaMetadataTypeKind.Tuple)
        {
            JsonArray tuple = [];
            foreach (LuaMetadataType argument in type.Arguments)
            {
                if (!TryCreateLiteral(argument, out JsonNode? item))
                    return false;
                tuple.Add(item);
            }
            value = tuple;
            return true;
        }
        value = Create(type, _ => null);
        return value is not null;
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
            || string.Equals(type, "sf.Vector2f", StringComparison.Ordinal))
        {
            value = new JsonArray(0.0, 0.0);
            return true;
        }
        if (string.Equals(type, "sf.Vector2i", StringComparison.Ordinal)
            || string.Equals(type, "sf.Vector2u", StringComparison.Ordinal))
        {
            value = new JsonArray(0, 0);
            return true;
        }
        if (string.Equals(type, "sf.Vector3f", StringComparison.Ordinal))
        {
            value = new JsonArray(0.0, 0.0, 0.0);
            return true;
        }
        if (string.Equals(type, "sf.Vector3i", StringComparison.Ordinal)
            || string.Equals(type, "sf.Vector3u", StringComparison.Ordinal))
        {
            value = new JsonArray(0, 0, 0);
            return true;
        }
        if (string.Equals(type, "sf.Color", StringComparison.Ordinal)
            || string.Equals(type, "sf.Colour", StringComparison.Ordinal))
        {
            value = new JsonArray(255, 255, 255, 255);
            return true;
        }
        if (string.Equals(type, "sf.IntRect", StringComparison.Ordinal))
        {
            value = new JsonArray(new JsonArray(0, 0, 0, 0));
            return true;
        }
        if (string.Equals(type, "sf.FloatRect", StringComparison.Ordinal))
        {
            value = new JsonArray(0.0, 0.0, 0.0, 0.0);
            return true;
        }
        value = null;
        return false;
    }
}
