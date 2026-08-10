using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace Ludork.Services;

public static class LocaleService
{
    private static readonly Dictionary<string, Dictionary<string, string>> localeData =
        new(StringComparer.Ordinal);

    public static string CurrentLanguage { get; private set; } = "en_GB";
    public static event EventHandler? LanguageChanged;

    public static void Initialize(string requestedLanguage)
    {
        string localeDirectory = resolveLocaleDirectory(false);
        string jsonPath = Path.Combine(localeDirectory, "locale.json");
        if (File.Exists(jsonPath))
            exportJson(jsonPath, localeDirectory);
        localeData.Clear();
        foreach (string path in Directory.EnumerateFiles(localeDirectory))
        {
            if (Path.HasExtension(path))
                continue;
            localeData[Path.GetFileName(path)] = LocaleBinarySerializer.Read(path);
        }
        CurrentLanguage = localeData.ContainsKey(requestedLanguage) ? requestedLanguage : "en_GB";
    }

    public static void Compile()
    {
        string localeDirectory = resolveLocaleDirectory(true);
        exportJson(Path.Combine(localeDirectory, "locale.json"), localeDirectory);
    }

    public static string Get(string key)
    {
        if (localeData.TryGetValue(CurrentLanguage, out Dictionary<string, string>? current)
            && current.TryGetValue(key, out string? value))
            return value;
        if (localeData.TryGetValue("en_GB", out Dictionary<string, string>? fallback)
            && fallback.TryGetValue(key, out value))
            return value;
        return key;
    }

    public static IReadOnlyCollection<string> GetAvailableLanguages() => localeData.Keys;

    public static bool SetLanguage(string language)
    {
        if (!localeData.ContainsKey(language))
            return false;
        CurrentLanguage = language;
        LanguageChanged?.Invoke(null, EventArgs.Empty);
        return true;
    }

    private static string resolveLocaleDirectory(bool requireJson)
    {
        string developmentLocale = Path.GetFullPath(
            Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "Locale"));
        string[] candidates = OperatingSystem.IsMacOS()
            ?
            [
                Path.Combine(EditorRuntimePaths.ContentRoot, "Locale"),
                Path.Combine(Environment.CurrentDirectory, "Locale"),
                developmentLocale,
                Path.Combine(AppContext.BaseDirectory, "Locale"),
            ]
            :
            [
                Path.Combine(EditorRuntimePaths.ContentRoot, "Locale"),
                developmentLocale,
                Path.Combine(Environment.CurrentDirectory, "Locale"),
            ];
        foreach (string candidate in candidates)
        {
            if (File.Exists(Path.Combine(candidate, "locale.json")))
                return candidate;
            if (!requireJson
                && Directory.Exists(candidate)
                && Directory.EnumerateFiles(candidate).Any(path => !Path.HasExtension(path)))
                return candidate;
        }
        throw new DirectoryNotFoundException(
            requireJson
                ? "Locale/locale.json was not found."
                : "Compiled Locale data was not found.");
    }

    private static void exportJson(string jsonPath, string outputDirectory)
    {
        using JsonDocument document = JsonDocument.Parse(File.ReadAllText(jsonPath));
        Dictionary<string, Dictionary<string, string>> languageBuckets = new(StringComparer.Ordinal);
        foreach (JsonProperty entry in document.RootElement.EnumerateObject())
        {
            if (entry.Value.ValueKind != JsonValueKind.Object)
                continue;
            foreach (JsonProperty locale in entry.Value.EnumerateObject())
            {
                if (!languageBuckets.TryGetValue(locale.Name, out Dictionary<string, string>? bucket))
                {
                    bucket = new Dictionary<string, string>(StringComparer.Ordinal);
                    languageBuckets[locale.Name] = bucket;
                }
                bucket[entry.Name] = locale.Value.ValueKind == JsonValueKind.String
                    ? locale.Value.GetString() ?? string.Empty
                    : locale.Value.ToString();
            }
        }
        foreach (KeyValuePair<string, Dictionary<string, string>> pair in languageBuckets)
            LocaleBinarySerializer.Write(Path.Combine(outputDirectory, pair.Key), pair.Value);
    }
}
