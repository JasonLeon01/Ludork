using Ludork.Models;
using System;
using System.Collections.Generic;

namespace Ludork.Services;

internal sealed record PreparedAssetDiskChanges(
    IReadOnlyList<PreparedAssetPathChange> Changes,
    IReadOnlyList<ReferenceRewrite> Rewrites,
    IReadOnlyList<string> DeletedPaths,
    ProjectAssetDiskInputs Inputs);

internal sealed record PreparedAssetPathChange(Guid Id, EditorDocumentState? Current,
    EditorDocumentState? Saved, bool ClearHistory);
