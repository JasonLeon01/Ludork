using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.Json;

namespace Ludork.Plugins.OfficialBlueprintAI.Localization;

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
        string json = File.ReadAllText(path);
        Dictionary<string, string>? values =
            JsonSerializer.Deserialize<Dictionary<string, string>>(json);
        return new PluginLocalizer(
            values ?? new Dictionary<string, string>(StringComparer.Ordinal));
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
