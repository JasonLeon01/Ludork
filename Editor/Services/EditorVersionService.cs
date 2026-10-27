using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.Json;

namespace Ludork.Services;

public static class EditorVersionService
{
    private static readonly IReadOnlyDictionary<string, string?> metadata = typeof(EditorVersionService).Assembly
        .GetCustomAttributes<AssemblyMetadataAttribute>()
        .ToDictionary(attribute => attribute.Key, attribute => attribute.Value);

    public static string BaseVersion { get; } = metadata["Ludork.BaseVersion"]!;
    public static bool IsDevelopment { get; } = bool.Parse(metadata["Ludork.Development"]!);
    public static string FullVersion
    {
        get
        {
            if (!IsDevelopment)
                return BaseVersion;
            string path = Path.Combine(EditorRuntimePaths.ContentRoot, "BuildInfo.json");
            if (!File.Exists(path)
                && !EditorRuntimePaths.IsWindowsPackage
                && (!OperatingSystem.IsMacOS()
                    || File.Exists(Path.Combine(AppContext.BaseDirectory, ".ludork-development"))))
            {
                return BaseVersion;
            }
            return readBuildInfoFullVersion(path);
        }
    }
    public static string DocumentationVersion { get; } = string.Join('.', BaseVersion.Split('.').Take(2));
    public static string GameVersion => $"v{DocumentationVersion}";
    public static IReadOnlyList<string> SupportedGameVersions { get; } = Array.AsReadOnly(
        JsonSerializer.Deserialize<string[]>(metadata["Ludork.SupportedGameVersions"]!)!);

    public static void ValidateBuildInfo(string path)
    {
        readBuildInfoFullVersion(path);
    }

    private static string readBuildInfoFullVersion(string path)
    {
        using JsonDocument document = JsonDocument.Parse(File.ReadAllText(path));
        JsonElement root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Object
            || root.EnumerateObject().Count() != 4
            || !root.TryGetProperty("version", out JsonElement version)
            || version.ValueKind != JsonValueKind.String || version.GetString() != BaseVersion
            || !root.TryGetProperty("dev", out JsonElement development)
            || development.ValueKind is not (JsonValueKind.True or JsonValueKind.False)
            || development.GetBoolean() != IsDevelopment
            || !root.TryGetProperty("builtAt", out JsonElement builtAt)
            || builtAt.ValueKind != JsonValueKind.String
            || !DateTimeOffset.TryParseExact(builtAt.GetString(), "yyyy-MM-dd'T'HH:mm:sszzz",
                CultureInfo.InvariantCulture, DateTimeStyles.None, out DateTimeOffset buildTime)
            || buildTime.Offset != TimeSpan.FromHours(8)
            || buildTime.Year is < 2000 or > 2099
            || !root.TryGetProperty("fullVersion", out JsonElement fullVersion)
            || fullVersion.ValueKind != JsonValueKind.String
            || fullVersion.GetString() != (IsDevelopment
                ? $"{BaseVersion}.{buildTime.ToString("yyyyMMddHH", CultureInfo.InvariantCulture)}"
                : BaseVersion))
        {
            throw new InvalidOperationException("The package build information is invalid or does not match the editor assembly version and channel.");
        }
        return fullVersion.GetString()!;
    }
}
