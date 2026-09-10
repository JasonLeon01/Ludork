using System.Collections.Generic;

namespace Ludork.Models;

public sealed record UiPreviewRuntimeSnapshot(
    string BuildId,
    string HostPath,
    string RegistryHash,
    string AdapterFingerprint,
    IReadOnlyList<UiControlDescriptor> Controls,
    IReadOnlyList<string> Files);
