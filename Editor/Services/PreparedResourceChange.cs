using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed record PreparedResourceChange(
    Guid Id,
    EditorDocumentState Before,
    EditorDocumentState After,
    EditorDocumentState Saved,
    JsonObject? Data,
    IReadOnlyList<DocumentHistoryEntry> Undo,
    IReadOnlyList<DocumentHistoryEntry> Redo,
    bool Modified,
    bool ContentChanged,
    MapCatalogEntry? Catalog,
    long MapBytes);
