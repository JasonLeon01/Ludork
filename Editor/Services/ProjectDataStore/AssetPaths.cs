using Ludork.Models;
using System;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    private ProjectAssetPaths? assetPaths;
    public ProjectAssetPaths AssetPaths => assetPaths ??= new ProjectAssetPaths(this);

    internal Task<bool> RunAssetPathOperationAsync(Func<ProjectDataStore, bool> operation,
        IProgress<EditorOperationProgress>? progress)
        => runResourceOperationAsync(operation, result => result, progress, "EDIT_OPERATION_MOVING");
}
