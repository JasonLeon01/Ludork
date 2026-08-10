using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace Ludork.Services;

public sealed record GameConfigData(
    string Script,
    string Language,
    double Scale,
    int FrameRate,
    bool VerticalSync,
    bool MusicOn,
    bool SoundOn,
    bool VoiceOn,
    double MusicVolume,
    double SoundVolume,
    double VoiceVolume);

public sealed record GameConfigSaveResult(bool Success, string Detail)
{
    public static GameConfigSaveResult Completed(string detail) => new(true, detail);
    public static GameConfigSaveResult Failed(string detail) => new(false, detail);
}

public sealed class GameConfigService
{
    private static readonly GameConfigData defaults = new(
        "Scripts/Entry.lua",
        "en_GB",
        2.0,
        120,
        true,
        true,
        true,
        true,
        100.0,
        100.0,
        100.0);
    private readonly string projectPath;
    private readonly string iniPath;
    private GameConfigData savedData;
    private GameConfigData? pendingData;
    private string? loadError;

    public GameConfigService(string projectPath)
    {
        if (!Path.IsPathFullyQualified(projectPath))
            throw new ArgumentException(nameof(projectPath));
        this.projectPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(projectPath));
        iniPath = Path.Combine(this.projectPath, "Main.ini");
        savedData = loadData();
    }

    public string IniPath => iniPath;
    public GameConfigData SavedData => savedData;
    public GameConfigData CurrentData => pendingData ?? savedData;
    public GameConfigData? PendingData => pendingData;
    public string? LoadError => loadError;
    public bool IsModified => pendingData is not null;
    public event EventHandler? Changed;

    public IReadOnlyList<string> GetLanguageOptions()
    {
        SortedSet<string> languages = new(StringComparer.Ordinal);
        string localeDirectory = Path.Combine(projectPath, "Data", "Locale");
        if (Directory.Exists(localeDirectory))
        {
            foreach (string path in Directory.EnumerateFiles(localeDirectory, "*.lua", SearchOption.TopDirectoryOnly))
                languages.Add(Path.GetFileNameWithoutExtension(path));
        }
        string currentLanguage = CurrentData.Language.Trim();
        if (currentLanguage.Length != 0)
            languages.Add(currentLanguage);
        if (languages.Count == 0)
            languages.Add(defaults.Language);
        return languages.ToArray();
    }

    public void SetPending(GameConfigData data)
    {
        GameConfigData normalized = normalize(data);
        pendingData = normalized == savedData ? null : normalized;
        Changed?.Invoke(this, EventArgs.Empty);
    }

    public void SetPending(GameConfigData data, GameConfigData baseline)
    {
        GameConfigData normalized = normalize(data);
        GameConfigData normalizedBaseline = normalize(baseline);
        GameConfigData current = CurrentData;
        SetPending(current with
        {
            Script = normalized.Script == normalizedBaseline.Script
                ? current.Script
                : normalized.Script,
            Language = normalized.Language == normalizedBaseline.Language
                ? current.Language
                : normalized.Language,
            Scale = normalized.Scale == normalizedBaseline.Scale
                ? current.Scale
                : normalized.Scale,
            FrameRate = normalized.FrameRate == normalizedBaseline.FrameRate
                ? current.FrameRate
                : normalized.FrameRate,
            VerticalSync = normalized.VerticalSync == normalizedBaseline.VerticalSync
                ? current.VerticalSync
                : normalized.VerticalSync,
            MusicOn = normalized.MusicOn == normalizedBaseline.MusicOn
                ? current.MusicOn
                : normalized.MusicOn,
            SoundOn = normalized.SoundOn == normalizedBaseline.SoundOn
                ? current.SoundOn
                : normalized.SoundOn,
            VoiceOn = normalized.VoiceOn == normalizedBaseline.VoiceOn
                ? current.VoiceOn
                : normalized.VoiceOn,
            MusicVolume = normalized.MusicVolume == normalizedBaseline.MusicVolume
                ? current.MusicVolume
                : normalized.MusicVolume,
            SoundVolume = normalized.SoundVolume == normalizedBaseline.SoundVolume
                ? current.SoundVolume
                : normalized.SoundVolume,
            VoiceVolume = normalized.VoiceVolume == normalizedBaseline.VoiceVolume
                ? current.VoiceVolume
                : normalized.VoiceVolume,
        });
    }

    public void DiscardPending()
    {
        if (pendingData is null)
            return;
        pendingData = null;
        Changed?.Invoke(this, EventArgs.Empty);
    }

    public void Reload()
    {
        savedData = loadData();
        pendingData = null;
        Changed?.Invoke(this, EventArgs.Empty);
    }

    public GameConfigSaveResult SavePending()
    {
        if (pendingData is null)
            return GameConfigSaveResult.Completed(string.Empty);

        try
        {
            IniDocument document = loadDocument();
            setKnownValues(document, pendingData);
            File.WriteAllText(iniPath, document.ToText(), new UTF8Encoding(false));
        }
        catch (IOException exception)
        {
            return GameConfigSaveResult.Failed($"Main.ini({exception.Message})");
        }
        catch (UnauthorizedAccessException exception)
        {
            return GameConfigSaveResult.Failed($"Main.ini({exception.Message})");
        }

        savedData = pendingData;
        pendingData = null;
        loadError = null;
        Changed?.Invoke(this, EventArgs.Empty);
        return GameConfigSaveResult.Completed("Main.ini");
    }

    private GameConfigData loadData()
    {
        loadError = null;
        IniDocument document;
        try
        {
            document = loadDocument();
        }
        catch (IOException exception)
        {
            loadError = exception.Message;
            document = IniDocument.Parse(string.Empty);
        }
        catch (UnauthorizedAccessException exception)
        {
            loadError = exception.Message;
            document = IniDocument.Parse(string.Empty);
        }
        return normalize(new GameConfigData(
            text(document, "script", defaults.Script),
            text(document, "language", defaults.Language, true),
            number(document, "scale", defaults.Scale, 1.0),
            Math.Max(1, integer(document, "framerate", defaults.FrameRate, 60)),
            boolean(document, "verticalsync", defaults.VerticalSync, false),
            boolean(document, "musicon", defaults.MusicOn, true),
            boolean(document, "soundon", defaults.SoundOn, true),
            boolean(document, "voiceon", defaults.VoiceOn, true),
            number(document, "musicvolume", defaults.MusicVolume, 100.0),
            number(document, "soundvolume", defaults.SoundVolume, 100.0),
            number(document, "voicevolume", defaults.VoiceVolume, 100.0)));
    }

    private IniDocument loadDocument()
    {
        return File.Exists(iniPath)
            ? IniDocument.Parse(File.ReadAllText(iniPath, Encoding.UTF8))
            : IniDocument.Parse(string.Empty);
    }

    private static GameConfigData normalize(GameConfigData data)
    {
        string script = data.Script.Trim();
        string language = data.Language.Trim();
        return data with
        {
            Script = script.Length == 0 ? defaults.Script : script,
            Language = language,
            Scale = Math.Round(double.IsFinite(data.Scale) ? data.Scale : 1.0, 2),
            FrameRate = Math.Max(1, data.FrameRate),
            MusicVolume = volume(data.MusicVolume),
            SoundVolume = volume(data.SoundVolume),
            VoiceVolume = volume(data.VoiceVolume),
        };
    }

    private static double volume(double value)
    {
        if (!double.IsFinite(value))
            return 100.0;
        return Math.Round(Math.Clamp(value, 0.0, 100.0), 2);
    }

    private static string text(
        IniDocument document,
        string key,
        string fallback,
        bool preserveEmpty = false)
    {
        string? value = document.GetValue("Main", key);
        if (value is null)
            return fallback;
        string result = value?.Trim() ?? string.Empty;
        return result.Length == 0 && !preserveEmpty ? fallback : result;
    }

    private static double number(
        IniDocument document,
        string key,
        double missingFallback,
        double invalidFallback)
    {
        string? value = document.GetValue("Main", key);
        if (value is null)
            return missingFallback;
        return double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out double result)
            && double.IsFinite(result)
            ? result
            : invalidFallback;
    }

    private static int integer(
        IniDocument document,
        string key,
        int missingFallback,
        int invalidFallback)
    {
        string? value = document.GetValue("Main", key);
        if (value is null)
            return missingFallback;
        return int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out int result)
            ? result
            : invalidFallback;
    }

    private static bool boolean(
        IniDocument document,
        string key,
        bool missingFallback,
        bool invalidFallback)
    {
        string? configured = document.GetValue("Main", key);
        if (configured is null)
            return missingFallback;
        string value = configured.Trim();
        if (value.Equals("1", StringComparison.OrdinalIgnoreCase)
            || value.Equals("true", StringComparison.OrdinalIgnoreCase)
            || value.Equals("yes", StringComparison.OrdinalIgnoreCase)
            || value.Equals("on", StringComparison.OrdinalIgnoreCase))
            return true;
        if (value.Equals("0", StringComparison.OrdinalIgnoreCase)
            || value.Equals("false", StringComparison.OrdinalIgnoreCase)
            || value.Equals("no", StringComparison.OrdinalIgnoreCase)
            || value.Equals("off", StringComparison.OrdinalIgnoreCase))
            return false;
        return invalidFallback;
    }

    private static void setKnownValues(IniDocument document, GameConfigData data)
    {
        document.SetValue("Main", "script", data.Script);
        document.SetValue("Main", "language", data.Language);
        document.SetValue("Main", "scale", data.Scale.ToString("F2", CultureInfo.InvariantCulture));
        document.SetValue("Main", "framerate", data.FrameRate.ToString(CultureInfo.InvariantCulture));
        document.SetValue("Main", "verticalsync", boolText(data.VerticalSync));
        document.SetValue("Main", "musicon", boolText(data.MusicOn));
        document.SetValue("Main", "soundon", boolText(data.SoundOn));
        document.SetValue("Main", "voiceon", boolText(data.VoiceOn));
        document.SetValue("Main", "musicvolume", data.MusicVolume.ToString("F2", CultureInfo.InvariantCulture));
        document.SetValue("Main", "soundvolume", data.SoundVolume.ToString("F2", CultureInfo.InvariantCulture));
        document.SetValue("Main", "voicevolume", data.VoiceVolume.ToString("F2", CultureInfo.InvariantCulture));
    }

    private static string boolText(bool value) => value ? "true" : "false";

    private sealed class IniDocument
    {
        private readonly List<string> lines;
        private readonly string newline;
        private bool trailingNewline;

        private IniDocument(List<string> lines, string newline, bool trailingNewline)
        {
            this.lines = lines;
            this.newline = newline;
            this.trailingNewline = trailingNewline;
        }

        public static IniDocument Parse(string text)
        {
            string newline = text.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
            bool trailingNewline = text.EndsWith("\n", StringComparison.Ordinal)
                || text.EndsWith("\r", StringComparison.Ordinal);
            string normalized = text.Replace("\r\n", "\n", StringComparison.Ordinal).Replace('\r', '\n');
            List<string> lines = normalized.Length == 0
                ? []
                : normalized.Split('\n').ToList();
            if (trailingNewline && lines.Count > 0 && lines[^1].Length == 0)
                lines.RemoveAt(lines.Count - 1);
            return new IniDocument(lines, newline, trailingNewline);
        }

        public string? GetValue(string section, string key)
        {
            if (!findSection(section, out int headerIndex, out int endIndex))
                return null;
            for (int index = headerIndex + 1; index < endIndex; index += 1)
            {
                if (tryReadKey(lines[index], out string candidate, out int separator)
                    && candidate.Equals(key, StringComparison.OrdinalIgnoreCase))
                {
                    return lines[index][(separator + 1)..].Trim();
                }
            }
            return null;
        }

        public void SetValue(string section, string key, string value)
        {
            if (!findSection(section, out int headerIndex, out int endIndex))
            {
                if (lines.Count > 0 && lines[^1].Length != 0)
                    lines.Add(string.Empty);
                lines.Add($"[{section}]");
                lines.Add($"{key} = {value}");
                trailingNewline = true;
                return;
            }
            for (int index = headerIndex + 1; index < endIndex; index += 1)
            {
                string line = lines[index];
                if (!tryReadKey(line, out string candidate, out int separator)
                    || !candidate.Equals(key, StringComparison.OrdinalIgnoreCase))
                    continue;
                int valueIndex = separator + 1;
                while (valueIndex < line.Length && char.IsWhiteSpace(line[valueIndex]))
                    valueIndex += 1;
                lines[index] = line[..(separator + 1)] + line[(separator + 1)..valueIndex] + value;
                return;
            }
            lines.Insert(endIndex, $"{key} = {value}");
            trailingNewline = true;
        }

        public string ToText()
        {
            string result = string.Join(newline, lines);
            return trailingNewline && lines.Count > 0 ? result + newline : result;
        }

        private bool findSection(string section, out int headerIndex, out int endIndex)
        {
            headerIndex = -1;
            endIndex = lines.Count;
            for (int index = 0; index < lines.Count; index += 1)
            {
                string line = lines[index].Trim();
                if (line.Length < 3 || line[0] != '[' || line[^1] != ']')
                    continue;
                if (headerIndex >= 0)
                {
                    endIndex = index;
                    return true;
                }
                string name = line[1..^1].Trim();
                if (name.Equals(section, StringComparison.OrdinalIgnoreCase))
                    headerIndex = index;
            }
            return headerIndex >= 0;
        }

        private static bool tryReadKey(string line, out string key, out int separator)
        {
            key = string.Empty;
            separator = -1;
            string trimmed = line.TrimStart();
            if (trimmed.Length == 0 || trimmed[0] is '#' or ';')
                return false;
            int equal = line.IndexOf('=');
            int colon = line.IndexOf(':');
            separator = equal < 0 ? colon : colon < 0 ? equal : Math.Min(equal, colon);
            if (separator < 0)
                return false;
            key = line[..separator].Trim();
            return key.Length != 0;
        }
    }
}
