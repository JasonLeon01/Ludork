using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;
using Ludork.Services.BlueprintAssistant;

namespace Ludork.Services;

internal sealed record StoredAppleSigning<TRecord>(
    TRecord Record,
    string CertificatePassword,
    string NotaryPassword)
    where TRecord : class;

internal sealed record MacOSStoredSigning(
    string SigningIdentity,
    string NotaryMode,
    string NotaryAppleId,
    string NotaryTeamId,
    string NotaryKeyPath,
    string NotaryKeyId,
    string NotaryKeyIssuer);

internal sealed record IOSStoredSigning(
    string TeamId,
    string ProvisioningProfilePath,
    string SigningIdentity);

internal sealed class AppleSigningCredentialStore<TRecord>
    where TRecord : class
{
    private const int SchemaVersion = 1;
    private static readonly JsonSerializerOptions serializerOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = false,
        WriteIndented = true,
    };
    private readonly string indexPath;
    private readonly IPluginSecretStore secretStore;

    public AppleSigningCredentialStore(string fileName, string secretScope) : this(
        Path.Combine(EditorPaths.IniDirectory, fileName),
        new PluginSecretStore(secretScope))
    {
    }

    internal AppleSigningCredentialStore(
        string indexPath,
        IPluginSecretStore secretStore)
    {
        this.indexPath = Path.GetFullPath(indexPath);
        this.secretStore = secretStore;
    }

    public static AppleSigningCredentialStore<MacOSStoredSigning> CreateMacOS() =>
        new("macos-signing.json", "editor/macos-signing");

    public static AppleSigningCredentialStore<IOSStoredSigning> CreateIOS() =>
        new("ios-signing.json", "editor/ios-signing");

    public async Task<StoredAppleSigning<TRecord>?> FindAsync(
        string certificatePath,
        CancellationToken cancellationToken)
    {
        string fullPath = normalizePath(certificatePath);
        CredentialIndexDocument document = await loadIndexAsync(cancellationToken);
        CredentialIndexEntry? entry = findEntry(document, fullPath);
        if (entry is null)
            return null;

        string? secretJson = await secretStore.ReadAsync(
            entry.SecretName,
            cancellationToken);
        if (secretJson is null)
            return null;
        StoredSecrets? secrets = JsonSerializer.Deserialize<StoredSecrets>(
            secretJson,
            serializerOptions);
        if (secrets is null
            || hasLineBreak(secrets.CertificatePassword)
            || hasLineBreak(secrets.NotaryPassword))
        {
            throw new InvalidDataException(
                $"Invalid Apple signing credential for '{fullPath}'.");
        }
        return new StoredAppleSigning<TRecord>(
            entry.Record,
            secrets.CertificatePassword ?? string.Empty,
            secrets.NotaryPassword ?? string.Empty);
    }

    public async Task SaveAsync(
        string certificatePath,
        TRecord record,
        string certificatePassword,
        string notaryPassword,
        CancellationToken cancellationToken)
    {
        if (record is null)
            throw new InvalidDataException("Apple signing information is missing.");
        if (hasLineBreak(certificatePassword) || hasLineBreak(notaryPassword))
            throw new InvalidDataException("Invalid Apple signing information.");
        string fullPath = normalizePath(certificatePath);
        CredentialIndexDocument document = await loadIndexAsync(cancellationToken);
        string? existingPath = findPath(document, fullPath);
        CredentialIndexEntry? existingEntry = existingPath is null
            ? null
            : document.Entries[existingPath];
        string secretName = existingEntry?.SecretName ?? createSecretName(fullPath);
        StoredSecrets secrets = new(certificatePassword, notaryPassword);
        await secretStore.WriteAsync(
            secretName,
            JsonSerializer.Serialize(secrets, serializerOptions),
            cancellationToken);

        if (existingPath is not null && !string.Equals(existingPath, fullPath, StringComparison.Ordinal))
            document.Entries.Remove(existingPath);
        document.Entries[fullPath] = new CredentialIndexEntry(
            record,
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
            throw new InvalidDataException("Apple signing credential index is empty.");
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
            throw new InvalidDataException("Apple signing credential index has no parent directory.");
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
                $"Unsupported Apple signing credential schema: {document.SchemaVersion}");
        }
        if (document.Entries is null)
            throw new InvalidDataException("Apple signing credential entries are missing.");
        HashSet<string> paths = new(pathComparer());
        HashSet<string> secretNames = new(StringComparer.Ordinal);
        foreach ((string path, CredentialIndexEntry entry) in document.Entries)
        {
            string fullPath = normalizePath(path);
            if (!paths.Add(fullPath))
                throw new InvalidDataException($"Duplicate Apple signing path: '{path}'.");
            if (entry is null)
                throw new InvalidDataException($"Missing Apple signing entry for '{path}'.");
            if (string.IsNullOrWhiteSpace(entry.SecretName) || hasLineBreak(entry.SecretName))
                throw new InvalidDataException($"Invalid Apple signing secret name for '{path}'.");
            if (entry.Record is null)
                throw new InvalidDataException($"Invalid Apple signing record for '{path}'.");
            if (!secretNames.Add(entry.SecretName))
                throw new InvalidDataException($"Duplicate Apple signing secret: '{entry.SecretName}'.");
        }
    }

    private static StringComparer pathComparer() =>
        OperatingSystem.IsWindows() || OperatingSystem.IsMacOS()
            ? StringComparer.OrdinalIgnoreCase
            : StringComparer.Ordinal;

    private static string normalizePath(string path)
    {
        if (string.IsNullOrWhiteSpace(path) || !Path.IsPathFullyQualified(path))
            throw new InvalidDataException("The Apple signing certificate path must be absolute.");
        return Path.GetFullPath(path).Normalize(NormalizationForm.FormC);
    }

    private static bool pathsEqual(string left, string right) =>
        pathComparer().Equals(normalizePath(left), normalizePath(right));

    private static string createSecretName(string fullPath)
    {
        string identity = OperatingSystem.IsWindows() || OperatingSystem.IsMacOS()
            ? fullPath.ToUpperInvariant()
            : fullPath;
        byte[] hash = SHA256.HashData(Encoding.UTF8.GetBytes(identity));
        return "certificate-" + Convert.ToHexString(hash).ToLowerInvariant();
    }

    private static bool hasLineBreak(string? value)
    {
        return value is not null && value.IndexOfAny(['\r', '\n']) >= 0;
    }

    private sealed class CredentialIndexDocument
    {
        public int SchemaVersion { get; set; } = AppleSigningCredentialStore<TRecord>.SchemaVersion;
        public Dictionary<string, CredentialIndexEntry> Entries { get; set; } = new();
    }

    private sealed record CredentialIndexEntry(
        TRecord Record,
        string SecretName);

    private sealed record StoredSecrets(
        string? CertificatePassword,
        string? NotaryPassword);
}
