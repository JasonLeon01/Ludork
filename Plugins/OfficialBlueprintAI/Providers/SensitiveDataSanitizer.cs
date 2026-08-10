using System;
using System.Text.RegularExpressions;

namespace Ludork.Plugins.OfficialBlueprintAI.Providers;

internal static class SensitiveDataSanitizer
{
    private static readonly Regex BearerTokenPattern = new(
        @"(?i)\bBearer\s+[A-Za-z0-9._~+/=-]+",
        RegexOptions.CultureInvariant |
        RegexOptions.Compiled);

    public static string Redact(string text, string apiKey)
    {
        string sanitized = text;
        if (!string.IsNullOrWhiteSpace(apiKey))
        {
            sanitized = sanitized.Replace(
                apiKey,
                "[REDACTED]",
                StringComparison.Ordinal);
        }

        return BearerTokenPattern.Replace(
            sanitized,
            "Bearer [REDACTED]");
    }

    public static InvalidOperationException Wrap(
        string prefix,
        Exception exception,
        string apiKey)
    {
        string message = Redact(exception.Message, apiKey);
        return new InvalidOperationException($"{prefix}: {message}");
    }
}
