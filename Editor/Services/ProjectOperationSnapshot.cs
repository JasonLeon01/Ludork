using Ludork.Models;
using System;
using System.Collections.Generic;

namespace Ludork.Services;

internal sealed record ProjectOperationSnapshot(
    string ProjectPath,
    long Revision,
    IReadOnlyList<ProjectOperationDocument> Documents,
    IReadOnlyList<MapCatalogEntry> MapCatalog,
    IReadOnlyDictionary<string, string> WorldMoves,
    IReadOnlyList<string> InvalidPaths,
    IReadOnlyDictionary<string, string> InvalidErrors,
    IReadOnlyList<string> DeletedPaths,
    bool GenerateEnums)
{
    internal ProjectDataStore CreateStore() => ProjectDataStore.CreateOperationStore(this);
}

internal sealed record ProjectOperationDocument(
    Guid Id,
    long Revision,
    EditorDocumentState Current,
    EditorDocumentState Saved,
    IReadOnlyList<DocumentHistoryEntry> Undo,
    IReadOnlyList<DocumentHistoryEntry> Redo);
