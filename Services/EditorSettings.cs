using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

namespace Ludork.Services;

public sealed class EditorSettings
{
    private const string SectionName = "Ludork";

    public int Width { get; set; } = 1280;
    public int Height { get; set; } = 720;
    public int UpperLeftWidth { get; set; } = 320;
    public int UpperRightWidth { get; set; } = 320;
    public int LowerAreaHeight { get; set; } = 240;
    public string Language { get; set; } = getDefaultLanguage();
    public string LastOpenPath { get; set; } = string.Empty;

    public static string ConfigPath => EditorPaths.IniFilePath;

    public static EditorSettings Load()
    {
        EditorSettings settings = new EditorSettings();
        if (!File.Exists(ConfigPath))
        {
            settings.Save();
            return settings;
        }

        Dictionary<string, string> values = readSection(ConfigPath, SectionName);
        settings.Width = readPositiveInt(values, "Width", settings.Width);
        settings.Height = readPositiveInt(values, "Height", settings.Height);
        settings.UpperLeftWidth = readPositiveInt(values, "UpperLeftWidth", settings.UpperLeftWidth);
        settings.UpperRightWidth = readPositiveInt(values, "UpperRightWidth", settings.UpperRightWidth);
        settings.LowerAreaHeight = readPositiveInt(values, "LowerAreaHeight", settings.LowerAreaHeight);
        settings.Language = readText(values, "Language", settings.Language);
        settings.LastOpenPath = readText(values, "LastOpenPath", string.Empty);
        return settings;
    }

    public string getLastPathOrHome()
    {
        if (!string.IsNullOrWhiteSpace(LastOpenPath) && Directory.Exists(LastOpenPath))
            return Path.GetFullPath(LastOpenPath);
        return Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
    }

    public void setLastOpenPath(string projectPath)
    {
        LastOpenPath = Path.GetFullPath(projectPath);
        Save();
    }

    public void Save()
    {
        string[] lines =
        [
            $"[{SectionName}]",
            $"width = {Width}",
            $"height = {Height}",
            $"upperleftwidth = {UpperLeftWidth}",
            $"upperrightwidth = {UpperRightWidth}",
            $"lowerareaheight = {LowerAreaHeight}",
            $"language = {Language}",
            $"lastopenpath = {LastOpenPath}",
            string.Empty,
        ];
        File.WriteAllLines(ConfigPath, lines, new UTF8Encoding(false));
    }

    private static Dictionary<string, string> readSection(string path, string sectionName)
    {
        Dictionary<string, string> values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        bool inSection = false;
        foreach (string rawLine in File.ReadLines(path, Encoding.UTF8))
        {
            string line = rawLine.Trim();
            if (line.Length == 0 || line.StartsWith(';') || line.StartsWith('#'))
                continue;
            if (line.StartsWith('[') && line.EndsWith(']'))
            {
                inSection = string.Equals(line[1..^1].Trim(), sectionName, StringComparison.OrdinalIgnoreCase);
                continue;
            }
            if (!inSection)
                continue;
            int separator = line.IndexOf('=');
            if (separator <= 0)
                continue;
            values[line[..separator].Trim()] = line[(separator + 1)..].Trim();
        }
        return values;
    }

    private static int readPositiveInt(IReadOnlyDictionary<string, string> values, string key, int defaultValue)
    {
        return values.TryGetValue(key, out string? value) && int.TryParse(value, out int parsed) && parsed > 0
            ? parsed
            : defaultValue;
    }

    private static string readText(IReadOnlyDictionary<string, string> values, string key, string defaultValue)
    {
        return values.TryGetValue(key, out string? value) && !string.IsNullOrWhiteSpace(value)
            ? value.Trim()
            : defaultValue;
    }

    private static string getDefaultLanguage()
    {
        string language = CultureInfo.CurrentCulture.Name.Replace('-', '_');
        return language is "en_GB" or "zh_CN" ? language : "en_GB";
    }
}
