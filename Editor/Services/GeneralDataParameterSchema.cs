using Ludork.Models;
using System;
using System.Globalization;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal static class GeneralDataParameterSchema
{
    public static bool IsParamReferenceAllowed(JsonObject? paramDef)
    {
        if (paramDef is null)
            return false;
        string type = ReadTypeName(paramDef["type"]);
        return type == "string"
            || type == "dict"
            || (type == "list" && GetContainerItemType(paramDef, "itemType") == "string");
    }

    public static JsonObject? GetParamReference(JsonObject? paramDef)
    {
        if (!IsParamReferenceAllowed(paramDef)
            || paramDef!["reference"] is not JsonObject reference)
        {
            return null;
        }
        string kind = reference["kind"]?.GetValue<string>() ?? string.Empty;
        if (kind == "animation")
            return reference;
        string key = reference["key"]?.GetValue<string>() ?? string.Empty;
        return kind == "general" && key.Length > 0 ? reference : null;
    }

    public static string GetContainerItemType(JsonObject paramDef, string name)
    {
        string? value = paramDef[name] is null ? null : ReadTypeName(paramDef[name]);
        if (string.IsNullOrWhiteSpace(value))
            throw new InvalidOperationException($"General Data container is missing {name}");
        return value;
    }

    public static string ReadTypeName(JsonNode? type)
    {
        if (type is JsonValue scalar && scalar.TryGetValue(out string? text))
            return text ?? "string";
        return type is null ? "string" : LuaMetadataType.Parse(type).ToString();
    }

    private static JsonNode CanonicalTypeNode(string type)
    {
        return type is "list" or "dict" ? JsonValue.Create(type)! : LuaMetadataType.Parse(type).ToSchema();
    }

    public static bool IsSfType(string type)
    {
        return type.StartsWith("sf.", StringComparison.Ordinal);
    }

    private static JsonNode CreateTypedDefault(string type)
    {
        return LuaMetadataValueDefaults.Create(
            LuaMetadataType.Parse(type),
            _ => JsonValue.Create(string.Empty)) ?? JsonValue.Create(string.Empty)!;
    }

    public static JsonObject BuildParamDefinition(GeneralDataParamCreation value)
    {
        JsonObject definition = new()
        {
            ["type"] = CanonicalTypeNode(value.Type),
            ["defaultValue"] = value.Type == "file"
                ? JsonValue.Create(string.Empty)
                : ParseDefaultValue(value.Type, value.DefaultText),
        };
        if (value.Type == "file" && value.DefaultText.Trim().Length != 0)
            definition["base"] = value.DefaultText.Trim();
        if (value.Comment.Length > 0)
            definition["comment"] = value.Comment;
        if (value.ItemType is not null)
            definition["itemType"] = CanonicalTypeNode(value.ItemType);
        if (value.ValueType is not null)
            definition["valueType"] = CanonicalTypeNode(value.ValueType);
        return definition;
    }

    public static JsonObject UpdateParamDefinition(
        JsonObject currentDefinition,
        GeneralDataParamCreation initialValue,
        GeneralDataParamCreation value)
    {
        JsonObject definition = (JsonObject)currentDefinition.DeepClone();
        bool typeChanged = HasValueTypeChanged(initialValue, value);
        if (initialValue.Type != value.Type)
            definition["type"] = CanonicalTypeNode(value.Type);
        if (typeChanged || initialValue.DefaultText != value.DefaultText)
        {
            if (typeChanged || value.Type != "file")
            {
                definition["defaultValue"] = value.Type == "file"
                    ? JsonValue.Create(string.Empty)
                    : ParseDefaultValue(value.Type, value.DefaultText);
            }
            if (value.Type == "file" && value.DefaultText.Trim().Length != 0)
                definition["base"] = value.DefaultText.Trim();
            else if (initialValue.Type == "file" || value.Type == "file")
                definition.Remove("base");
        }
        if (initialValue.Comment.Trim() != value.Comment)
        {
            if (value.Comment.Length == 0)
                definition.Remove("comment");
            else
                definition["comment"] = value.Comment;
        }
        if (initialValue.ItemType != value.ItemType)
        {
            if (value.ItemType is null)
                definition.Remove("itemType");
            else
                definition["itemType"] = CanonicalTypeNode(value.ItemType);
        }
        if (initialValue.ValueType != value.ValueType)
        {
            if (value.ValueType is null)
                definition.Remove("valueType");
            else
                definition["valueType"] = CanonicalTypeNode(value.ValueType);
        }
        if (typeChanged && !IsParamReferenceAllowed(definition))
            definition.Remove("reference");
        return definition;
    }

    public static GeneralDataParamCreation CreateParamCreation(
        string name,
        JsonObject definition)
    {
        string type = ReadTypeName(definition["type"]);
        return new GeneralDataParamCreation(
            name,
            type,
            type == "list" ? GetContainerItemType(definition, "itemType") : null,
            type == "dict" ? GetContainerItemType(definition, "valueType") : null,
            type == "file"
                ? definition["base"]?.GetValue<string>() ?? string.Empty
                : FormatDefaultValue(type, definition["defaultValue"]),
            definition["comment"]?.GetValue<string>() ?? string.Empty);
    }

    public static bool HasValueTypeChanged(
        GeneralDataParamCreation current,
        GeneralDataParamCreation next)
    {
        if (current.Type != next.Type)
            return true;
        return current.Type switch
        {
            "list" => current.ItemType != next.ItemType,
            "dict" => current.ValueType != next.ValueType,
            _ => false,
        };
    }

    private static string FormatDefaultValue(string type, JsonNode? value)
    {
        return type switch
        {
            "int" => (value?.GetValue<long?>() ?? 0).ToString(CultureInfo.InvariantCulture),
            "float" => (value?.GetValue<double?>() ?? 0.0).ToString(CultureInfo.InvariantCulture),
            "bool" => value?.GetValue<bool?>() == true ? "true" : "false",
            "list" or "dict" => string.Empty,
            _ when IsSfType(type) => string.Empty,
            _ when LuaMetadataType.Parse(type).Kind != LuaMetadataTypeKind.Named => value?.ToJsonString() ?? "null",
            _ => value?.GetValue<string>() ?? string.Empty,
        };
    }

    private static JsonNode ParseDefaultValue(string type, string text)
    {
        return type switch
        {
            "int" => long.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out long i) ? i : 0,
            "float" => double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double d) ? d : 0.0,
            "bool" => text.Equals("true", StringComparison.OrdinalIgnoreCase) ? true : false,
            "list" => new JsonArray(),
            "dict" => new JsonObject(),
            _ when LuaMetadataType.Parse(type).Kind != LuaMetadataTypeKind.Named => JsonNode.Parse(text)!,
            "file" => JsonValue.Create(string.Empty)!,
            _ when IsSfType(type) => CreateTypedDefault(type),
            _ => JsonValue.Create(text) ?? JsonValue.Create(string.Empty)!,
        };
    }
}
