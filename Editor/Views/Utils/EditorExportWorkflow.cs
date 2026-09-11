using Avalonia.Controls;
using Ludork.Services;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public static class EditorExportWorkflow
{
    public static async Task<ProjectExportResult> ExportAsync(
        Window owner,
        ProjectSaveService saveService,
        ProjectExportService exportService,
        Action<string> writeOutput,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (!await EditorSaveWorkflow.TrySaveAsync(owner, saveService, false))
            return ProjectExportResult.CancelledResult();
        cancellationToken.ThrowIfCancellationRequested();
        return await exportService.ExportAsync(writeOutput, cancellationToken);
    }
}
