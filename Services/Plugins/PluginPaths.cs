using System;
using System.IO;

namespace Ludork.Services.Plugins;

public static class PluginPaths
{
    private const string DevelopmentMarkerFileName = ".ludork-development";

    public static PluginEnvironment Resolve()
    {
        return resolve(
            AppContext.BaseDirectory,
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile));
    }

    private static PluginEnvironment resolve(
        string baseDirectory,
        string userProfile)
    {
        string? developmentRoot = findDevelopmentRoot(baseDirectory);
        if (developmentRoot is not null)
            return createEnvironment(developmentRoot);

        if (OperatingSystem.IsWindows())
            return createEnvironment(EditorRuntimePaths.ContentRoot);

        string userRoot = Path.Combine(userProfile, "Ludork");
        return createEnvironment(userRoot);
    }

    public static string GetPluginDataDirectory(
        PluginEnvironment environment,
        string pluginId)
    {
        if (!PluginPackageInspector.IsValidIdentifier(pluginId))
            throw new ArgumentException($"Invalid plugin ID: {pluginId}", nameof(pluginId));
        return Path.Combine(environment.DataDirectory, pluginId);
    }

    private static string? findDevelopmentRoot(string startPath)
    {
        string baseDirectory = Path.TrimEndingDirectorySeparator(
            Path.GetFullPath(startPath));
        string markerPath = Path.Combine(
            baseDirectory,
            DevelopmentMarkerFileName);
        if (!File.Exists(markerPath))
            return null;
        string markerRoot = File.ReadAllText(markerPath).Trim();
        if (string.IsNullOrWhiteSpace(markerRoot)
            || !Path.IsPathFullyQualified(markerRoot))
        {
            return null;
        }
        string developmentRoot = Path.TrimEndingDirectorySeparator(
            Path.GetFullPath(markerRoot));
        return File.Exists(Path.Combine(developmentRoot, "Ludork.csproj"))
            ? developmentRoot
            : null;
    }

    private static PluginEnvironment createEnvironment(string rootDirectory)
    {
        string root = Path.GetFullPath(rootDirectory);
        return new PluginEnvironment(
            root,
            Path.Combine(root, "Plugins"),
            Path.Combine(root, "plugins.json"));
    }
}
