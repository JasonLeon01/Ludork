using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace Ludork.Services;

public sealed class EditorSettings
{
    private const int MaxRecentProjectCount = 3;
    private const string SectionName = "Ludork";

    private readonly List<string> recentProjectPaths = [];

    public int Width { get; set; } = 1512;
    public int Height { get; set; } = 982;
    public int UpperLeftWidth { get; set; } = 320;
    public int UpperRightWidth { get; set; } = 400;
    public int LowerLeftWidth { get; set; } = 320;
    public int LowerAreaHeight { get; set; } = 240;
    public string Language { get; set; } = getDefaultLanguage();
    public string LastOpenPath { get; set; } = string.Empty;
    public IReadOnlyList<string> RecentProjectPaths => recentProjectPaths;

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
        settings.LowerLeftWidth = readPositiveInt(values, "LowerLeftWidth", settings.LowerLeftWidth);
        settings.LowerAreaHeight = readPositiveInt(values, "LowerAreaHeight", settings.LowerAreaHeight);
        settings.Language = readText(values, "Language", settings.Language);
        settings.LastOpenPath = readText(values, "LastOpenPath", string.Empty);
        for (int index = 0; index < MaxRecentProjectCount; index++)
        {
            string projectFilePath = readText(values, $"RecentProject{index}", string.Empty);
            if (!string.IsNullOrWhiteSpace(projectFilePath))
                settings.recentProjectPaths.Add(projectFilePath);
        }
        return settings;
    }

    public string getLastPathOrHome()
    {
        if (!string.IsNullOrWhiteSpace(LastOpenPath) && Directory.Exists(LastOpenPath))
            return Path.GetFullPath(LastOpenPath);
        return Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
    }

    public void recordOpenedProject(string projectFilePath)
    {
        string fullPath = Path.GetFullPath(projectFilePath).Normalize(NormalizationForm.FormC);
        string projectPath = Path.GetDirectoryName(fullPath)
            ?? throw new ArgumentException("Project file path has no parent directory.", nameof(projectFilePath));
        LastOpenPath = projectPath;
        recentProjectPaths.RemoveAll(path => pathsEqual(path, fullPath));
        recentProjectPaths.Insert(0, fullPath);
        if (recentProjectPaths.Count > MaxRecentProjectCount)
            recentProjectPaths.RemoveRange(MaxRecentProjectCount, recentProjectPaths.Count - MaxRecentProjectCount);
        Save();
    }

    public void removeMissingRecentProjects()
    {
        List<string> validPaths = [];
        foreach (string projectFilePath in recentProjectPaths)
        {
            if (validPaths.Count >= MaxRecentProjectCount
                || !Path.IsPathFullyQualified(projectFilePath)
                || !projectFilePath.EndsWith(".proj", StringComparison.OrdinalIgnoreCase)
                || !File.Exists(projectFilePath))
            {
                continue;
            }
            string fullPath = Path.GetFullPath(projectFilePath).Normalize(NormalizationForm.FormC);
            if (!validPaths.Any(path => pathsEqual(path, fullPath)))
                validPaths.Add(fullPath);
        }

        if (recentProjectPaths.Count == validPaths.Count
            && recentProjectPaths.Zip(validPaths).All(pair => pathsEqual(pair.First, pair.Second)))
        {
            return;
        }
        recentProjectPaths.Clear();
        recentProjectPaths.AddRange(validPaths);
        Save();
    }

    public void Save()
    {
        List<string> lines =
        [
            $"[{SectionName}]",
            $"width = {Width}",
            $"height = {Height}",
            $"upperleftwidth = {UpperLeftWidth}",
            $"upperrightwidth = {UpperRightWidth}",
            $"lowerleftwidth = {LowerLeftWidth}",
            $"lowerareaheight = {LowerAreaHeight}",
            $"language = {Language}",
            $"lastopenpath = {LastOpenPath}",
        ];
        for (int index = 0; index < recentProjectPaths.Count; index++)
            lines.Add($"recentproject{index} = {recentProjectPaths[index]}");
        lines.Add(string.Empty);
        File.WriteAllLines(ConfigPath, lines, new UTF8Encoding(false));
    }

    private static bool pathsEqual(string left, string right)
    {
        StringComparison comparison = OperatingSystem.IsWindows() || OperatingSystem.IsMacOS()
            ? StringComparison.OrdinalIgnoreCase
            : StringComparison.Ordinal;
        string leftPath = Path.GetFullPath(left).Normalize(NormalizationForm.FormC);
        string rightPath = Path.GetFullPath(right).Normalize(NormalizationForm.FormC);
        return leftPath.Equals(rightPath, comparison);
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
