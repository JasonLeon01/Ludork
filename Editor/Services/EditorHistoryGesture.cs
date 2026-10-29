using System;
using System.Collections.Generic;

namespace Ludork.Services;

internal sealed class EditorHistoryGesture(long id, Action? flushPending)
{
    internal long Id { get; } = id;
    internal Action? FlushPending { get; set; } = flushPending;
    internal EditorDocument? Document { get; set; }
    internal EditorDocumentState? Before { get; set; }
    internal string? Description { get; set; }
    internal bool HasChanges { get; set; }
    internal bool Completing { get; set; }
    internal bool HasMapEdits { get; set; } = true;
    internal List<MapHistoryEdits> MapBatches { get; } = [];

    internal MapHistoryEdits? FreezeMapEdits()
    {
        if (!HasMapEdits)
            return null;
        return MapHistoryEdits.Combine(MapBatches);
    }
}
