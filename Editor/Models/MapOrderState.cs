using System.Collections.Generic;

namespace Ludork.Models;

public sealed record MapOrderState(
    IReadOnlyDictionary<string, IReadOnlyList<MapOrderNode>> Trees,
    IReadOnlyDictionary<string, string> Saved);
