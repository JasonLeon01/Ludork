using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Threading;

namespace Ludork.Services;

internal static class ReferenceDocumentBatchScanner
{
    internal static DocumentReferenceResult[] Scan(string projectPath,
        IReadOnlyList<ReferenceBuildDocument> documents,
        IReadOnlyDictionary<string, JsonObject> blueprints, ProjectEnumCatalog projectEnums,
        CancellationToken cancellationToken = default, Action<int>? completed = null)
    {
        LuaValidationReadCache reads = new(projectPath, projectEnums, cancellationToken);
        return EditorValidationBatch.Run<ReferenceBuildDocument, ReferenceDocumentScanWorker, DocumentReferenceResult>(
            documents, () => new ReferenceDocumentScanWorker(reads, blueprints, projectEnums, cancellationToken),
            static (worker, document) => worker.Scan(document), static worker => worker.Dispose(),
            cancellationToken, completed);
    }
}
