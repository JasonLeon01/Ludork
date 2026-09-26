namespace Ludork.Services;

public sealed record HarmonySigningOptions(
    string KeystorePath,
    string CertificatePath,
    string ProfilePath,
    string KeyAlias,
    string KeystorePassword,
    string KeyPassword)
{
    public override string ToString() =>
        nameof(HarmonySigningOptions) + " { Redacted }";
}
