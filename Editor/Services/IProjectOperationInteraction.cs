using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public interface IProjectOperationInteraction
{
    bool IsClosing { get; }
    bool IndividualWindow { get; }
    bool LiveDebugRequested { get; }
    void PrepareRun();
    void BeginOutput();
    void EndOutput();
    void RestoreViewport();
    Task<bool> SaveAsync(bool needsBuild);
    Task<bool> ConfirmRebuildAsync(CancellationToken cancellationToken);
    Task<bool> ConfirmReexportAsync(CancellationToken cancellationToken);
    Task<ProjectRunResult> ExportAsync(CancellationToken cancellationToken);
    Task<nint> PrepareViewportAsync(ProjectWindowMode mode);
    Task ShowFailureAsync(ProjectRunResult result, bool building);
}

public enum EditorProjectOperationKind
{
    Construct,
    Export,
    Play,
}
