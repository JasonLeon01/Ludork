using Ludork.Models;
using System.Collections.Generic;

namespace Ludork.Services;

internal sealed record EditorDocumentLiveChange(bool ContentChanged,
    IReadOnlyList<JsonDataEdit>? Edits = null, object? Origin = null);
