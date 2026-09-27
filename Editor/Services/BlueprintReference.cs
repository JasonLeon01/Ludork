using System;

namespace Ludork.Services;

internal static class BlueprintReference
{
    private const string Prefix = "Data.Blueprints.";

    public static bool IsReference(string? value)
    {
        return normalizeInput(value).StartsWith(Prefix, StringComparison.Ordinal);
    }

    public static string NormalizeKey(string? value)
    {
        string key = normalizeInput(value);
        if (key.StartsWith(Prefix, StringComparison.Ordinal))
            return key[Prefix.Length..].Replace('.', '/').Trim('/');
        return key.EndsWith(DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase)
            ? key[..^DataConfig.DataFileExtension.Length]
            : key;
    }

    public static string ToReference(string? value)
    {
        string key = NormalizeKey(value);
        return key.Length == 0 ? string.Empty : Prefix + key.Replace('/', '.');
    }

    private static string normalizeInput(string? value)
    {
        return value?.Trim().Replace('\\', '/').Trim('/') ?? string.Empty;
    }
}
