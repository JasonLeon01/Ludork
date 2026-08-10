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
        if (!File.Exists(environment.RegistryPath))
        {
            return new PluginRegistryState(
                false,
                true,
                new PluginRegistryDocument(),
                string.Empty);
        }

        try
        {
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
        validate(document);
        Directory.CreateDirectory(environment.RootDirectory);
        string temporaryPath = Path.Combine(
            environment.RootDirectory,
            $".plugins.json.{Guid.NewGuid():N}.tmp");
        string json = JsonSerializer.Serialize(document, SerializerOptions) + Environment.NewLine;
        try
        {
            await File.WriteAllTextAsync(
                temporaryPath,
                json,
                new UTF8Encoding(false),
                cancellationToken);
            File.Move(temporaryPath, environment.RegistryPath, true);
        }
        finally
        {
            if (File.Exists(temporaryPath))
                File.Delete(temporaryPath);
        }
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
