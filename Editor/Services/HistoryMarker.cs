using System;

namespace Ludork.Services;

public enum HistoryMarkerKind
{
    ReferenceUpdate,
}

public sealed record HistoryMarker(
    string Reason,
    Guid? SourceDocumentId = null,
    string? SourcePath = null,
    bool IsBarrier = false,
    HistoryMarkerKind Kind = HistoryMarkerKind.ReferenceUpdate);
