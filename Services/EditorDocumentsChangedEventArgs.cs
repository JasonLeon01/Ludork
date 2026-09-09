using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Services;

public sealed class EditorDocumentsChangedEventArgs(IEnumerable<EditorDocumentChange> changes, bool reset = false) : EventArgs
{
    public bool Reset { get; } = reset;
    public IReadOnlyList<EditorDocumentChange> Changes { get; } = Array.AsReadOnly(changes.ToArray());

    public bool AffectsSection(string section) => Reset || Changes.Any(change => change.Section == section);
}

public sealed record EditorDocumentChange(Guid DocumentId, string Section, string? PreviousKey, string? Key,
    string? PreviousPath, string? Path, bool ContentChanged)
{
    public bool IdentityChanged => PreviousKey != Key || PreviousPath != Path;
}
