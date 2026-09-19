using Ludork.Plugin.Abstractions;
using System.Collections.Generic;

namespace Ludork.Services;

internal sealed record ResourceCleanupFileFingerprint(long Size, long LastWriteTicks, string Sha256);

internal sealed record ResourceCleanupSnapshot(
    string ProjectPath,
    ResourceCleanupReport Report,
    IReadOnlyDictionary<string, ResourceCleanupFileFingerprint> Fingerprints,
    IReadOnlyList<string> DeletionOrder);
