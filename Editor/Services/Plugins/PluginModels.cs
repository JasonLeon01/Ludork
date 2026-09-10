using Ludork.Plugin.Abstractions;
using System.IO;

namespace Ludork.Services.Plugins;

public enum PluginRuntimeStatus
{
    Loaded,
    ManifestInvalid,
    CompileFailed,
    InitializationFailed,
    PendingRestart,
    UnregisteredPendingRestart,
    PendingDeletion,
}

public sealed record PluginEnvironment(
    string RootDirectory,
    string PluginsDirectory,
    string RegistryPath)
{
    public string DataDirectory => Path.Combine(PluginsDirectory, ".data");
}

public sealed record PluginRuntimeInfo(
    string Id,
    string Name,
    string Version,
    string Directory,
    string SourcePath,
    PluginRuntimeStatus Status,
    string Diagnostic);

public sealed record RegisteredPluginMenuCommand(
    string PluginId,
    string PluginName,
    string PluginDirectory,
    string PluginDataDirectory,
    PluginMenuCommand Command);

public sealed record RegisteredPluginMapContextMenuCommand(
    string PluginId,
    string PluginName,
    string PluginDirectory,
    string PluginDataDirectory,
    PluginMapContextMenuCommand Command);

public sealed record RegisteredTextHintProvider(
    string PluginId,
    string PluginName,
    ITextHintProvider Provider);

public sealed record RegisteredProjectOperationHook(
    string PluginId,
    string PluginName,
    IProjectOperationHook Hook);

public sealed record PluginManagementItem(
    string Id,
    string Name,
    string Version,
    string Directory,
    string SourcePath,
    PluginRuntimeStatus Status,
    string Diagnostic,
    bool CanUninstall);

public sealed record PluginImportPreview(
    bool Success,
    string Id,
    string Name,
    string Version,
    string SourcePath,
    string Error);

public sealed record PluginManagementResult(
    bool Success,
    string Error,
    bool RestartRequired)
{
    public static PluginManagementResult Completed(bool restartRequired)
    {
        return new PluginManagementResult(true, string.Empty, restartRequired);
    }

    public static PluginManagementResult Failed(string error)
    {
        return new PluginManagementResult(false, error, false);
    }
}

internal sealed record PluginRegistryState(
    bool Exists,
    bool IsValid,
    PluginRegistryDocument Document,
    string Diagnostic);
