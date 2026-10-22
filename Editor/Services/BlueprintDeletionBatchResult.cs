using System.Collections.Generic;

namespace Ludork.Services;

internal sealed record BlueprintDeletionBatchResult(
    IReadOnlyList<BlueprintDeletionBatchResult.Item> Items, string? Error)
{
    internal bool Aborted => Error is not null;

    internal sealed record Item(string Path, bool Deleted, string? Error);
}
