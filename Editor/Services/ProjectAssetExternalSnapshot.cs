using System.Collections.Generic;

namespace Ludork.Services;

internal sealed record ProjectAssetExternalSnapshot(IReadOnlyList<ExternalFileChange> Changes,
    ProjectAssetDiskInputs Inputs);
