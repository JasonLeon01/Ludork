using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace Ludork.Services.Plugins;

public static class PluginPaths
{
    private const string DevelopmentMarkerFileName = ".ludork-development";
    public const string ProjectPluginsDirectoryName = "__ProjectPlugins";

    public static string NormalizeProjectPath(string projectPath)
    {
        string fullPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(projectPath));
        return OperatingSystem.IsWindows() ? fullPath.ToUpperInvariant() : fullPath;
    }

    public static PluginEnvironment ForProject(PluginEnvironment global, string projectPath)
    {
        string normalized = NormalizeProjectPath(projectPath);
        string hash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(normalized)))
            [..16].ToLowerInvariant();
        string name = Path.GetFileName(normalized);
        StringBuilder safeName = new();
        foreach (char character in name)
        {
            if (safeName.Length == 48)
                break;
            safeName.Append(char.IsLetterOrDigit(character) || character is '-' or '_' ? character : '_');
        }
        string key = $"{(safeName.Length == 0 ? "Project" : safeName.ToString())}-{hash}";
        string root = Path.Combine(global.PluginsDirectory, ProjectPluginsDirectoryName, key);
        return new PluginEnvironment(global.RootDirectory, root, Path.Combine(root, "plugins.json"))
        {
            ProjectKey = key,
        };
    }

    public static string GetSecretStoreId(PluginEnvironment environment, string pluginId)
    {
        return environment.ProjectKey is null
            ? pluginId
            : $"ProjectPlugins/{environment.ProjectKey}/{pluginId}";
    }

    internal static void EnsureEnvironmentIsSafe(PluginEnvironment environment)
    {
        string root = environment.RootDirectory;
        if (!Directory.Exists(root))
            root = Path.GetDirectoryName(root) ?? root;
        if (!EditorPathSandbox.TryResolve(root, environment.PluginsDirectory, out _, allowMissing: true)
            || !EditorPathSandbox.TryResolve(root, environment.RegistryPath, out _, allowMissing: true))
        {
            throw new InvalidDataException("Plugin storage is outside its root, linked or inaccessible.");
        }
    }

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
        string path = Path.Combine(environment.DataDirectory, pluginId);
        EnsureEnvironmentIsSafe(environment);
        if (!EditorPathSandbox.TryResolve(environment.RootDirectory, path, out _, allowMissing: true))
            throw new InvalidDataException($"Plugin data path is linked or inaccessible: {path}");
        return path;
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
