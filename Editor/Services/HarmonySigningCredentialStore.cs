using Ludork.Plugin.Abstractions;
using Ludork.Services.BlueprintAssistant;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal sealed class HarmonySigningCredentialStore
{
    private readonly SigningCredentialStore<CredentialIndexEntry, StoredPasswords> store;

    public HarmonySigningCredentialStore() : this(
        Path.Combine(EditorPaths.IniDirectory, "harmony-signing.json"),
        new PluginSecretStore("editor/harmony-signing"))
    {
    }

    internal HarmonySigningCredentialStore(string indexPath, IPluginSecretStore secretStore)
    {
        store = new(
            indexPath,
            secretStore,
            "Harmony",
            "keystore-",
            entry => entry.SecretName,
            isValidEntry,
            passwords => SigningInput.IsNonEmptySingleLine(passwords.KeystorePassword)
                && SigningInput.IsNonEmptySingleLine(passwords.KeyPassword));
    }

    public async Task<HarmonySigningOptions?> FindAsync(
        string keystorePath,
        CancellationToken cancellationToken)
    {
        (string Path, CredentialIndexEntry Entry, StoredPasswords Secrets)? stored =
            await store.FindAsync(keystorePath, cancellationToken);
        return stored is { } credential
            ? new HarmonySigningOptions(
                credential.Path,
                credential.Entry.CertificatePath,
                credential.Entry.ProfilePath,
                credential.Entry.Alias,
                credential.Secrets.KeystorePassword,
                credential.Secrets.KeyPassword)
            : null;
    }

    public Task SaveAsync(HarmonySigningOptions signing, CancellationToken cancellationToken)
    {
        string certificatePath = SigningInput.NormalizePath(signing.CertificatePath);
        string profilePath = SigningInput.NormalizePath(signing.ProfilePath);
        return store.SaveAsync(
            signing.KeystorePath,
            new StoredPasswords(signing.KeystorePassword, signing.KeyPassword),
            secretName => new CredentialIndexEntry(certificatePath, profilePath, signing.KeyAlias, secretName),
            cancellationToken);
    }

    private static bool isValidEntry(CredentialIndexEntry entry)
    {
        SigningInput.NormalizePath(entry.CertificatePath);
        SigningInput.NormalizePath(entry.ProfilePath);
        return !string.IsNullOrWhiteSpace(entry.Alias)
            && !SigningInput.HasInvalidCharacters(entry.Alias);
    }

    private sealed record CredentialIndexEntry(
        string CertificatePath,
        string ProfilePath,
        string Alias,
        string SecretName);

    private sealed record StoredPasswords(string KeystorePassword, string KeyPassword);
}
