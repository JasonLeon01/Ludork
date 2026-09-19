using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text.Json;
using System.Text.Json.Nodes;
using Ludork.Models;
using Ludork.Services;

namespace Ludork.Views.Utils;

internal static class BlueprintNodeTextValues
{
    public static string Format(string typeName, JsonNode? value)
    {
        LuaMetadataType type = LuaMetadataType.Parse(typeName);
        if (value is null)
            return type.IsAny ? "null" : string.Empty;
        if (type.Kind == LuaMetadataTypeKind.Named
            && value is JsonValue scalar && scalar.TryGetValue(out string? text))
        {
            return text ?? string.Empty;
        }
        return value.ToJsonString();
    }

    public static bool TryParse(string typeName, string text, out JsonNode? value, out string? error)
    {
        LuaMetadataType type = LuaMetadataType.Parse(typeName);
        value = null;
        error = null;
        if (type.Kind == LuaMetadataTypeKind.Named && type.Name is "string" or "file")
        {
            value = JsonValue.Create(text);
            return true;
        }
        if (type.IsAny)
        {
            try
            {
                value = JsonNode.Parse(text);
                if (value is JsonValue scalar && scalar.TryGetValue(out string? _))
                    value = JsonValue.Create(text);
            }
            catch (JsonException)
            {
                value = JsonValue.Create(text);
            }
            return true;
        }
        string trimmed = text.Trim();
        if (trimmed.Length == 0)
            return true;
        if (type.Kind == LuaMetadataTypeKind.Named && type.Name is "function" or "event")
        {
            value = JsonValue.Create(text);
        }
        else if (trimmed == "null")
        {
            return true;
        }
        else if (type.Kind == LuaMetadataTypeKind.Named && type.Name == "int")
        {
            if (!long.TryParse(trimmed, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out long integer))
            {
                error = LocaleService.Get("BLUEPRINT_TEXT_VALUE_INTEGER_ERROR");
                return false;
            }
            value = JsonValue.Create(integer);
        }
        else if (type.Kind == LuaMetadataTypeKind.Named && type.Name is "float" or "double" or "number")
        {
            if (!double.TryParse(trimmed, NumberStyles.Float, CultureInfo.InvariantCulture, out double number)
                || !double.IsFinite(number))
            {
                error = LocaleService.Get("BLUEPRINT_TEXT_VALUE_NUMBER_ERROR");
                return false;
            }
            value = JsonValue.Create(number);
        }
        else if (type.Kind == LuaMetadataTypeKind.Named && type.Name == "bool")
        {
            if (!bool.TryParse(trimmed, out bool boolean))
            {
                error = LocaleService.Get("BLUEPRINT_TEXT_VALUE_BOOLEAN_ERROR");
                return false;
            }
            value = JsonValue.Create(boolean);
        }
        else if (isExpressionType(type) && trimmed[0] is not '[' and not '{')
        {
            value = JsonValue.Create(text);
        }
        else
        {
            try
            {
                value = JsonNode.Parse(text);
            }
            catch (JsonException exception)
            {
                error = LocaleService.Get("BLUEPRINT_TEXT_VALUE_JSON_ERROR").Replace("{reason}", exception.Message);
                return false;
            }
        }

        List<string> errors = [];
        LuaMetadataLiteralValidation.ValidateNodeParameter(type, value, "$", errors);
        if (errors.Count == 0)
            return true;
        value = null;
        error = LocaleService.Get("BLUEPRINT_TEXT_VALUE_TYPE_ERROR")
            .Replace("{type}", type.ToString())
            .Replace("{reason}", string.Join("; ", errors));
        return false;
    }

    private static bool isExpressionType(LuaMetadataType type)
    {
        return type.Kind == LuaMetadataTypeKind.Named && type.Name is not
            ("nil" or "Pair" or "sf.Vector2f" or "sf.Vector3f" or "sf.Vector2i" or "sf.Vector3i"
            or "sf.Vector2u" or "sf.Vector3u" or "sf.Color" or "sf.FloatRect" or "sf.IntRect");
    }
}
