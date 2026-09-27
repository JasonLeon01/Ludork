using Ludork.Plugin.Abstractions;
using Ludork.Services.BlueprintAssistant;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

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
    private readonly SigningCredentialStore<CredentialIndexEntry, StoredSecrets> store;

    public AppleSigningCredentialStore(string fileName, string secretScope) : this(
        Path.Combine(EditorPaths.IniDirectory, fileName),
        new PluginSecretStore(secretScope))
    {
    }

    internal AppleSigningCredentialStore(string indexPath, IPluginSecretStore secretStore)
    {
        store = new(
            indexPath,
            secretStore,
            "Apple",
            "certificate-",
            entry => entry.SecretName,
            entry => entry.Record is not null,
            secrets => !SigningInput.HasInvalidCharacters(secrets.CertificatePassword)
                && !SigningInput.HasInvalidCharacters(secrets.NotaryPassword));
    }

    public static AppleSigningCredentialStore<MacOSStoredSigning> CreateMacOS() =>
        new("macos-signing.json", "editor/macos-signing");

    public static AppleSigningCredentialStore<IOSStoredSigning> CreateIOS() =>
        new("ios-signing.json", "editor/ios-signing");

    public async Task<StoredAppleSigning<TRecord>?> FindAsync(
        string certificatePath,
        CancellationToken cancellationToken)
    {
        (string Path, CredentialIndexEntry Entry, StoredSecrets Secrets)? stored =
            await store.FindAsync(certificatePath, cancellationToken);
        return stored is { } credential
            ? new StoredAppleSigning<TRecord>(
                credential.Entry.Record,
                credential.Secrets.CertificatePassword ?? string.Empty,
                credential.Secrets.NotaryPassword ?? string.Empty)
            : null;
    }

    public Task SaveAsync(
        string certificatePath,
        TRecord record,
        string certificatePassword,
        string notaryPassword,
        CancellationToken cancellationToken) =>
        store.SaveAsync(
            certificatePath,
            new StoredSecrets(certificatePassword, notaryPassword),
            secretName => new CredentialIndexEntry(record, secretName),
            cancellationToken);

    private sealed record CredentialIndexEntry(TRecord Record, string SecretName);

    private sealed record StoredSecrets(string? CertificatePassword, string? NotaryPassword);
}
