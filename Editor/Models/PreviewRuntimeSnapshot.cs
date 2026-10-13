using System.Collections.Generic;

namespace Ludork.Models;

public sealed record PreviewRuntimeSnapshot(
    string BuildId,
    string HostPath,
    string RegistryHash,
    string AdapterFingerprint,
    IReadOnlyList<UiControlDescriptor> Controls,
    IReadOnlyList<string> Files);
