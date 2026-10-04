using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.Plugins;

internal sealed class PluginRegistryStore
{
    private static readonly JsonSerializerOptions SerializerOptions = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = false,
    };

    private readonly PluginEnvironment environment;

    public PluginRegistryStore(PluginEnvironment environment)
    {
        this.environment = environment;
    }

    public async Task<PluginRegistryState> LoadAsync(CancellationToken cancellationToken)
    {
        try
        {
            PluginPaths.EnsureEnvironmentIsSafe(environment);
            if (!File.Exists(environment.RegistryPath))
                return new PluginRegistryState(false, true, new PluginRegistryDocument(), string.Empty);
            string json = await File.ReadAllTextAsync(
                environment.RegistryPath,
                Encoding.UTF8,
                cancellationToken);
            PluginRegistryDocument? document = JsonSerializer.Deserialize<PluginRegistryDocument>(
                json,
                SerializerOptions);
            if (document is null)
                throw new InvalidDataException("Plugin registry is empty.");
            validate(document);
            return new PluginRegistryState(true, true, document, string.Empty);
        }
        catch (Exception exception) when (
            exception is JsonException
            or InvalidDataException
            or IOException
            or UnauthorizedAccessException)
        {
            return new PluginRegistryState(
                true,
                false,
                new PluginRegistryDocument(),
                $"Failed to read plugin registry '{environment.RegistryPath}': {exception.Message}");
        }
    }

    public async Task SaveAsync(
        PluginRegistryDocument document,
        CancellationToken cancellationToken)
    {
        PluginPaths.EnsureEnvironmentIsSafe(environment);
        validate(document);
        string json = JsonSerializer.Serialize(document, SerializerOptions) + Environment.NewLine;
        await FilePersistence.WriteAllTextAtomicAsync(environment.RegistryPath, json, cancellationToken);
    }

    private static void validate(PluginRegistryDocument document)
    {
        if (document.SchemaVersion != 1)
            throw new InvalidDataException($"Unsupported plugin registry schema: {document.SchemaVersion}");
        if (document.Plugins is null)
            throw new InvalidDataException("Plugin registry plugins must be an array.");
        if (document.PendingDelete is null)
            throw new InvalidDataException("Plugin registry pendingDelete must be an array.");

        HashSet<string> ids = new(StringComparer.Ordinal);
        StringComparer directoryComparer = OperatingSystem.IsWindows()
            ? StringComparer.OrdinalIgnoreCase
            : StringComparer.Ordinal;
        HashSet<string> directories = new(directoryComparer);
        foreach (PluginRegistryEntry entry in document.Plugins)
        {
            if (string.IsNullOrWhiteSpace(entry.Id))
                throw new InvalidDataException("Plugin registry contains an empty plugin ID.");
            if (!PluginPackageInspector.IsSafeDirectoryName(entry.Directory))
                throw new InvalidDataException($"Invalid plugin directory: {entry.Directory}");
            if (!ids.Add(entry.Id))
                throw new InvalidDataException($"Duplicate plugin ID: {entry.Id}");
            if (!directories.Add(entry.Directory))
                throw new InvalidDataException($"Duplicate plugin directory: {entry.Directory}");
        }
        foreach (string directory in document.PendingDelete)
        {
            if (!PluginPackageInspector.IsSafeDirectoryName(directory))
                throw new InvalidDataException($"Invalid pendingDelete directory: {directory}");
            if (!directories.Add(directory))
                throw new InvalidDataException(
                    $"Plugin directory is both registered and pending deletion: {directory}");
        }
    }
}
