using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.Json;

namespace Ludork.Plugin.Abstractions;

public sealed class PluginLocalizer
{
    private readonly IReadOnlyDictionary<string, string> values;
    private readonly IReadOnlyDictionary<string, string>? fallback;

    private PluginLocalizer(
        IReadOnlyDictionary<string, string> values,
        IReadOnlyDictionary<string, string>? fallback = null)
    {
        this.values = values;
        this.fallback = fallback;
    }

    public static PluginLocalizer LoadDirectory(string pluginDirectory, string language)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(pluginDirectory);
        string requestedPath = Path.Combine(pluginDirectory, "locales", $"{language}.json");
        string path = File.Exists(requestedPath)
            ? requestedPath
            : Path.Combine(pluginDirectory, "locales", "en_GB.json");
        Dictionary<string, string>? values =
            JsonSerializer.Deserialize<Dictionary<string, string>>(File.ReadAllText(path));
        return new PluginLocalizer(values ?? new Dictionary<string, string>(StringComparer.Ordinal));
    }

    public static PluginLocalizer LoadCatalog(string localePath, string language)
    {
        using FileStream stream = File.OpenRead(localePath);
        using JsonDocument document = JsonDocument.Parse(stream);
        if (document.RootElement.ValueKind != JsonValueKind.Object)
            throw new InvalidDataException("Plugin locale root must be an object.");
        Dictionary<string, IReadOnlyDictionary<string, string>> languages = new(StringComparer.Ordinal);
        foreach (JsonProperty languageProperty in document.RootElement.EnumerateObject())
        {
            if (languageProperty.Value.ValueKind != JsonValueKind.Object)
                continue;
            Dictionary<string, string> values = new(StringComparer.Ordinal);
            foreach (JsonProperty valueProperty in languageProperty.Value.EnumerateObject())
            {
                if (valueProperty.Value.ValueKind == JsonValueKind.String)
                    values[valueProperty.Name] = valueProperty.Value.GetString() ?? string.Empty;
            }
            languages[languageProperty.Name] = values;
        }
        string selectedLanguage = string.IsNullOrWhiteSpace(language) ? "en_GB" : language;
        languages.TryGetValue(selectedLanguage, out IReadOnlyDictionary<string, string>? selected);
        languages.TryGetValue("en_GB", out IReadOnlyDictionary<string, string>? fallback);
        return new PluginLocalizer(selected ?? new Dictionary<string, string>(StringComparer.Ordinal), fallback);
    }

    public string Text(string key)
    {
        if (values.TryGetValue(key, out string? value))
            return value;
        return fallback is not null && fallback.TryGetValue(key, out string? fallbackValue)
            ? fallbackValue
            : key;
    }

    public string Format(string key, params object[] arguments)
    {
        return string.Format(CultureInfo.CurrentCulture, Text(key), arguments);
    }
}
