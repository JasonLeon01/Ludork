using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Numerics;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace Ludork.Services;

public static class ProjectVersionService
{
    public static string? GetOpenError(string projectFilePath)
    {
        string supported = string.Join(", ", EditorVersionService.SupportedGameVersions);
        string? version;
        try
        {
            using FileStream stream = File.OpenRead(projectFilePath);
            using JsonDocument document = JsonDocument.Parse(stream);
            version = document.RootElement.ValueKind == JsonValueKind.Object
                && document.RootElement.TryGetProperty("version", out JsonElement value)
                && value.ValueKind == JsonValueKind.String
                    ? value.GetString()
                    : null;
        }
        catch (JsonException)
        {
            return string.Format(LocaleService.Get("PROJECT_VERSION_INVALID"), supported);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            return exception.Message;
        }

        if (!TryParse(version, out _))
            return string.Format(LocaleService.Get("PROJECT_VERSION_INVALID"), supported);
        return EditorVersionService.SupportedGameVersions.Contains(version, StringComparer.Ordinal)
            ? null
            : string.Format(LocaleService.Get("PROJECT_VERSION_UNSUPPORTED"), version, supported);
    }

    internal static bool TryParse(string? version, out (BigInteger Major, BigInteger Minor) result)
    {
        result = default;
        if (version is null)
            return false;
        Match match = Regex.Match(version, @"\Av(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\z");
        if (!match.Success)
            return false;
        result = (
            BigInteger.Parse(match.Groups[1].Value, CultureInfo.InvariantCulture),
            BigInteger.Parse(match.Groups[2].Value, CultureInfo.InvariantCulture));
        return true;
    }
}
