using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

internal static class LuaMetadataLiteralValidation
{
    public static void ValidateNodeParameter(LuaMetadataType type, JsonNode? value, string path, ICollection<string> errors)
    {
        if (value is not null)
            validate(type, value, path, errors, true);
    }

    public static void ValidateUnions(LuaMetadataType type, JsonNode? value, string path, ICollection<string> errors)
    {
        if (type.ContainsUnion && value is not null)
            validate(type, value, path, errors, false);
    }

    private static void validate(LuaMetadataType type, JsonNode? value, string path, ICollection<string> errors, bool strict)
    {
        if (type.Kind == LuaMetadataTypeKind.Union)
        {
            if (value is not JsonObject wrapper || wrapper.Count != 2
                || !wrapper.ContainsKey("$type") || !wrapper.ContainsKey("$value"))
            {
                errors.Add(path + " requires a union literal with $type and $value");
                return;
            }
            LuaMetadataType? branch = type.Arguments.FirstOrDefault(candidate => JsonNode.DeepEquals(candidate.ToSchema(), wrapper["$type"]));
            if (branch is null)
            {
                errors.Add(path + ".$type selects an undeclared union branch");
                return;
            }
            validate(branch, wrapper["$value"], path + ".$value", errors, true);
            return;
        }
        if (type.Kind is LuaMetadataTypeKind.List or LuaMetadataTypeKind.Tuple)
        {
            if (value is not JsonArray sequence || type.Kind == LuaMetadataTypeKind.Tuple && sequence.Count != type.Arguments.Count)
            {
                errors.Add(path + " must match " + type);
                return;
            }
            for (int index = 0; index < sequence.Count; index++)
                validate(type.Arguments[type.Kind == LuaMetadataTypeKind.List ? 0 : index], sequence[index], $"{path}[{index}]", errors, strict);
            return;
        }
        if (type.Kind == LuaMetadataTypeKind.Dictionary)
        {
            if (value is not JsonObject dictionary)
            {
                errors.Add(path + " must be a dictionary");
                return;
            }
            foreach (KeyValuePair<string, JsonNode?> item in dictionary)
                validate(type.Arguments[1], item.Value, path + "." + item.Key, errors, strict);
            return;
        }
        if (!strict || type.IsAny)
            return;
        if (type.Kind == LuaMetadataTypeKind.Table)
        {
            if (value is not JsonObject and not JsonArray)
                errors.Add(path + " must be a table");
            return;
        }
        bool valid = type.Name switch
        {
            "nil" => value is null,
            "bool" => value is JsonValue boolean && boolean.TryGetValue(out bool _),
            "string" or "file" => value is JsonValue text && text.TryGetValue(out string? _),
            "int" => tryInteger(value, out long _),
            "float" or "double" or "number" => tryNumber(value, out double _),
            "function" or "event" => isExpression(value),
            "sf.Vector2f" or "Pair" => validVector(value, 2, false, false),
            "sf.Vector3f" => validVector(value, 3, false, false),
            "sf.Vector2i" => validVector(value, 2, true, false),
            "sf.Vector3i" => validVector(value, 3, true, false),
            "sf.Vector2u" => validVector(value, 2, true, true),
            "sf.Vector3u" => validVector(value, 3, true, true),
            "sf.Color" => value is JsonArray colour && colour.Count == 4 && colour.All(component => tryInteger(component, out long channel) && channel >= 0 && channel <= 255),
            "sf.FloatRect" => validVector(value, 4, false, false),
            "sf.IntRect" => value is JsonArray rectangle && rectangle.Count == 1 && validVector(rectangle[0], 4, true, false),
            _ => value is JsonObject or JsonArray || isExpression(value),
        };
        if (!valid)
            errors.Add(path + " must be a literal of type " + type.Name);
    }

    private static bool isExpression(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? expression)
            && !string.IsNullOrWhiteSpace(expression);
    }

    private static bool validVector(JsonNode? value, int count, bool integer, bool unsigned)
    {
        if (value is not JsonArray vector || vector.Count != count)
            return false;
        foreach (JsonNode? component in vector)
        {
            if (integer)
            {
                if (!tryInteger(component, out long integerComponent)
                    || integerComponent < (unsigned ? 0 : int.MinValue)
                    || integerComponent > (unsigned ? uint.MaxValue : int.MaxValue))
                {
                    return false;
                }
                continue;
            }
            if (!tryNumber(component, out double number))
                return false;
            if (Math.Abs(number) > float.MaxValue)
                return false;
        }
        return true;
    }

    private static bool tryInteger(JsonNode? value, out long integer)
    {
        integer = 0;
        if (value is not JsonValue scalar || scalar.GetValueKind() != System.Text.Json.JsonValueKind.Number)
            return false;
        if (scalar.TryGetValue(out integer))
            return true;
        if (scalar.TryGetValue(out double number) && !double.IsFinite(number))
            return false;
        return long.TryParse(scalar.ToJsonString(), NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out integer);
    }

    private static bool tryNumber(JsonNode? value, out double number)
    {
        number = 0;
        if (value is not JsonValue scalar)
            return false;
        if (scalar.TryGetValue(out double floating))
            number = floating;
        else if (scalar.TryGetValue(out long integer))
            number = integer;
        else if (scalar.TryGetValue(out int smallInteger))
            number = smallInteger;
        else
            return false;
        return double.IsFinite(number);
    }
}
