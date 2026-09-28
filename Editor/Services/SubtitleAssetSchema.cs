using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public static class SubtitleAssetSchema
{
    public const string AssetType = "subtitle";

    public static JsonObject Create() => new() { ["type"] = AssetType, ["sections"] = new JsonArray() };

    public static IReadOnlyList<string> Validate(JsonObject data, string key = "")
    {
        List<string> errors = [];
        string path = "Assets/Subtitles/" + key;
        if (data["type"] is not JsonValue type || !type.TryGetValue(out string? text) || text != AssetType)
            errors.Add(path + ".type must be subtitle");
        if (data["sections"] is not JsonArray sections)
        {
            errors.Add(path + ".sections must be an array");
            return errors;
        }
        List<(double Start, double End, int Index)> intervals = [];
        for (int index = 0; index < sections.Count; index++)
        {
            string sectionPath = path + ".sections[" + index + "]";
            if (sections[index] is not JsonObject section)
            {
                errors.Add(sectionPath + " must be an object");
                continue;
            }
            if (!tryNumber(section["startTime"], out double start)
                || !tryNumber(section["endTime"], out double end) || start < 0 || end <= start)
                errors.Add(sectionPath + " requires finite times with 0 <= startTime < endTime");
            else
                intervals.Add((start, end, index));
            if (section["content"] is JsonArray lines)
                validateLines(lines, sectionPath + ".content", errors);
            else if (section["content"] is JsonObject translations)
            {
                foreach (KeyValuePair<string, JsonNode?> language in translations)
                {
                    if (language.Value is JsonArray translatedLines)
                        validateLines(translatedLines, sectionPath + ".content." + language.Key, errors);
                    else
                        errors.Add(sectionPath + ".content." + language.Key + " must be a string array");
                }
            }
            else
                errors.Add(sectionPath + ".content must be a string array or language dictionary");
        }
        double lastEnd = 0;
        foreach ((double start, double end, int index) in intervals.OrderBy(interval => interval.Start))
        {
            if (start < lastEnd)
                errors.Add(path + ".sections[" + index + "] overlaps another section");
            lastEnd = Math.Max(lastEnd, end);
        }
        return errors;
    }

    public static JsonObject SortSections(JsonObject data)
    {
        JsonObject result = (JsonObject)data.DeepClone();
        if (result["sections"] is JsonArray sections)
            result["sections"] = new JsonArray(sections.OrderBy(section =>
                tryNumber(section!["startTime"], out double time) ? time : 0)
                .Select(section => section!.DeepClone()).ToArray());
        return result;
    }

    private static bool tryNumber(JsonNode? node, out double number)
    {
        number = 0;
        if (node is not JsonValue value)
            return false;
        if (value.TryGetValue(out number))
            return double.IsFinite(number);
        if (value.TryGetValue(out int integer))
            number = integer;
        else if (value.TryGetValue(out long longInteger))
            number = longInteger;
        else if (value.TryGetValue(out decimal decimalNumber))
            number = (double)decimalNumber;
        else if (value.TryGetValue(out float floatNumber))
            number = floatNumber;
        else
            return false;
        return double.IsFinite(number);
    }

    private static void validateLines(JsonArray lines, string path, ICollection<string> errors)
    {
        for (int index = 0; index < lines.Count; index++)
            if (lines[index] is not JsonValue value || !value.TryGetValue(out string? _))
                errors.Add(path + "[" + index + "] must be a string");
    }
}
