using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugin.Abstractions;

public interface IResourceCleanupHost
{
    string ProjectPath { get; }
    Task<IReadOnlyList<string>> ReadKeepPathsAsync(CancellationToken cancellationToken);
    Task SaveKeepPathsAsync(IReadOnlyList<string> paths, CancellationToken cancellationToken);
    Task<ResourceCleanupReport> ScanAsync(
        IReadOnlyList<string> nativeKeepPaths,
        IReadOnlyList<string> userKeepPaths,
        IProgress<ResourceCleanupProgress>? progress,
        CancellationToken cancellationToken);
    Task<ResourceCleanupTrashResult> TrashAsync(
        string reportId,
        IProgress<ResourceCleanupProgress>? progress,
        CancellationToken cancellationToken);
}

public sealed record ResourceCleanupCandidate(string RelativePath, string Category, long SizeBytes);

public sealed record ResourceCleanupIssue(string Path, string Message);

public sealed record ResourceCleanupProgress(string Stage, int Completed, int Total, string CurrentPath);

public sealed record ResourceCleanupReport(
    string Id,
    IReadOnlyList<ResourceCleanupCandidate> Candidates,
    IReadOnlyList<ResourceCleanupIssue> Issues)
{
    public bool CanTrash => Issues.Count == 0 && Candidates.Count != 0;
    public long TotalBytes => Candidates.Sum(candidate => candidate.SizeBytes);
}

public sealed record ResourceCleanupTrashResult(
    IReadOnlyList<string> RecycledPaths,
    IReadOnlyList<string> RemainingPaths,
    string Error,
    bool Cancelled);
