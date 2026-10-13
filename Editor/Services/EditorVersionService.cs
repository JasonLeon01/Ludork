using System;
using System.Collections.Generic;
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
    public static string FullVersion { get; } = metadata["Ludork.FullVersion"]!;
    public static bool IsDevelopment { get; } = bool.Parse(metadata["Ludork.Development"]!);
    public static string DocumentationVersion { get; } = string.Join('.', BaseVersion.Split('.').Take(2));
    public static string GameVersion => $"v{DocumentationVersion}";
    public static IReadOnlyList<string> SupportedGameVersions { get; } = Array.AsReadOnly(
        JsonSerializer.Deserialize<string[]>(metadata["Ludork.SupportedGameVersions"]!)!);

    public static void ValidateBuildInfo(string path)
    {
        using JsonDocument document = JsonDocument.Parse(File.ReadAllText(path));
        JsonElement root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Object
            || !root.TryGetProperty("version", out JsonElement version)
            || version.ValueKind != JsonValueKind.String || version.GetString() != BaseVersion
            || !root.TryGetProperty("fullVersion", out JsonElement fullVersion)
            || fullVersion.ValueKind != JsonValueKind.String || fullVersion.GetString() != FullVersion
            || !root.TryGetProperty("dev", out JsonElement development)
            || development.ValueKind is not (JsonValueKind.True or JsonValueKind.False)
            || development.GetBoolean() != IsDevelopment)
        {
            throw new InvalidOperationException("The editor assembly version or channel does not match the package build information. Rebuild the editor for this package.");
        }
    }
}
