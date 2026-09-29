using System;
using System.IO;
using System.Text.Json;

namespace Ludork.Services;

public static class EditorVersionService
{
    private static readonly Lazy<(Version Version, string FullVersion)> current = new(readVersion);

    public static string FullVersion => current.Value.FullVersion;
    public static string DocumentationVersion => $"{current.Value.Version.Major}.{current.Value.Version.Minor}";

    private static (Version Version, string FullVersion) readVersion()
    {
        string? buildInfoPath = EditorRuntimePaths.FindFile("BuildInfo.json");
        if (buildInfoPath is not null)
        {
            using JsonDocument document = JsonDocument.Parse(File.ReadAllText(buildInfoPath));
            Version packagedVersion = Version.Parse(document.RootElement.GetProperty("version").GetString()!);
            string fullVersion = document.RootElement.GetProperty("fullVersion").GetString()!;
            return (packagedVersion, fullVersion);
        }
        Version assemblyVersion = typeof(EditorVersionService).Assembly.GetName().Version!;
        return (assemblyVersion, assemblyVersion.ToString(3));
    }
}
