using Ludork.Models;
using System;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class WorldDataService
{
    public Task<bool> RenameWorldMapAsync(string currentKey, string newKey, IProgress<EditorOperationProgress>? progress = null)
        => isValidMapChildName(newKey)
            ? store.RenameDocumentResourceAsync("WorldMaps", normalizeWorldKey(currentKey), newKey, progress)
            : Task.FromResult(false);
}
