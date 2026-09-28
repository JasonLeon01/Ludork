using System;
using System.Globalization;
using System.Diagnostics.CodeAnalysis;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal static class JsonScalar
{
    public static bool TryGetNumber(JsonNode? node, out double result)
    {
        result = 0;
        if (node is not JsonValue value)
            return false;
        if (value.TryGetValue(out double real)) result = real;
        else if (value.TryGetValue(out float single)) result = single;
        else if (value.TryGetValue(out int integer)) result = integer;
        else if (value.TryGetValue(out long wide)) result = wide;
        else if (value.TryGetValue(out decimal precise)) result = (double)precise;
        else if (value.TryGetValue(out uint unsigned)) result = unsigned;
        else if (value.TryGetValue(out ulong unsignedWide)) result = unsignedWide;
        else if (value.TryGetValue(out short small)) result = small;
        else if (value.TryGetValue(out ushort unsignedSmall)) result = unsignedSmall;
        else if (value.TryGetValue(out byte octet)) result = octet;
        else if (value.TryGetValue(out sbyte signedOctet)) result = signedOctet;
        else return false;
        return true;
    }

    public static double Number(JsonNode? node, double fallback = 0)
        => TryGetNumber(node, out double result) ? result : fallback;

    public static bool TryGetFiniteNumber(JsonNode? node, out double result)
        => TryGetNumber(node, out result) && double.IsFinite(result);

    public static double FiniteNumber(JsonNode? node, double fallback = 0)
        => TryGetFiniteNumber(node, out double result) ? result : fallback;

    public static double NumberFromText(JsonNode? node, double fallback = 0)
        => TryGetNumber(node, out double result)
            || double.TryParse(String(node), NumberStyles.Float, CultureInfo.InvariantCulture, out result)
                ? result : fallback;

    public static bool TryGetIntegerFromText(JsonNode? node, out int result)
    {
        if (TryGetFiniteNumber(node, out double number))
        {
            result = (int)Math.Clamp(number, int.MinValue, int.MaxValue);
            return true;
        }
        return int.TryParse(String(node), NumberStyles.Integer, CultureInfo.InvariantCulture, out result);
    }

    public static int IntegerFromText(JsonNode? node, int fallback = 0)
        => TryGetIntegerFromText(node, out int result) ? result : fallback;

    public static decimal ToDecimal(double number, decimal fallback = 0)
    {
        if (!double.IsFinite(number))
            return fallback;
        if (number >= (double)decimal.MaxValue)
            return decimal.MaxValue;
        if (number <= (double)decimal.MinValue)
            return decimal.MinValue;
        return (decimal)number;
    }

    public static decimal Decimal(JsonNode? node, double fallback = 0)
        => ToDecimal(Number(node, fallback), ToDecimal(fallback));

    public static bool TryGetString(JsonNode? node, out string result)
    {
        if (node is JsonValue value && value.TryGetValue(out string? text) && text is not null)
        {
            result = text;
            return true;
        }
        result = string.Empty;
        return false;
    }

    [return: NotNullIfNotNull(nameof(fallback))]
    public static string? String(JsonNode? node, string? fallback = null)
        => TryGetString(node, out string text) ? text : fallback;

    public static bool Bool(JsonNode? node, bool fallback = false)
        => node is JsonValue value && value.TryGetValue(out bool result) ? result : fallback;

    public static string? ScalarType(JsonNode? node)
    {
        if (node is not JsonValue value)
            return null;
        if (value.TryGetValue(out bool _)) return "bool";
        if (value.TryGetValue(out string? _)) return "string";
        if (value.TryGetValue(out int _) || value.TryGetValue(out long _)
            || value.TryGetValue(out uint _) || value.TryGetValue(out ulong _)
            || value.TryGetValue(out short _) || value.TryGetValue(out ushort _)
            || value.TryGetValue(out byte _) || value.TryGetValue(out sbyte _)) return "int";
        return TryGetNumber(node, out _) ? "float" : null;
    }
}
