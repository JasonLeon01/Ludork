using Ludork.Models;
using System;
using System.Globalization;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal static class GeneralDataParameterSchema
{
    public static string GetContainerItemType(JsonObject paramDef, string name)
    {
        string? value = paramDef[name] is null ? null : ReadTypeName(paramDef[name]);
        if (string.IsNullOrWhiteSpace(value))
            throw new InvalidDataException($"General Data container is missing {name}");
        return value;
    }

    public static string ReadTypeName(JsonNode? type)
    {
        if (type is JsonValue scalar && scalar.TryGetValue(out string? text))
            return text ?? "string";
        return type is null ? "string" : LuaMetadataType.Parse(type).ToString();
    }

    public static LuaMetadataType GetValueSchema(JsonObject definition)
    {
        string type = ReadTypeName(definition["type"]);
        return type switch
        {
            "list" => LuaMetadataType.Parse(new JsonObject { ["list"] = LuaMetadataType.Parse(GetContainerItemType(definition, "itemType")).ToSchema() }),
            "dict" => LuaMetadataType.Parse(new JsonObject { ["dict"] = LuaMetadataType.Parse(GetContainerItemType(definition, "valueType")).ToSchema() }),
            _ => LuaMetadataType.Parse(definition["type"]),
        };
    }

    private static LuaMetadataType GetType(GeneralDataParamCreation value)
    {
        return value.Type switch
        {
            "list" => LuaMetadataType.Parse(new JsonObject { ["list"] = LuaMetadataType.Parse(value.ItemType ?? "any").ToSchema() }),
            "dict" => LuaMetadataType.Parse(new JsonObject { ["dict"] = LuaMetadataType.Parse(value.ValueType ?? "any").ToSchema() }),
            _ => LuaMetadataType.Parse(value.Type),
        };
    }

    public static bool IsSfType(string type)
    {
        return type.StartsWith("sf.", StringComparison.Ordinal);
    }

    private static JsonNode? CreateTypedDefault(string type, Func<LuaMetadataType, LuaEnumDefinition>? resolveEnum)
    {
        return LuaMetadataValueDefaults.Create(
            LuaMetadataType.Parse(type),
            _ => JsonValue.Create(string.Empty), resolveEnum);
    }

    public static JsonObject BuildParamDefinition(GeneralDataParamCreation value, Func<LuaMetadataType, LuaEnumDefinition>? resolveEnum = null)
    {
        JsonObject definition = new()
        {
            ["type"] = GetType(value).ToSchema(),
            ["defaultValue"] = value.Type == "file"
                ? JsonValue.Create(string.Empty)
                : ParseDefaultValue(GetType(value).ToString(), value.DefaultText, resolveEnum),
        };
        if (value.Type == "file" && value.DefaultText.Trim().Length != 0)
            definition["base"] = value.DefaultText.Trim();
        if (value.Comment.Length > 0)
            definition["comment"] = value.Comment;
        return definition;
    }

    public static JsonObject UpdateParamDefinition(
        JsonObject currentDefinition,
        GeneralDataParamCreation initialValue,
        GeneralDataParamCreation value,
        Func<LuaMetadataType, LuaEnumDefinition>? resolveEnum = null)
    {
        JsonObject definition = (JsonObject)currentDefinition.DeepClone();
        bool typeChanged = HasValueTypeChanged(initialValue, value);
        definition["type"] = GetType(value).ToSchema();
        definition.Remove("itemType");
        definition.Remove("valueType");
        if (typeChanged || initialValue.DefaultText != value.DefaultText)
        {
            if (typeChanged || value.Type != "file")
            {
                definition["defaultValue"] = value.Type == "file"
                    ? JsonValue.Create(string.Empty)
                    : ParseDefaultValue(GetType(value).ToString(), value.DefaultText, resolveEnum);
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
        return !JsonNode.DeepEquals(GetType(current).ToSchema(), GetType(next).ToSchema());
    }

    private static string FormatDefaultValue(string type, JsonNode? value)
    {
        return type switch
        {
            "int" => (value?.GetValue<long?>() ?? 0).ToString(CultureInfo.InvariantCulture),
            "float" => (value?.GetValue<double?>() ?? 0.0).ToString(CultureInfo.InvariantCulture),
            "bool" => value?.GetValue<bool?>() == true ? "true" : "false",
            "list" or "dict" => value?.ToJsonString() ?? (type == "list" ? "[]" : "{}"),
            _ when IsSfType(type) => value?.ToJsonString() ?? "null",
            _ when LuaMetadataType.Parse(type).Kind != LuaMetadataTypeKind.Named => value?.ToJsonString() ?? "null",
            _ => value?.GetValue<string>() ?? string.Empty,
        };
    }

    private static JsonNode? ParseDefaultValue(string type, string text, Func<LuaMetadataType, LuaEnumDefinition>? resolveEnum)
    {
        return type switch
        {
            "int" => long.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out long i) ? i : 0,
            "float" => double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double d) ? d : 0.0,
            "bool" => text.Equals("true", StringComparison.OrdinalIgnoreCase) ? true : false,
            "list" => string.IsNullOrWhiteSpace(text) ? new JsonArray() : JsonNode.Parse(text),
            "dict" => string.IsNullOrWhiteSpace(text) ? new JsonObject() : JsonNode.Parse(text),
            _ when LuaMetadataType.Parse(type).Kind != LuaMetadataTypeKind.Named => string.IsNullOrWhiteSpace(text)
                ? CreateTypedDefault(type, resolveEnum) : JsonNode.Parse(text),
            "file" => JsonValue.Create(string.Empty)!,
            _ when IsSfType(type) => string.IsNullOrWhiteSpace(text) ? CreateTypedDefault(type, resolveEnum) : JsonNode.Parse(text),
            _ => JsonValue.Create(text) ?? JsonValue.Create(string.Empty)!,
        };
    }
}
