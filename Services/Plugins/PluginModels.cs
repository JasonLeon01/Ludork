using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json.Serialization;

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

internal sealed class PluginManifest
{
    [JsonPropertyName("schemaVersion")]
    public int SchemaVersion { get; set; }

    [JsonPropertyName("id")]
    public string Id { get; set; } = string.Empty;

    [JsonPropertyName("name")]
    public string Name { get; set; } = string.Empty;

    [JsonPropertyName("version")]
    public string Version { get; set; } = string.Empty;

    [JsonPropertyName("minimumEditorVersion")]
    public string MinimumEditorVersion { get; set; } = string.Empty;

    [JsonPropertyName("entryType")]
    public string EntryType { get; set; } = string.Empty;
}

internal sealed class PluginRegistryDocument
{
    [JsonPropertyName("schemaVersion")]
    public int SchemaVersion { get; set; } = 1;

    [JsonPropertyName("plugins")]
    public List<PluginRegistryEntry> Plugins { get; set; } = [];

    [JsonPropertyName("pendingDelete")]
    public List<string> PendingDelete { get; set; } = [];

    public PluginRegistryDocument Clone()
    {
        PluginRegistryDocument document = new()
        {
            SchemaVersion = SchemaVersion,
        };
        foreach (PluginRegistryEntry entry in Plugins)
            document.Plugins.Add(new PluginRegistryEntry { Id = entry.Id, Directory = entry.Directory });
        document.PendingDelete.AddRange(PendingDelete);
        return document;
    }
}

internal sealed class PluginRegistryEntry
{
    [JsonPropertyName("id")]
    public string Id { get; set; } = string.Empty;

    [JsonPropertyName("directory")]
    public string Directory { get; set; } = string.Empty;
}

internal sealed record PluginRegistryState(
    bool Exists,
    bool IsValid,
    PluginRegistryDocument Document,
    string Diagnostic);
