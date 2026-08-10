using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.Json;

namespace Ludork.Plugins.OfficialLocaleTools;

internal sealed class PluginLocalizer
{
    private readonly IReadOnlyDictionary<string, IReadOnlyDictionary<string, string>> _languages;
    private readonly string _language;

    private PluginLocalizer(
        IReadOnlyDictionary<string, IReadOnlyDictionary<string, string>> languages,
        string language)
    {
        _languages = languages;
        _language = language;
    }

    public static PluginLocalizer Load(string localePath, string language)
    {
        using FileStream stream = File.OpenRead(localePath);
        using JsonDocument document = JsonDocument.Parse(stream);
        if (document.RootElement.ValueKind != JsonValueKind.Object)
        {
            throw new InvalidDataException("Plugin locale root must be an object.");
        }

        Dictionary<string, IReadOnlyDictionary<string, string>> languages =
            new Dictionary<string, IReadOnlyDictionary<string, string>>(StringComparer.Ordinal);
        foreach (JsonProperty languageProperty in document.RootElement.EnumerateObject())
        {
            if (languageProperty.Value.ValueKind != JsonValueKind.Object)
            {
                continue;
            }

            Dictionary<string, string> values = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (JsonProperty valueProperty in languageProperty.Value.EnumerateObject())
            {
                if (valueProperty.Value.ValueKind == JsonValueKind.String)
                {
                    values[valueProperty.Name] = valueProperty.Value.GetString() ?? string.Empty;
                }
            }
            languages[languageProperty.Name] = values;
        }

        string selectedLanguage = string.IsNullOrWhiteSpace(language) ? "en_GB" : language;
        return new PluginLocalizer(languages, selectedLanguage);
    }

    public string Text(string key)
    {
        if (_languages.TryGetValue(_language, out IReadOnlyDictionary<string, string>? selected) &&
            selected.TryGetValue(key, out string? selectedText))
        {
            return selectedText;
        }
        if (_languages.TryGetValue("en_GB", out IReadOnlyDictionary<string, string>? fallback) &&
            fallback.TryGetValue(key, out string? fallbackText))
        {
            return fallbackText;
        }
        return key;
    }

    public string Format(string key, params object[] arguments)
    {
        return string.Format(CultureInfo.CurrentCulture, Text(key), arguments);
    }
}
