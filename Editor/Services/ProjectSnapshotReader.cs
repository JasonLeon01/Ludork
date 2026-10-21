using Ludork.Models;
using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal static class ProjectSnapshotReader
{
    internal static async Task<T> ReadAsync<T>(ProjectDataStore gameData,
        Func<ProjectDataStore, T> read, IProgress<EditorOperationProgress>? progress = null,
        CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        ProjectOperationSnapshot snapshot = await gameData.CaptureOperationSnapshotAsync(progress);
        T result = await Task.Run(() =>
        {
            cancellationToken.ThrowIfCancellationRequested();
            ReferenceInputFiles files = ReferenceInputFiles.Capture(snapshot.ProjectPath,
                snapshot.Documents.SelectMany(document => new[] { document.Current.Path, document.Saved.Path }), includeData: true);
            using ProjectDataStore captured = snapshot.CreateStore();
            progress?.Report(new EditorOperationProgress("EDIT_OPERATION_VALIDATING"));
            T result = read(captured);
            cancellationToken.ThrowIfCancellationRequested();
            if (!files.IsCurrent())
                throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
            return result;
        }, cancellationToken);
        cancellationToken.ThrowIfCancellationRequested();
        if (gameData.IsDisposed || gameData.Documents.Revision != snapshot.Revision)
            throw new InvalidOperationException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        return result;
    }
}
