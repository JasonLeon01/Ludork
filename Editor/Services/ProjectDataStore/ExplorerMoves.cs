using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    internal async Task<Exception?> ApplyExplorerMovesAsync(IReadOnlyList<(string OldPath, string NewPath)> assets,
        IReadOnlyList<(string OldPath, string NewPath)> resources, IReadOnlyList<(string OldPath, string NewPath)> files,
        IProgress<EditorOperationProgress>? progress)
    {
        if (assets.Count == 0 && resources.Count == 0)
            return await ApplyExternalFileChangesAsync([], files, [], progress);
        if (files.Count != 0)
            return await applyExternalFileChangesAsync([], files, [], store => store.MoveExplorerResources(assets, resources), progress);
        Exception? notification = null;
        await runResourceOperationAsync(store => store.MoveExplorerResources(assets, resources),
            moved => moved, progress, "EDIT_OPERATION_MOVING", error => notification = error);
        return notification;
    }

    internal bool MoveExplorerResources(IReadOnlyList<(string OldPath, string NewPath)> assets,
        IReadOnlyList<(string OldPath, string NewPath)> resources)
    {
        if (assets.Count != 0)
            AssetPaths.Move(assets);
        List<string> errors = [];
        foreach ((string oldPath, string newPath) in resources)
            if (!TryRenameManagedPath(oldPath, newPath, out string? error) || error is not null)
                errors.Add($"{oldPath}: {error ?? LocaleService.Get("MOVE_FILE_FAILED")}");
        if (errors.Count != 0)
            throw new IOException(string.Join(Environment.NewLine, errors));
        return true;
    }
}
