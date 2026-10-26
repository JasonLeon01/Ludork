using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Services;

internal sealed class MapHistoryEdits
{
    internal static MapHistoryEdits Empty { get; } = new([], []);

    internal MapHistoryEdits(IEnumerable<JsonDataEdit> forward, IEnumerable<JsonDataEdit> backward)
    {
        Forward = Array.AsReadOnly(forward.ToArray());
        Backward = Array.AsReadOnly(backward.ToArray());
    }

    internal IReadOnlyList<JsonDataEdit> Forward { get; }
    internal IReadOnlyList<JsonDataEdit> Backward { get; }

    internal MapHistoryEdits Append(MapHistoryEdits next)
    {
        if (next.Forward.Count == 0 && next.Backward.Count == 0)
            return this;
        if (Forward.Count == 0 && Backward.Count == 0)
            return next;
        return new MapHistoryEdits(Forward.Concat(next.Forward), next.Backward.Concat(Backward));
    }
}
