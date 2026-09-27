using Ludork.Plugin.Abstractions;
using Ludork.Services.BlueprintAssistant;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal sealed class AndroidSigningCredentialStore
{
    private readonly SigningCredentialStore<CredentialIndexEntry, StoredPasswords> store;

    public AndroidSigningCredentialStore() : this(
        Path.Combine(EditorPaths.IniDirectory, "android-signing.json"),
        new PluginSecretStore("editor/android-signing"))
    {
    }

    internal AndroidSigningCredentialStore(string indexPath, IPluginSecretStore secretStore)
    {
        store = new(
            indexPath,
            secretStore,
            "Android",
            "keystore-",
            entry => entry.SecretName,
            entry => !string.IsNullOrWhiteSpace(entry.Alias)
                && !SigningInput.HasInvalidCharacters(entry.Alias),
            passwords => SigningInput.IsNonEmptySingleLine(passwords.KeystorePassword)
                && SigningInput.IsNonEmptySingleLine(passwords.KeyPassword));
    }

    public async Task<AndroidSigningOptions?> FindAsync(
        string keystorePath,
        CancellationToken cancellationToken)
    {
        (string Path, CredentialIndexEntry Entry, StoredPasswords Secrets)? stored =
            await store.FindAsync(keystorePath, cancellationToken);
        return stored is { } credential
            ? new AndroidSigningOptions(
                credential.Path,
                credential.Entry.Alias,
                credential.Secrets.KeystorePassword,
                credential.Secrets.KeyPassword)
            : null;
    }

    public Task SaveAsync(AndroidSigningOptions signing, CancellationToken cancellationToken) =>
        store.SaveAsync(
            signing.KeystorePath,
            new StoredPasswords(signing.KeystorePassword, signing.KeyPassword),
            secretName => new CredentialIndexEntry(signing.KeyAlias, secretName),
            cancellationToken);

    private sealed record CredentialIndexEntry(string Alias, string SecretName);

    private sealed record StoredPasswords(string KeystorePassword, string KeyPassword);
}
