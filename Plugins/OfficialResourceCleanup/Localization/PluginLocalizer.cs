using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.Json;

namespace Ludork.Plugins.OfficialResourceCleanup.Localization;

internal sealed class PluginLocalizer
{
    private readonly IReadOnlyDictionary<string, string> values;

    private PluginLocalizer(IReadOnlyDictionary<string, string> values)
    {
        this.values = values;
    }

    public static PluginLocalizer Load(string pluginDirectory, string language)
    {
        string requested = Path.Combine(pluginDirectory, "locales", language + ".json");
        string path = File.Exists(requested)
            ? requested : Path.Combine(pluginDirectory, "locales", "en_GB.json");
        Dictionary<string, string> values =
            JsonSerializer.Deserialize<Dictionary<string, string>>(File.ReadAllText(path))
            ?? new Dictionary<string, string>(StringComparer.Ordinal);
        return new PluginLocalizer(values);
    }

    public string Text(string key) => values.TryGetValue(key, out string? value) ? value : key;

    public string Format(string key, params object[] arguments) =>
        string.Format(CultureInfo.CurrentCulture, Text(key), arguments);

    public string Size(long bytes)
    {
        string[] units = ["B", "KiB", "MiB", "GiB", "TiB"];
        double value = bytes;
        int unit = 0;
        while (value >= 1024 && unit < units.Length - 1)
        {
            value /= 1024;
            unit++;
        }
        return value.ToString(unit == 0 ? "N0" : "N1", CultureInfo.CurrentCulture) + " " + units[unit];
    }
}
