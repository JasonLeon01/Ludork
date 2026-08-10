using System;
using System.Text.RegularExpressions;

namespace Ludork.Plugins.OfficialBlueprintAI.History;

public static class BlueprintAssistantText
{
    private static readonly Regex BearerPattern = new(
        "(Authorization\\s*:\\s*Bearer\\s+)[^\\s,;]+",
        RegexOptions.IgnoreCase |
        RegexOptions.CultureInvariant |
        RegexOptions.Compiled);
    private static readonly Regex ApiKeyPattern = new(
        "((?:api[-_ ]?key|x-api-key)\\s*[:=]\\s*)[^\\s,;]+",
        RegexOptions.IgnoreCase |
        RegexOptions.CultureInvariant |
        RegexOptions.Compiled);
    private static readonly Regex OpenAiKeyPattern = new(
        "\\bsk-[A-Za-z0-9_-]{12,}\\b",
        RegexOptions.CultureInvariant |
        RegexOptions.Compiled);

    public static string SanitizeError(string? message)
    {
        string value = message ?? string.Empty;
        value = BearerPattern.Replace(value, "$1<redacted>");
        value = ApiKeyPattern.Replace(value, "$1<redacted>");
        value = OpenAiKeyPattern.Replace(value, "<redacted>");
        return value;
    }
}
