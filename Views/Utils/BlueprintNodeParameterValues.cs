using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils;

internal static class BlueprintNodeParameterValues
{
    public static JsonArray NormalizeRoute(JsonNode? value)
    {
        JsonArray result = [];
        if (value is not JsonArray route)
            return result;
        foreach (JsonNode? item in route)
        {
            if (item is JsonArray step
                && step.Count >= 2
                && tryGetInt(step[0], out int x)
                && tryGetInt(step[1], out int y))
            {
                result.Add(new JsonArray(x, y));
            }
        }
        return result;
    }

    public static JsonArray? NormalizePosition(JsonNode? value)
    {
        if (value is not JsonArray position
            || position.Count < 2
            || !tryGetInt(position[0], out int x)
            || !tryGetInt(position[1], out int y))
        {
            return null;
        }
        return new JsonArray(x, y);
    }

    public static string FormatRoute(JsonNode? value)
    {
        JsonArray route = NormalizeRoute(value);
        List<string> steps = [];
        foreach (JsonNode? item in route)
        {
            JsonArray step = (JsonArray)item!;
            steps.Add($"({getInt(step[0])}, {getInt(step[1])})");
        }
        return "[" + string.Join(", ", steps) + "]";
    }

    public static string FormatPosition(JsonNode? value)
    {
        JsonArray? position = NormalizePosition(value);
        if (position is null)
            return LocaleService.Get("TRANSFER_POS_NONE");
        return LocaleService.Get("TRANSFER_POS_LABEL")
            .Replace("{x}", getInt(position[0]).ToString(CultureInfo.InvariantCulture), StringComparison.Ordinal)
            .Replace("{y}", getInt(position[1]).ToString(CultureInfo.InvariantCulture), StringComparison.Ordinal);
    }

    public static string GetString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text)
            ? text ?? string.Empty
            : string.Empty;
    }

    public static IReadOnlyList<RouteStep> GetRouteSteps(JsonNode? value)
    {
        JsonArray route = NormalizeRoute(value);
        List<RouteStep> result = new(route.Count);
        foreach (JsonNode? item in route)
        {
            JsonArray step = (JsonArray)item!;
            result.Add(new RouteStep(getInt(step[0]), getInt(step[1])));
        }
        return result;
    }

    public static JsonArray RouteToJson(IEnumerable<RouteStep> route)
    {
        JsonArray result = [];
        foreach (RouteStep step in route)
            result.Add(new JsonArray(step.X, step.Y));
        return result;
    }

    private static int getInt(JsonNode? value)
    {
        return tryGetInt(value, out int result) ? result : 0;
    }

    private static bool tryGetInt(JsonNode? value, out int result)
    {
        if (value is JsonValue scalar)
        {
            if (scalar.TryGetValue(out int integer))
            {
                result = integer;
                return true;
            }
            if (scalar.TryGetValue(out long longValue))
            {
                result = (int)Math.Clamp(longValue, int.MinValue, int.MaxValue);
                return true;
            }
            if (scalar.TryGetValue(out double number) && double.IsFinite(number))
            {
                result = (int)Math.Clamp(number, int.MinValue, int.MaxValue);
                return true;
            }
            if (scalar.TryGetValue(out string? text)
                && int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out integer))
            {
                result = integer;
                return true;
            }
        }
        result = 0;
        return false;
    }
}
