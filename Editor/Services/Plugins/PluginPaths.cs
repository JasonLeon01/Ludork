using System;
using System.IO;

namespace Ludork.Services.Plugins;

public static class PluginPaths
{
    public const string ProjectPluginsDirectoryName = "__ProjectPlugins";

    public static PluginEnvironment ForProject(PluginEnvironment global, string projectPath)
    {
        string key = EditorProjectIdentity.GetKey(projectPath);
        string root = Path.Combine(global.PluginsDirectory, ProjectPluginsDirectoryName, key);
        return new PluginEnvironment(global.RootDirectory, root, Path.Combine(root, "plugins.json"))
        {
            ProjectKey = key,
            WorkspaceEnvironment = global.WorkspaceEnvironment is null
                ? null
                : ForProject(global.WorkspaceEnvironment, projectPath),
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
        string root = EditorPaths.UserDataDirectory;
        string workspaceRoot = Environment.CurrentDirectory;
        string marker = Path.Combine(AppContext.BaseDirectory, ".ludork-development");
        if (!File.Exists(Path.Combine(workspaceRoot, "plugins.json")) && File.Exists(marker))
        {
            string developmentRoot = File.ReadAllText(marker).Trim();
            if (Path.IsPathFullyQualified(developmentRoot)
                && File.Exists(Path.Combine(developmentRoot, "Ludork.csproj")))
                workspaceRoot = developmentRoot;
        }
        PluginEnvironment? workspace = EditorProjectIdentity.NormalizePath(root)
            == EditorProjectIdentity.NormalizePath(workspaceRoot)
            ? null
            : new PluginEnvironment(workspaceRoot, Path.Combine(workspaceRoot, "Plugins"),
                Path.Combine(workspaceRoot, "plugins.json"));
        return new PluginEnvironment(root, Path.Combine(root, "Plugins"), Path.Combine(root, "plugins.json"))
        {
            WorkspaceEnvironment = workspace,
        };
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
}
