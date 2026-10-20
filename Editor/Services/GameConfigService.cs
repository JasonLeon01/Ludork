using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed record GameConfigData(
    string Language,
    double Scale,
    double MaximumRenderScale,
    double LightingRenderScale,
    int FrameRate,
    int AntiAliasingLevel,
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
    private const int DefaultAntiAliasingLevel = 8;
    private const int SfmlDefaultAntiAliasingLevel = 0;
    private static readonly GameConfigData defaults = new(
        "en_GB",
        0.0,
        2.0,
        1.0,
        60,
        DefaultAntiAliasingLevel,
        true,
        true,
        true,
        true,
        100.0,
        100.0,
        100.0);
    private readonly string projectPath;
    private readonly SemaphoreSlim operationGate = new(1, 1);
    private string? configPath;
    private GameConfigData savedData = defaults;
    private string? loadError;

    public GameConfigService(string projectPath)
    {
        if (!Path.IsPathFullyQualified(projectPath))
            throw new ArgumentException(nameof(projectPath));
        this.projectPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(projectPath));
    }

    public string? ConfigPath => configPath;
    public GameConfigData SavedData => savedData;
    public GameConfigData CurrentData => savedData;
    public string? LoadError => loadError;
    public event EventHandler? Changed;

    public IReadOnlyList<string> GetLanguageOptions()
    {
        SortedSet<string> languages = new(StringComparer.Ordinal);
        string localeDirectory = Path.Combine(projectPath, "Scripts", "Source", "Locale");
        if (Directory.Exists(localeDirectory))
        {
            foreach (string path in Directory.EnumerateFiles(localeDirectory, "*.lua", SearchOption.TopDirectoryOnly))
            {
                string language = Path.GetFileNameWithoutExtension(path);
                if (!language.Equals("Core", StringComparison.OrdinalIgnoreCase))
                    languages.Add(language);
            }
        }
        string currentLanguage = CurrentData.Language.Trim();
        if (currentLanguage.Length != 0)
            languages.Add(currentLanguage);
        if (languages.Count == 0)
            languages.Add(defaults.Language);
        return languages.ToArray();
    }

    public async Task<GameConfigSaveResult> ConfirmAsync(
        GameConfigData data,
        GameConfigData baseline,
        string expectedConfigPath,
        CancellationToken cancellationToken = default)
    {
        await operationGate.WaitAsync(cancellationToken);
        try
        {
            await reloadAsync(cancellationToken);
            if (loadError is not null)
                return GameConfigSaveResult.Failed(loadError);
            if (configPath is null || !configPath.Equals(expectedConfigPath, StringComparison.Ordinal))
                return GameConfigSaveResult.Failed(LocaleService.Get("GAME_CONFIG_IDENTITY_CHANGED"));
            cancellationToken.ThrowIfCancellationRequested();
            return confirm(data, baseline, configPath);
        }
        finally
        {
            operationGate.Release();
        }
    }

    private GameConfigSaveResult confirm(GameConfigData data, GameConfigData baseline, string path)
    {
        GameConfigData normalized = normalize(data);
        GameConfigData normalizedBaseline = normalize(baseline);
        GameConfigData current = CurrentData;
        GameConfigData confirmed = current with
        {
            Language = normalized.Language == normalizedBaseline.Language
                ? current.Language
                : normalized.Language,
            Scale = normalized.Scale == normalizedBaseline.Scale
                ? current.Scale
                : normalized.Scale,
            MaximumRenderScale = normalized.MaximumRenderScale == normalizedBaseline.MaximumRenderScale
                ? current.MaximumRenderScale
                : normalized.MaximumRenderScale,
            LightingRenderScale = normalized.LightingRenderScale == normalizedBaseline.LightingRenderScale
                ? current.LightingRenderScale
                : normalized.LightingRenderScale,
            FrameRate = normalized.FrameRate == normalizedBaseline.FrameRate
                ? current.FrameRate
                : normalized.FrameRate,
            AntiAliasingLevel =
                normalized.AntiAliasingLevel == normalizedBaseline.AntiAliasingLevel
                    ? current.AntiAliasingLevel
                    : normalized.AntiAliasingLevel,
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
        };
        if (confirmed == savedData && File.Exists(path))
            return GameConfigSaveResult.Completed(string.Empty);

        string? failure = null;
        try
        {
            ConfigDocument document = loadDocument(path);
            setKnownValues(document, confirmed);
            FilePersistence.WriteAllTextAtomic(path, document.ToText());
        }
        catch (IOException exception)
        {
            failure = exception.Message;
        }
        catch (UnauthorizedAccessException exception)
        {
            failure = exception.Message;
        }
        catch (InvalidDataException exception)
        {
            failure = exception.Message;
        }

        if (failure is not null)
            return GameConfigSaveResult.Failed($"{Path.GetFileName(path)}({failure})");
        savedData = confirmed;
        loadError = null;
        Changed?.Invoke(this, EventArgs.Empty);
        return GameConfigSaveResult.Completed(Path.GetFileName(path));
    }

    public async Task ReloadAsync(CancellationToken cancellationToken = default)
    {
        await operationGate.WaitAsync(cancellationToken);
        try
        {
            await reloadAsync(cancellationToken);
        }
        finally
        {
            operationGate.Release();
        }
    }

    private async Task reloadAsync(CancellationToken cancellationToken)
    {
        string? path = null;
        string? error = null;
        GameConfigData data = defaults;
        try
        {
            ProjectPackageMetadataResult result = await ProjectPackageMetadataService.ReadRuntimeAppNameAsync(
                projectPath, cancellationToken);
            if (result.ExitCode != 0)
                error = !string.IsNullOrWhiteSpace(result.Error) ? result.Error
                    : !string.IsNullOrWhiteSpace(result.Output) ? result.Output
                    : "The game's APP_NAME could not be resolved.";
            else if (result.Output.Length == 0)
                error = "The game's APP_NAME could not be resolved.";
            else
            {
                path = GameUserDataPaths.GetMainConfigPath(result.Output);
                data = loadData(loadDocument(path));
            }
        }
        catch (IOException exception)
        {
            error = exception.Message;
        }
        catch (UnauthorizedAccessException exception)
        {
            error = exception.Message;
        }
        catch (InvalidDataException exception)
        {
            error = exception.Message;
        }
        catch (ArgumentException exception)
        {
            error = exception.Message;
        }
        configPath = path;
        savedData = data;
        loadError = error;
        Changed?.Invoke(this, EventArgs.Empty);
    }

    private static GameConfigData loadData(ConfigDocument document)
    {
        return normalize(new GameConfigData(
            text(document, "language", defaults.Language, true),
            number(document, "scale", defaults.Scale, 1.0),
            number(document, "maxrenderscale", defaults.MaximumRenderScale, 2.0),
            number(document, "lightingrenderscale", defaults.LightingRenderScale, 1.0),
            integer(document, "framerate", defaults.FrameRate, 30),
            Math.Max(0, integer(
                document,
                "antialiasinglevel",
                defaults.AntiAliasingLevel,
                SfmlDefaultAntiAliasingLevel)),
            boolean(document, "verticalsync", defaults.VerticalSync, false),
            boolean(document, "musicon", defaults.MusicOn, true),
            boolean(document, "soundon", defaults.SoundOn, true),
            boolean(document, "voiceon", defaults.VoiceOn, true),
            number(document, "musicvolume", defaults.MusicVolume, 100.0),
            number(document, "soundvolume", defaults.SoundVolume, 100.0),
            number(document, "voicevolume", defaults.VoiceVolume, 100.0)));
    }

    private static ConfigDocument loadDocument(string path)
    {
        try
        {
            return ConfigDocument.Parse(path, ConfigDocument.ReadText(path));
        }
        catch (FileNotFoundException)
        {
            return ConfigDocument.Create(path);
        }
        catch (DirectoryNotFoundException)
        {
            return ConfigDocument.Create(path);
        }
    }

    private static GameConfigData normalize(GameConfigData data)
    {
        string language = data.Language.Trim();
        return data with
        {
            Language = language,
            Scale = Math.Round(double.IsFinite(data.Scale) && data.Scale >= 0.0
                ? data.Scale
                : 1.0, 2),
            MaximumRenderScale = double.IsFinite(data.MaximumRenderScale) && data.MaximumRenderScale >= 0.0
                ? data.MaximumRenderScale
                : 2.0,
            LightingRenderScale = lightingRenderScale(data.LightingRenderScale),
            FrameRate = data.FrameRate < 0 ? 1 : data.FrameRate,
            AntiAliasingLevel = Math.Max(0, data.AntiAliasingLevel),
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

    private static double lightingRenderScale(double value)
    {
        return value is 0.5 or 0.75 or 1.0 ? value : 1.0;
    }

    private static string text(
        ConfigDocument document,
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
        ConfigDocument document,
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
        ConfigDocument document,
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
        ConfigDocument document,
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

    private static void setKnownValues(ConfigDocument document, GameConfigData data)
    {
        document.SetValue("Main", "language", data.Language);
        document.SetValue("Main", "scale", data.Scale.ToString("F2", CultureInfo.InvariantCulture));
        document.SetValue(
            "Main",
            "maxrenderscale",
            data.MaximumRenderScale.ToString("G17", CultureInfo.InvariantCulture));
        document.SetValue(
            "Main",
            "lightingrenderscale",
            data.LightingRenderScale.ToString("G17", CultureInfo.InvariantCulture));
        document.SetValue("Main", "framerate", data.FrameRate.ToString(CultureInfo.InvariantCulture));
        document.SetValue(
            "Main",
            "antialiasinglevel",
            data.AntiAliasingLevel.ToString(CultureInfo.InvariantCulture));
        document.SetValue("Main", "verticalsync", boolText(data.VerticalSync));
        document.SetValue("Main", "musicon", boolText(data.MusicOn));
        document.SetValue("Main", "soundon", boolText(data.SoundOn));
        document.SetValue("Main", "voiceon", boolText(data.VoiceOn));
        document.SetValue("Main", "musicvolume", data.MusicVolume.ToString("F2", CultureInfo.InvariantCulture));
        document.SetValue("Main", "soundvolume", data.SoundVolume.ToString("F2", CultureInfo.InvariantCulture));
        document.SetValue("Main", "voicevolume", data.VoiceVolume.ToString("F2", CultureInfo.InvariantCulture));
    }

    private static string boolText(bool value) => value ? "true" : "false";
}
