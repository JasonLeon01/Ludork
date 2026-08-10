using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.Json;

namespace Ludork.Plugins.OfficialRandomMap.Localization;

internal sealed class PluginLocalizer
{
    private readonly IReadOnlyDictionary<string, string> values;

    private PluginLocalizer(IReadOnlyDictionary<string, string> values)
    {
        this.values = values;
    }

    public static PluginLocalizer Load(string pluginDirectory, string language)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(pluginDirectory);
        string requestedPath = Path.Combine(
            pluginDirectory,
            "locales",
            $"{language}.json");
        string fallbackPath = Path.Combine(
            pluginDirectory,
            "locales",
            "en_GB.json");
        string path = File.Exists(requestedPath) ? requestedPath : fallbackPath;
        Dictionary<string, string>? result =
            JsonSerializer.Deserialize<Dictionary<string, string>>(
                File.ReadAllText(path));
        return new PluginLocalizer(
            result ?? new Dictionary<string, string>(StringComparer.Ordinal));
    }

    public string Text(string key)
    {
        return values.TryGetValue(key, out string? value) ? value : key;
    }

    public string Format(string key, params object[] arguments)
    {
        return string.Format(
            CultureInfo.CurrentCulture,
            Text(key),
            arguments);
    }
}
