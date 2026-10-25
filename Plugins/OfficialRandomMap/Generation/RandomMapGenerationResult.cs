using System.Collections.Generic;

namespace Ludork.Plugins.OfficialRandomMap.Generation;

public sealed record RandomMapGenerationResult(
    bool Success,
    string Error,
    IReadOnlyList<IReadOnlyList<int?>> Tiles,
    int Seed);
