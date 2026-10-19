namespace Ludork.Services;

public sealed record ExternalFileChange(string Path, string? PreviousFingerprint, string? Fingerprint, byte[]? Content);
