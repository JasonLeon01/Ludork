using Ludork.Plugin.Abstractions;
using System;
using System.Diagnostics;
using System.IO;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public static class LudorkServerConfiguration
{
    private static readonly JsonSerializerOptions jsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
    };

    public static string GetTestConfigPath(string projectPath)
    {
        string root = OperatingSystem.IsWindows()
            ? EditorPaths.IniDirectory
            : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Ludork");
        string projectName = Path.GetFileName(Path.TrimEndingDirectorySeparator(Path.GetFullPath(projectPath)));
        return Path.Combine(root, ".localserver", projectName, "test.json");
    }

    public static LudorkServerSettings ReadTestSettings(string projectPath)
    {
        string path = GetTestConfigPath(projectPath);
        if (!File.Exists(path))
        {
            string projectName = Path.GetFileName(Path.TrimEndingDirectorySeparator(Path.GetFullPath(projectPath)));
            return new(false, "http://localhost:7777/" + Uri.EscapeDataString(projectName), string.Empty);
        }
        LudorkServerSettings? settings = JsonSerializer.Deserialize<LudorkServerSettings>(File.ReadAllText(path), jsonOptions);
        if (settings is null)
            throw new InvalidDataException(LocaleService.Get("LUDORK_SERVER_TEST_LOAD_FAILED"));
        settings = settings with { Url = settings.Url ?? string.Empty, Key = settings.Key ?? string.Empty };
        if (settings.GetValidationError() is not null)
            throw new InvalidDataException(LocaleService.Get("LUDORK_SERVER_TEST_LOAD_FAILED"));
        return settings;
    }

    public static Task SaveTestSettingsAsync(
        string projectPath,
        LudorkServerSettings settings,
        CancellationToken cancellationToken = default)
    {
        string? error = settings.GetValidationError();
        if (error is not null)
            throw new InvalidDataException(LocaleService.Get(error));
        return FilePersistence.WriteAllTextAtomicAsync(
            GetTestConfigPath(projectPath),
            JsonSerializer.Serialize(settings, jsonOptions) + Environment.NewLine,
            cancellationToken);
    }

    public static void ApplyTestEnvironment(ProcessStartInfo startInfo, string projectPath)
    {
        clearEnvironment(startInfo);
        startInfo.Environment["LUDORK_SERVER_CONFIG_FILE"] = GetTestConfigPath(projectPath);
    }

    public static void ApplyPackEnvironment(ProcessStartInfo startInfo, LudorkServerSettings settings)
    {
        clearEnvironment(startInfo);
        startInfo.Environment["LUDORK_SERVER_ENABLED"] = settings.Enabled ? "1" : "0";
        if (settings.Enabled)
        {
            startInfo.Environment["LUDORK_SERVER_URL"] = settings.Url;
            startInfo.Environment["LUDORK_SERVER_KEY"] = settings.Key;
        }
    }

    private static void clearEnvironment(ProcessStartInfo startInfo)
    {
        startInfo.Environment.Remove("LUDORK_SERVER_CONFIG_FILE");
        startInfo.Environment.Remove("LUDORK_SERVER_ENABLED");
        startInfo.Environment.Remove("LUDORK_SERVER_URL");
        startInfo.Environment.Remove("LUDORK_SERVER_KEY");
    }
}
