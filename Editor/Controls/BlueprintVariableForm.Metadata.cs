using Avalonia.Media;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class BlueprintVariableForm
{
    private static string createUniqueDictionaryKey(JsonObject items)
    {
        string key = "key";
        int suffix = 2;
        while (items.ContainsKey(key))
        {
            key = $"key_{suffix}";
            suffix++;
        }
        return key;
    }

    private static string getDisplayName(BlueprintVariableField field)
    {
        return EditorDisplayName.Format(field.Name);
    }

    private static string createTooltip(
        string description,
        BlueprintVariableDependency? dependency)
    {
        List<string> parts = [];
        if (!string.IsNullOrWhiteSpace(description))
            parts.Add(description);
        if (dependency is not null && !string.IsNullOrWhiteSpace(dependency.Source))
        {
            string expected = formatNode(dependency.ExpectedValue);
            if (string.Equals(dependency.Operator, "!=", StringComparison.Ordinal))
                expected = $"!= {expected}";
            string template = LocaleService.Get("META_RELY_TOOLTIP");
            parts.Add(template
                .Replace("{source}", dependency.Source, StringComparison.Ordinal)
                .Replace("{value}", expected, StringComparison.Ordinal));
        }
        return string.Join("\n\n", parts);
    }

    private static string? getAssetSubdirectory(BlueprintVariableField field)
    {
        if (field.AssetSubdirectory is not null)
            return field.AssetSubdirectory;
        string? value = getMetadataString(field, "PathVars");
        return value;
    }

    private static string? getRectSourceField(BlueprintVariableField field)
    {
        if (!string.IsNullOrWhiteSpace(field.RectSourceField))
            return field.RectSourceField;
        return getMetadataString(field, "RectRangeVars");
    }

    private static BlueprintVariableDependency? getDependency(BlueprintVariableField field)
    {
        if (field.Dependency is not null)
            return field.Dependency.Clone();
        JsonNode? rely = getMetadataNode(field, "Rely");
        if (rely is JsonObject relyMap
            && getObjectString(relyMap, "source") is null
            && getObjectString(relyMap, "key") is null
            && getObjectString(relyMap, "var") is null
            && relyMap.TryGetPropertyValue(field.Name, out JsonNode? nestedRule)
            && nestedRule is not null)
        {
            rely = nestedRule;
        }
        if (rely is JsonArray array
            && array.Count >= 2
            && JsonScalar.TryGetString(array[0], out string source))
        {
            return new BlueprintVariableDependency(source, (array[1])?.DeepClone());
        }
        if (rely is not JsonObject rule)
            return null;
        string? sourceName = getObjectString(rule, "source")
            ?? getObjectString(rule, "key")
            ?? getObjectString(rule, "var");
        if (string.IsNullOrWhiteSpace(sourceName))
            return null;
        string operation = getObjectString(rule, "op")
            ?? getObjectString(rule, "operator")
            ?? "==";
        rule.TryGetPropertyValue("value", out JsonNode? expected);
        return new BlueprintVariableDependency(sourceName, (expected)?.DeepClone(), operation);
    }

    private static bool isColourField(BlueprintVariableField field)
    {
        string type = getTypeName(field);
        return string.Equals(type, "sf.Color", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Color", StringComparison.OrdinalIgnoreCase);
    }

    private static bool isProgressField(BlueprintVariableField field)
    {
        return field.Range is not null
            || getMetadataNode(field, "ProgressVars") is not null
            || getMetadataNode(field, "SliderVars") is not null
            || getMetadataNode(field, "RangeVars") is not null;
    }

    private static BlueprintVariableRange getProgressRange(BlueprintVariableField field)
    {
        if (field.Range is not null)
            return field.Range.Normalize();
        JsonNode? spec = getMetadataNode(field, "ProgressVars")
            ?? getMetadataNode(field, "SliderVars")
            ?? getMetadataNode(field, "RangeVars");
        if (spec is JsonValue)
            return new BlueprintVariableRange(0, getDouble(spec, 100), 1).Normalize();
        if (spec is JsonArray array)
        {
            double minimum = array.Count > 0 ? getDouble(array[0], 0) : 0;
            double maximum = array.Count > 1 ? getDouble(array[1], 100) : 100;
            double step = array.Count > 2 ? getDouble(array[2], 1) : 1;
            return new BlueprintVariableRange(minimum, maximum, step).Normalize();
        }
        if (spec is JsonObject range)
        {
            double minimum = getDouble(range["minimum"] ?? range["min"], 0);
            double maximum = getDouble(range["maximum"] ?? range["max"], 100);
            double step = getDouble(range["step"], 1);
            return new BlueprintVariableRange(minimum, maximum, step).Normalize();
        }
        return new BlueprintVariableRange(0, 100, 1);
    }

    private static VectorSpec? getVectorSpec(BlueprintVariableField field)
    {
        string type = getTypeName(field);
        if (string.Equals(type, "Pair", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "sf.Vector2f", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector2f", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(2, false, -999999999m, 999999999m);
        if (string.Equals(type, "sf.Vector2i", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector2i", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(2, true, int.MinValue, int.MaxValue);
        if (string.Equals(type, "sf.Vector2u", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector2u", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(2, true, 0, uint.MaxValue);
        if (string.Equals(type, "sf.Vector3f", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector3f", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(3, false, -999999999m, 999999999m);
        if (string.Equals(type, "sf.Vector3i", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector3i", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(3, true, int.MinValue, int.MaxValue);
        if (string.Equals(type, "sf.Vector3u", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector3u", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(3, true, 0, uint.MaxValue);
        return null;
    }

    private static bool isVectorType(BlueprintVariableField field)
    {
        return getVectorSpec(field) is not null;
    }

    private static bool isIntRectType(string type)
    {
        return string.Equals(type, "sf.IntRect", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "IntRect", StringComparison.OrdinalIgnoreCase);
    }

    private static bool isBoolType(string type, JsonNode? value)
    {
        if (string.Equals(type, "bool", StringComparison.OrdinalIgnoreCase))
            return true;
        if (isPrimitiveType(type))
            return false;
        return value is JsonValue json && json.TryGetValue(out bool _);
    }

    private static bool isIntegerType(string type, JsonNode? value)
    {
        if (string.Equals(type, "int", StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }
        if (isPrimitiveType(type))
            return false;
        if (value is not JsonValue json)
            return false;
        return json.TryGetValue(out int _) || json.TryGetValue(out long _);
    }

    private static bool isFloatType(string type, JsonNode? value)
    {
        if (string.Equals(type, "float", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "double", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "number", StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }
        if (isPrimitiveType(type))
            return false;
        if (value is not JsonValue json)
            return false;
        return json.TryGetValue(out double _) || json.TryGetValue(out decimal _);
    }

    private static bool isPrimitiveType(string type)
    {
        return string.Equals(type, "bool", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "int", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "float", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "double", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "number", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "string", StringComparison.OrdinalIgnoreCase);
    }

    private static string getTypeName(BlueprintVariableField field)
    {
        return field.Type;
    }

    private static string inferType(JsonNode? value)
    {
        if (value is JsonObject)
            return "dict";
        if (value is JsonArray)
            return "list";
        return JsonScalar.ScalarType(value) ?? "string";
    }

    private static JsonArray flattenArray(JsonNode? value)
    {
        JsonArray result = [];
        if (value is not JsonArray source)
            return result;
        foreach (JsonNode? item in source)
        {
            if (item is JsonArray nested)
            {
                foreach (JsonNode? nestedItem in nested)
                    result.Add((nestedItem)?.DeepClone());
            }
            else
            {
                result.Add((item)?.DeepClone());
            }
        }
        return result;
    }

    private static RectRangeSelection parseRect(JsonNode? value, RectRangeSelection fallback)
    {
        if (value is not JsonArray values || values.Count == 0
            || values[0] is not JsonArray arguments || arguments.Count < 4)
            return fallback;
        return new RectRangeSelection(
            getInt(arguments[0], fallback.X),
            getInt(arguments[1], fallback.Y),
            getInt(arguments[2], fallback.Width),
            getInt(arguments[3], fallback.Height));
    }

    private static JsonArray rectToJson(RectRangeSelection rect)
    {
        return new JsonArray(new JsonArray(rect.X, rect.Y, rect.Width, rect.Height));
    }

    private static string formatRect(RectRangeSelection rect)
    {
        return $"(({rect.X}, {rect.Y}), ({rect.Width}, {rect.Height}))";
    }

    private static Color parseColour(JsonNode? value)
    {
        if (value is JsonValue textValue && textValue.TryGetValue(out string? text))
        {
            string candidate = text?.Trim() ?? string.Empty;
            if (candidate.StartsWith('#') && Color.TryParse(candidate, out Color parsed))
                return parsed;
        }
        JsonArray channels = flattenArray(value);
        if (channels.Count < 3)
            return Colors.White;
        byte red = (byte)Math.Clamp(getInt(channels[0], 255), 0, 255);
        byte green = (byte)Math.Clamp(getInt(channels[1], 255), 0, 255);
        byte blue = (byte)Math.Clamp(getInt(channels[2], 255), 0, 255);
        byte alpha = (byte)Math.Clamp(channels.Count > 3 ? getInt(channels[3], 255) : 255, 0, 255);
        return Color.FromArgb(alpha, red, green, blue);
    }

    private static JsonNode? getMetadataNode(BlueprintVariableField field, string key)
    {
        if (field.Meta.TryGetPropertyValue(key, out JsonNode? direct))
            return direct;
        if (field.Meta["Meta"] is JsonObject nested
            && nested.TryGetPropertyValue(key, out JsonNode? nestedValue))
        {
            return nestedValue;
        }
        return null;
    }

    private static JsonObject? getMetadataObject(BlueprintVariableField field, string key)
    {
        return getMetadataNode(field, key) is JsonObject value
            ? value.DeepClone() as JsonObject
            : null;
    }

    private static JsonObject getTupleItemMeta(BlueprintVariableField field, int index)
    {
        JsonNode? tupleMeta = getMetadataNode(field, "TupleMeta");
        if (tupleMeta is JsonArray tupleArray
            && index >= 0
            && index < tupleArray.Count
            && tupleArray[index] is JsonObject arrayItemMeta)
        {
            return arrayItemMeta.DeepClone() as JsonObject ?? [];
        }
        if (tupleMeta is JsonObject tupleObject
            && tupleObject[(index + 1).ToString(CultureInfo.InvariantCulture)] is JsonObject objectItemMeta)
        {
            return objectItemMeta.DeepClone() as JsonObject ?? [];
        }
        return [];
    }

    private static IReadOnlyList<BlueprintVariableOption> getValueOptions(
        BlueprintVariableField field)
    {
        if (field.Options.Count > 0)
            return field.Options;
        if (getMetadataNode(field, "DropBox") is not JsonArray values)
            return [];
        List<BlueprintVariableOption> result = [];
        foreach (JsonNode? value in values)
        {
            string label = JsonScalar.TryGetString(value, out string text)
                ? text
                : value?.ToJsonString() ?? string.Empty;
            result.Add(new BlueprintVariableOption(label, value));
        }
        return result;
    }

    private static string? getMetadataString(BlueprintVariableField field, string key)
    {
        JsonNode? value = getMetadataNode(field, key);
        return JsonScalar.TryGetString(value, out string result) ? result : null;
    }

    private static string? getObjectString(JsonObject value, string key)
    {
        return value.TryGetPropertyValue(key, out JsonNode? node)
            && JsonScalar.TryGetString(node, out string result)
            ? result
            : null;
    }

    private static string getText(JsonNode? value)
    {
        if (value is null)
            return string.Empty;
        if (JsonScalar.TryGetString(value, out string text))
            return text;
        return value.ToJsonString();
    }

    private static string formatNode(JsonNode? value)
    {
        if (value is null)
            return "null";
        if (value is JsonValue json && json.TryGetValue(out bool boolean))
            return boolean ? "true" : "false";
        return getText(value);
    }

    private static int getInt(JsonNode? value, int fallback = 0)
    {
        if (value is JsonValue json)
        {
            if (json.TryGetValue(out int integer))
                return integer;
            if (json.TryGetValue(out long longValue))
                return (int)Math.Clamp(longValue, int.MinValue, int.MaxValue);
            if (json.TryGetValue(out double number))
                return (int)number;
        }
        return int.TryParse(getText(value), NumberStyles.Integer, CultureInfo.InvariantCulture, out int result)
            ? result
            : fallback;
    }

    private static decimal getDecimal(JsonNode? value)
    {
        if (decimal.TryParse(getText(value), NumberStyles.Float, CultureInfo.InvariantCulture, out decimal number))
            return number;
        double floating = getDouble(value);
        return floating > 0 ? decimal.MaxValue : floating < 0 ? decimal.MinValue : 0;
    }

    private static bool isWhole(double value)
    {
        return Math.Abs(value - Math.Round(value)) < double.Epsilon;
    }

    private readonly record struct VectorSpec(int Count, bool IsInteger, decimal Minimum, decimal Maximum);
    private static double getDouble(JsonNode? value, double fallback = 0) => JsonScalar.NumberFromText(value, fallback);
}
