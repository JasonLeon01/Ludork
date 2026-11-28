using Avalonia.Threading;
using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Frozen;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.Plugins;

internal sealed class PluginBlueprintWorkspace(ProjectDataStore gameData, Action prepareProject,
    Action<IReadOnlyList<string>> notifyCreated) : IPluginBlueprintWorkspace
{
    public string ProjectPath => gameData.ProjectPath;

    public async Task<IPluginBlueprintSnapshot> CaptureAsync(CancellationToken cancellationToken)
    {
        return await Dispatcher.UIThread.InvokeAsync(() =>
        {
            prepare(cancellationToken);
            EditorDocument[] documents = gameData.Documents.All.Where(document => document.Section == "Blueprints").ToArray();
            PluginBlueprintSource[] blueprints = documents.Where(document => document.Exists)
                .Select(document => new PluginBlueprintSource(document.Path, document.CaptureState().InternalData!)).ToArray();
            StringComparer comparer = FileSystemPathIdentity.GetProjectComparer(ProjectPath);
            FrozenSet<string> paths = documents.SelectMany(document => new[] { document.Path, document.SavedPath })
                .ToFrozenSet(comparer);
            return new PluginBlueprintSnapshot(this, blueprints, paths, comparer, gameData.GetProjectEnumCatalog(),
                gameData.Documents.Revision);
        });
    }

    internal async Task CreateAsync(PluginBlueprintSnapshot snapshot, IReadOnlyList<PluginBlueprintCreation> creations,
        CancellationToken cancellationToken)
    {
        await Dispatcher.UIThread.InvokeAsync(() =>
        {
            prepare(cancellationToken);
            if (gameData.Documents.Revision != snapshot.Revision || !gameData.EditOperations.TryBegin())
                throw new PluginWorkspaceUnavailableException();
        });
        try
        {
            PluginBlueprintCreationBatch.Output[] outputs = await Task.Run(
                () => PluginBlueprintCreationBatch.Prepare(snapshot, creations, cancellationToken), cancellationToken);
            if (outputs.Length == 0)
                return;
            SaveResult result = await Task.Run(() =>
            {
                EditorFileSaveBatch batch = new();
                foreach (PluginBlueprintCreationBatch.Output output in outputs)
                    batch.CreateFile(output.Path, output.Content);
                return batch.Execute(cancellationToken: cancellationToken, canWrite: () =>
                {
                    if (!Dispatcher.UIThread.Invoke(() => !gameData.IsDisposed && gameData.Documents.Revision == snapshot.Revision))
                        throw new PluginWorkspaceUnavailableException();
                    return EditorValidationBatch.RunFiles(outputs, output =>
                        EditorPathSandbox.TryResolve(ProjectPath, output.Path, out _, allowMissing: true), cancellationToken)
                        .All(valid => valid);
                });
            }, cancellationToken);
            if (!result.Success)
                throw new IOException(result.Details);
            await Dispatcher.UIThread.InvokeAsync(() =>
            {
                gameData.applyExternalChanges(outputs.ToDictionary(output => ("Blueprints", output.Key),
                    output => (JsonObject?)output.Data), false, null);
                notifyCreated(outputs.Select(output => output.Path).ToArray());
            });
        }
        finally
        {
            await Dispatcher.UIThread.InvokeAsync(gameData.EditOperations.Complete);
        }
    }

    private void prepare(CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (gameData.IsDisposed || gameData.EditOperations.IsBusy)
            throw new PluginWorkspaceUnavailableException();
        prepareProject();
        gameData.CompleteDocumentChanges();
    }
}
