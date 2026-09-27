using Avalonia.Controls;
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
    private readonly Dictionary<string, Dictionary<string, string>> sections = new(StringComparer.OrdinalIgnoreCase);
    private readonly string? configPath;
    private string? savedText;

    public EditorSettings(string? path = null)
    {
        configPath = path;
    }

    public int Width { get; set; } = 1512;
    public int Height { get; set; } = 982;
    public int UpperLeftWidth { get; set; } = 320;
    public int UpperRightWidth { get; set; } = 400;
    public int LowerAreaHeight { get; set; } = 240;
    public string Language { get; set; } = getDefaultLanguage();
    public string LastOpenPath { get; set; } = string.Empty;
    public bool FileExplorerIconView { get; set; } = true;
    public double FileExplorerZoom { get; set; } = 100;
    public bool ActorLibraryIconView { get; set; } = true;
    public bool FileExplorerSourcesExpanded { get; set; }
    public EditorExternalOpenTarget FileExplorerOpenTarget { get; set; }
    public bool LightActorSelection { get; set; }
    public IReadOnlyList<string> RecentProjectPaths => recentProjectPaths;
    public string? LastError { get; private set; }
    public event EventHandler<string>? SaveFailed;
    public static string ConfigPath => EditorPaths.IniFilePath;

    public static EditorSettings Load(string? path = null)
    {
        EditorSettings settings = new(path);
        try
        {
            string resolvedPath = path ?? ConfigPath;
            if (!File.Exists(resolvedPath))
            {
                settings.Save();
                return settings;
            }
            string text = File.ReadAllText(resolvedPath, Encoding.UTF8);
            settings.readSections(text);
            settings.savedText = text;
        }
        catch (IOException exception)
        {
            settings.LastError = exception.Message;
        }
        catch (UnauthorizedAccessException exception)
        {
            settings.LastError = exception.Message;
        }
        Dictionary<string, string> values = settings.section(SectionName);
        settings.Width = readPositiveInt(values, "width", settings.Width);
        settings.Height = readPositiveInt(values, "height", settings.Height);
        settings.UpperLeftWidth = readPositiveInt(values, "upperleftwidth", settings.UpperLeftWidth);
        settings.UpperRightWidth = readPositiveInt(values, "upperrightwidth", settings.UpperRightWidth);
        settings.LowerAreaHeight = readPositiveInt(values, "lowerareaheight", settings.LowerAreaHeight);
        settings.Language = readText(values, "language", settings.Language);
        settings.LastOpenPath = readText(values, "lastopenpath", string.Empty);
        for (int index = 0; index < MaxRecentProjectCount; index++)
        {
            string projectFilePath = readText(values, $"recentproject{index}", string.Empty);
            if (!string.IsNullOrWhiteSpace(projectFilePath))
                settings.recentProjectPaths.Add(projectFilePath);
        }
        Dictionary<string, string> explorer = settings.section("FileExplorer");
        settings.FileExplorerIconView = readBool(explorer, "iconview", true);
        double zoom = readPositiveDouble(explorer, "zoom");
        settings.FileExplorerZoom = zoom > 0 ? Math.Clamp(zoom, 50, 200) : 100;
        settings.ActorLibraryIconView = readBool(settings.section("ActorLibrary"), "iconview", true);
        settings.FileExplorerSourcesExpanded = readBool(explorer, "sourcesexpanded", false);
        if (Enum.TryParse(readText(explorer, "opentarget", "Folder"), true, out EditorExternalOpenTarget target)
            && Enum.IsDefined(target))
            settings.FileExplorerOpenTarget = target;
        settings.LightActorSelection = readBool(settings.section("MapEditor"), "lightactorselection", false);
        return settings;
    }

    public EditorWindowState? GetWindowState(string key)
    {
        Dictionary<string, string> values = section(key == "Main" ? SectionName : "Window." + key);
        double width = key == "Main" ? Width : readPositiveDouble(values, "width");
        double height = key == "Main" ? Height : readPositiveDouble(values, "height");
        if (width <= 0 || height <= 0)
            return null;
        return new EditorWindowState(width, height, readInt(values, "x"), readInt(values, "y"),
            readBool(values, "maximized", false));
    }

    public void SetWindowState(string key, EditorWindowState state)
    {
        Dictionary<string, string> values = section(key == "Main" ? SectionName : "Window." + key);
        values["width"] = state.Width.ToString("R", CultureInfo.InvariantCulture);
        values["height"] = state.Height.ToString("R", CultureInfo.InvariantCulture);
        if (key == "Main")
        {
            Width = (int)Math.Round(state.Width);
            Height = (int)Math.Round(state.Height);
        }
        if (state.X is int x && state.Y is int y)
        {
            values["x"] = x.ToString(CultureInfo.InvariantCulture);
            values["y"] = y.ToString(CultureInfo.InvariantCulture);
        }
        values["maximized"] = state.Maximized.ToString();
    }

    public GridLength[]? GetPanelLayout(string key)
    {
        string[] parts = readText(section("Layouts"), key, string.Empty).Split(',');
        if (parts.Length == 0 || parts.Length > 16)
            return null;
        GridLength[] lengths = new GridLength[parts.Length];
        for (int index = 0; index < parts.Length; index++)
        {
            string part = parts[index].Trim();
            if (part.Equals("Auto", StringComparison.OrdinalIgnoreCase))
            {
                lengths[index] = GridLength.Auto;
                continue;
            }
            bool star = part.EndsWith('*');
            string number = star ? part[..^1] : part;
            if (!double.TryParse(number, NumberStyles.Float, CultureInfo.InvariantCulture, out double value)
                || !double.IsFinite(value) || value < 0 || value > 100000)
                return null;
            lengths[index] = new GridLength(value, star ? GridUnitType.Star : GridUnitType.Pixel);
        }
        return lengths;
    }

    public void SetPanelLayout(string key, GridLength[] lengths)
    {
        section("Layouts")[key] = string.Join(",", lengths.Select(length => length.IsAuto ? "Auto"
            : length.Value.ToString("R", CultureInfo.InvariantCulture) + (length.IsStar ? "*" : string.Empty)));
    }

    public bool GetExpanded(string key, bool defaultValue) => readBool(section("Expanded"), key, defaultValue);

    public void SetExpanded(string key, bool expanded) => section("Expanded")[key] = expanded.ToString();

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

    public bool Save()
    {
        string? temporaryPath = null;
        try
        {
            string path = configPath ?? ConfigPath;
            string text = serialize();
            if (text == savedText && File.Exists(path))
                return true;
            string? directory = Path.GetDirectoryName(Path.GetFullPath(path));
            Directory.CreateDirectory(directory!);
            temporaryPath = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
            File.WriteAllText(temporaryPath, text, new UTF8Encoding(false));
            File.Move(temporaryPath, path, true);
            savedText = text;
            LastError = null;
            return true;
        }
        catch (IOException exception)
        {
            reportFailure(exception.Message);
            return false;
        }
        catch (UnauthorizedAccessException exception)
        {
            reportFailure(exception.Message);
            return false;
        }
        finally
        {
            if (temporaryPath is not null)
            {
                try
                {
                    File.Delete(temporaryPath);
                }
                catch (IOException exception)
                {
                    reportFailure(exception.Message);
                }
                catch (UnauthorizedAccessException exception)
                {
                    reportFailure(exception.Message);
                }
            }
        }
    }

    private void reportFailure(string message)
    {
        bool changed = LastError != message;
        LastError = message;
        if (changed)
            SaveFailed?.Invoke(this, message);
    }

    private string serialize()
    {
        Dictionary<string, string> main = section(SectionName);
        main["width"] = Width.ToString(CultureInfo.InvariantCulture);
        main["height"] = Height.ToString(CultureInfo.InvariantCulture);
        main["upperleftwidth"] = UpperLeftWidth.ToString(CultureInfo.InvariantCulture);
        main["upperrightwidth"] = UpperRightWidth.ToString(CultureInfo.InvariantCulture);
        main.Remove("lowerleftwidth");
        main["lowerareaheight"] = LowerAreaHeight.ToString(CultureInfo.InvariantCulture);
        main["language"] = Language;
        main["lastopenpath"] = LastOpenPath;
        main.TryAdd("maximized", "False");
        for (int index = 0; index < MaxRecentProjectCount; index++)
        {
            if (index < recentProjectPaths.Count)
                main[$"recentproject{index}"] = recentProjectPaths[index];
            else
                main.Remove($"recentproject{index}");
        }
        Dictionary<string, string> explorer = section("FileExplorer");
        explorer["iconview"] = FileExplorerIconView.ToString();
        explorer["zoom"] = FileExplorerZoom.ToString("R", CultureInfo.InvariantCulture);
        section("ActorLibrary")["iconview"] = ActorLibraryIconView.ToString();
        explorer["sourcesexpanded"] = FileExplorerSourcesExpanded.ToString();
        explorer["opentarget"] = FileExplorerOpenTarget.ToString();
        section("MapEditor")["lightactorselection"] = LightActorSelection.ToString();
        StringBuilder text = new();
        foreach (KeyValuePair<string, Dictionary<string, string>> entry in sections)
        {
            if (entry.Value.Count == 0)
                continue;
            text.Append('[').Append(entry.Key).AppendLine("]");
            foreach (KeyValuePair<string, string> value in entry.Value)
                text.Append(value.Key).Append(" = ").AppendLine(value.Value);
            text.AppendLine();
        }
        return text.ToString();
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

    private Dictionary<string, string> section(string name)
    {
        if (!sections.TryGetValue(name, out Dictionary<string, string>? values))
        {
            values = new(StringComparer.OrdinalIgnoreCase);
            sections.Add(name, values);
        }
        return values;
    }

    private void readSections(string text)
    {
        Dictionary<string, string>? values = null;
        using StringReader reader = new(text);
        while (reader.ReadLine() is string rawLine)
        {
            string line = rawLine.Trim();
            if (line.Length == 0 || line.StartsWith(';') || line.StartsWith('#'))
                continue;
            if (line.StartsWith('[') && line.EndsWith(']'))
            {
                values = section(line[1..^1].Trim());
                continue;
            }
            int separator = line.IndexOf('=');
            if (values is not null && separator > 0)
                values[line[..separator].Trim()] = line[(separator + 1)..].Trim();
        }
    }

    private static int? readInt(IReadOnlyDictionary<string, string> values, string key)
    {
        return values.TryGetValue(key, out string? value)
            && int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out int parsed) ? parsed : null;
    }

    private static int readPositiveInt(IReadOnlyDictionary<string, string> values, string key, int defaultValue)
    {
        return readInt(values, key) is int value && value > 0 && value <= 100000 ? value : defaultValue;
    }

    private static double readPositiveDouble(IReadOnlyDictionary<string, string> values, string key)
    {
        return values.TryGetValue(key, out string? value)
            && double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out double parsed)
            && double.IsFinite(parsed) && parsed > 0 && parsed <= 100000 ? parsed : 0;
    }

    private static bool readBool(IReadOnlyDictionary<string, string> values, string key, bool defaultValue)
    {
        return values.TryGetValue(key, out string? value) && bool.TryParse(value, out bool parsed) ? parsed : defaultValue;
    }

    private static string readText(IReadOnlyDictionary<string, string> values, string key, string defaultValue)
    {
        return values.TryGetValue(key, out string? value) && !string.IsNullOrWhiteSpace(value) ? value.Trim() : defaultValue;
    }

    private static string getDefaultLanguage()
    {
        string language = CultureInfo.CurrentCulture.Name.Replace('-', '_');
        return language is "en_GB" or "zh_CN" ? language : "en_GB";
    }
}
