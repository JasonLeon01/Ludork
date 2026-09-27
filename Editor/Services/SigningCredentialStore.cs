using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal sealed class SigningCredentialStore<TEntry, TSecrets>
    where TEntry : class
    where TSecrets : class
{
    private const int SchemaVersion = 1;
    private static readonly JsonSerializerOptions serializerOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = false,
        WriteIndented = true,
    };
    private static readonly StringComparer pathComparer =
        OperatingSystem.IsWindows() || OperatingSystem.IsMacOS()
            ? StringComparer.OrdinalIgnoreCase
            : StringComparer.Ordinal;
    private readonly string indexPath;
    private readonly IPluginSecretStore secretStore;
    private readonly string platformName;
    private readonly string secretPrefix;
    private readonly Func<TEntry, string> getSecretName;
    private readonly Func<TEntry, bool> isValidEntry;
    private readonly Func<TSecrets, bool> isValidSecrets;

    public SigningCredentialStore(
        string indexPath,
        IPluginSecretStore secretStore,
        string platformName,
        string secretPrefix,
        Func<TEntry, string> getSecretName,
        Func<TEntry, bool> isValidEntry,
        Func<TSecrets, bool> isValidSecrets)
    {
        this.indexPath = Path.GetFullPath(indexPath);
        this.secretStore = secretStore;
        this.platformName = platformName;
        this.secretPrefix = secretPrefix;
        this.getSecretName = getSecretName;
        this.isValidEntry = isValidEntry;
        this.isValidSecrets = isValidSecrets;
    }

    public async Task<(string Path, TEntry Entry, TSecrets Secrets)?> FindAsync(
        string path,
        CancellationToken cancellationToken)
    {
        string fullPath = SigningInput.NormalizePath(path);
        CredentialIndexDocument document = await loadIndexAsync(cancellationToken);
        string? storedPath = findPath(document, fullPath);
        if (storedPath is null)
            return null;

        TEntry entry = document.Entries[storedPath];
        string? secretJson = await secretStore.ReadAsync(
            getSecretName(entry),
            cancellationToken);
        if (secretJson is null)
            return null;
        TSecrets? secrets = JsonSerializer.Deserialize<TSecrets>(secretJson, serializerOptions);
        if (secrets is null || !isValidSecrets(secrets))
            throw new InvalidDataException($"Invalid {platformName} signing credential for '{fullPath}'.");
        return (fullPath, entry, secrets);
    }

    public async Task SaveAsync(
        string path,
        TSecrets secrets,
        Func<string, TEntry> createEntry,
        CancellationToken cancellationToken)
    {
        string fullPath = SigningInput.NormalizePath(path);
        if (secrets is null || !isValidSecrets(secrets))
            throw new InvalidDataException($"Invalid {platformName} signing information.");
        CredentialIndexDocument document = await loadIndexAsync(cancellationToken);
        string? existingPath = findPath(document, fullPath);
        string secretName = existingPath is null
            ? createSecretName(fullPath)
            : getSecretName(document.Entries[existingPath]);
        if (existingPath is not null && !string.Equals(existingPath, fullPath, StringComparison.Ordinal))
            document.Entries.Remove(existingPath);
        document.Entries[fullPath] = createEntry(secretName);
        validateDocument(document);

        await secretStore.WriteAsync(
            secretName,
            JsonSerializer.Serialize(secrets, serializerOptions),
            cancellationToken);
        await saveIndexAsync(document, cancellationToken);
    }

    private async Task<CredentialIndexDocument> loadIndexAsync(CancellationToken cancellationToken)
    {
        if (!File.Exists(indexPath))
            return new CredentialIndexDocument();
        string json = await File.ReadAllTextAsync(indexPath, Encoding.UTF8, cancellationToken);
        CredentialIndexDocument? document =
            JsonSerializer.Deserialize<CredentialIndexDocument>(json, serializerOptions);
        if (document is null)
            throw new InvalidDataException($"{platformName} signing credential index is empty.");
        validateDocument(document);
        return document;
    }

    private async Task saveIndexAsync(
        CredentialIndexDocument document,
        CancellationToken cancellationToken)
    {
        string? directory = Path.GetDirectoryName(indexPath);
        if (directory is null)
            throw new InvalidDataException($"{platformName} signing credential index has no parent directory.");
        Directory.CreateDirectory(directory);
        string temporaryPath = Path.Combine(directory, $".{Path.GetFileName(indexPath)}.{Guid.NewGuid():N}.tmp");
        string json = JsonSerializer.Serialize(document, serializerOptions) + Environment.NewLine;
        try
        {
            await File.WriteAllTextAsync(temporaryPath, json, new UTF8Encoding(false), cancellationToken);
            File.Move(temporaryPath, indexPath, true);
        }
        finally
        {
            if (File.Exists(temporaryPath))
                File.Delete(temporaryPath);
        }
    }

    private static string? findPath(CredentialIndexDocument document, string path) =>
        document.Entries.Keys.FirstOrDefault(
            storedPath => pathComparer.Equals(SigningInput.NormalizePath(storedPath), path));

    private void validateDocument(CredentialIndexDocument document)
    {
        if (document.SchemaVersion != SchemaVersion)
        {
            throw new InvalidDataException(
                $"Unsupported {platformName} signing credential schema: {document.SchemaVersion}");
        }
        if (document.Entries is null)
            throw new InvalidDataException($"{platformName} signing credential entries are missing.");
        HashSet<string> paths = new(pathComparer);
        HashSet<string> secretNames = new(StringComparer.Ordinal);
        foreach ((string path, TEntry entry) in document.Entries)
        {
            string fullPath = SigningInput.NormalizePath(path);
            if (!paths.Add(fullPath))
                throw new InvalidDataException($"Duplicate {platformName} signing path: '{path}'.");
            if (entry is null || !isValidEntry(entry))
                throw new InvalidDataException($"Invalid {platformName} signing entry for '{path}'.");
            string secretName = getSecretName(entry);
            if (string.IsNullOrWhiteSpace(secretName) || SigningInput.HasInvalidCharacters(secretName))
                throw new InvalidDataException($"Invalid {platformName} signing secret name for '{path}'.");
            if (!secretNames.Add(secretName))
                throw new InvalidDataException($"Duplicate {platformName} signing secret: '{secretName}'.");
        }
    }

    private string createSecretName(string fullPath)
    {
        string identity = OperatingSystem.IsWindows() || OperatingSystem.IsMacOS()
            ? fullPath.ToUpperInvariant()
            : fullPath;
        byte[] hash = SHA256.HashData(Encoding.UTF8.GetBytes(identity));
        return secretPrefix + Convert.ToHexString(hash).ToLowerInvariant();
    }

    private sealed class CredentialIndexDocument
    {
        public int SchemaVersion { get; set; } = SigningCredentialStore<TEntry, TSecrets>.SchemaVersion;
        public Dictionary<string, TEntry> Entries { get; set; } = new();
    }
}
