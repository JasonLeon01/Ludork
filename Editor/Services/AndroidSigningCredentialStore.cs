using Ludork.Plugin.Abstractions;
using Ludork.Services.BlueprintAssistant;
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

internal sealed class AndroidSigningCredentialStore
{
    private const int SchemaVersion = 1;
    private const string SecretScope = "editor/android-signing";
    private static readonly JsonSerializerOptions serializerOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = false,
        WriteIndented = true,
    };
    private readonly string indexPath;
    private readonly IPluginSecretStore secretStore;

    public AndroidSigningCredentialStore() : this(
        Path.Combine(EditorPaths.IniDirectory, "android-signing.json"),
        new PluginSecretStore(SecretScope))
    {
    }

    internal AndroidSigningCredentialStore(
        string indexPath,
        IPluginSecretStore secretStore)
    {
        this.indexPath = Path.GetFullPath(indexPath);
        this.secretStore = secretStore;
    }

    public async Task<AndroidSigningOptions?> FindAsync(
        string keystorePath,
        CancellationToken cancellationToken)
    {
        string fullPath = normalizePath(keystorePath);
        CredentialIndexDocument document = await loadIndexAsync(cancellationToken);
        CredentialIndexEntry? entry = findEntry(document, fullPath);
        if (entry is null)
            return null;

        string? secretJson = await secretStore.ReadAsync(
            entry.SecretName,
            cancellationToken);
        if (secretJson is null)
            return null;
        StoredPasswords? passwords = JsonSerializer.Deserialize<StoredPasswords>(
            secretJson,
            serializerOptions);
        if (passwords is null
            || string.IsNullOrEmpty(passwords.KeystorePassword)
            || string.IsNullOrEmpty(passwords.KeyPassword)
            || hasLineBreak(passwords.KeystorePassword)
            || hasLineBreak(passwords.KeyPassword))
        {
            throw new InvalidDataException(
                $"Invalid Android signing credential for '{fullPath}'.");
        }
        return new AndroidSigningOptions(
            fullPath,
            entry.Alias,
            passwords.KeystorePassword,
            passwords.KeyPassword);
    }

    public async Task SaveAsync(
        AndroidSigningOptions signing,
        CancellationToken cancellationToken)
    {
        string fullPath = normalizePath(signing.KeystorePath);
        validateSigning(signing);
        CredentialIndexDocument document = await loadIndexAsync(cancellationToken);
        string? existingPath = findPath(document, fullPath);
        CredentialIndexEntry? existingEntry = existingPath is null
            ? null
            : document.Entries[existingPath];
        string secretName = existingEntry?.SecretName ?? createSecretName(fullPath);
        StoredPasswords passwords = new(
            signing.KeystorePassword,
            signing.KeyPassword);
        string secretJson = JsonSerializer.Serialize(passwords, serializerOptions);
        await secretStore.WriteAsync(secretName, secretJson, cancellationToken);

        if (existingPath is not null && !string.Equals(existingPath, fullPath, StringComparison.Ordinal))
            document.Entries.Remove(existingPath);
        document.Entries[fullPath] = new CredentialIndexEntry(
            signing.KeyAlias,
            secretName);
        await saveIndexAsync(document, cancellationToken);
    }

    private async Task<CredentialIndexDocument> loadIndexAsync(
        CancellationToken cancellationToken)
    {
        if (!File.Exists(indexPath))
            return new CredentialIndexDocument();
        string json = await File.ReadAllTextAsync(
            indexPath,
            Encoding.UTF8,
            cancellationToken);
        CredentialIndexDocument? document =
            JsonSerializer.Deserialize<CredentialIndexDocument>(json, serializerOptions);
        if (document is null)
            throw new InvalidDataException("Android signing credential index is empty.");
        validateDocument(document);
        return document;
    }

    private async Task saveIndexAsync(
        CredentialIndexDocument document,
        CancellationToken cancellationToken)
    {
        validateDocument(document);
        string? directory = Path.GetDirectoryName(indexPath);
        if (directory is null)
            throw new InvalidDataException("Android signing credential index has no parent directory.");
        Directory.CreateDirectory(directory);
        string temporaryPath = Path.Combine(
            directory,
            $".{Path.GetFileName(indexPath)}.{Guid.NewGuid():N}.tmp");
        string json = JsonSerializer.Serialize(document, serializerOptions)
            + Environment.NewLine;
        try
        {
            await File.WriteAllTextAsync(
                temporaryPath,
                json,
                new UTF8Encoding(false),
                cancellationToken);
            File.Move(temporaryPath, indexPath, true);
        }
        finally
        {
            if (File.Exists(temporaryPath))
                File.Delete(temporaryPath);
        }
    }

    private static CredentialIndexEntry? findEntry(
        CredentialIndexDocument document,
        string path)
    {
        string? storedPath = findPath(document, path);
        return storedPath is null ? null : document.Entries[storedPath];
    }

    private static string? findPath(
        CredentialIndexDocument document,
        string path)
    {
        return document.Entries.Keys.FirstOrDefault(
            storedPath => pathsEqual(storedPath, path));
    }

    private static void validateDocument(CredentialIndexDocument document)
    {
        if (document.SchemaVersion != SchemaVersion)
        {
            throw new InvalidDataException(
                $"Unsupported Android signing credential schema: {document.SchemaVersion}");
        }
        if (document.Entries is null)
            throw new InvalidDataException("Android signing credential entries are missing.");
        StringComparer pathComparer = OperatingSystem.IsWindows() || OperatingSystem.IsMacOS()
            ? StringComparer.OrdinalIgnoreCase
            : StringComparer.Ordinal;
        HashSet<string> paths = new(pathComparer);
        HashSet<string> secretNames = new(StringComparer.Ordinal);
        foreach ((string path, CredentialIndexEntry entry) in document.Entries)
        {
            string fullPath = normalizePath(path);
            if (!paths.Add(fullPath))
                throw new InvalidDataException($"Duplicate Android signing path: '{path}'.");
            if (entry is null)
                throw new InvalidDataException($"Missing Android signing entry for '{path}'.");
            if (string.IsNullOrWhiteSpace(entry.Alias) || hasLineBreak(entry.Alias))
                throw new InvalidDataException($"Invalid Android signing alias for '{path}'.");
            if (string.IsNullOrWhiteSpace(entry.SecretName) || hasLineBreak(entry.SecretName))
                throw new InvalidDataException($"Invalid Android signing secret name for '{path}'.");
            if (!secretNames.Add(entry.SecretName))
                throw new InvalidDataException($"Duplicate Android signing secret: '{entry.SecretName}'.");
        }
    }

    private static void validateSigning(AndroidSigningOptions signing)
    {
        if (string.IsNullOrWhiteSpace(signing.KeyAlias)
            || hasLineBreak(signing.KeyAlias)
            || string.IsNullOrEmpty(signing.KeystorePassword)
            || string.IsNullOrEmpty(signing.KeyPassword)
            || hasLineBreak(signing.KeystorePassword)
            || hasLineBreak(signing.KeyPassword))
        {
            throw new InvalidDataException("Invalid Android signing information.");
        }
    }

    private static string normalizePath(string path)
    {
        if (string.IsNullOrWhiteSpace(path) || !Path.IsPathFullyQualified(path))
            throw new InvalidDataException("Android signing keystore path must be absolute.");
        return Path.GetFullPath(path).Normalize(NormalizationForm.FormC);
    }

    private static bool pathsEqual(string left, string right)
    {
        StringComparison comparison = OperatingSystem.IsWindows() || OperatingSystem.IsMacOS()
            ? StringComparison.OrdinalIgnoreCase
            : StringComparison.Ordinal;
        return normalizePath(left).Equals(normalizePath(right), comparison);
    }

    private static string createSecretName(string fullPath)
    {
        string identity = OperatingSystem.IsWindows() || OperatingSystem.IsMacOS()
            ? fullPath.ToUpperInvariant()
            : fullPath;
        byte[] hash = SHA256.HashData(Encoding.UTF8.GetBytes(identity));
        return "keystore-" + Convert.ToHexString(hash).ToLowerInvariant();
    }

    private static bool hasLineBreak(string value)
    {
        return value.IndexOfAny(['\r', '\n']) >= 0;
    }

    private sealed class CredentialIndexDocument
    {
        public CredentialIndexDocument()
        {
        }

        public int SchemaVersion { get; set; } = AndroidSigningCredentialStore.SchemaVersion;
        public Dictionary<string, CredentialIndexEntry> Entries { get; set; } = new();
    }

    private sealed record CredentialIndexEntry(
        string Alias,
        string SecretName);

    private sealed record StoredPasswords(
        string KeystorePassword,
        string KeyPassword);
}
