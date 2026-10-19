using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class ProjectExternalChangeService : IDisposable
{
    private readonly ProjectDataStore gameData;
    private readonly ProjectSaveService save;
    private readonly object sync = new();
    private readonly Dictionary<string, string> baseline;
    private readonly CancellationTokenSource lifetime = new();
    private long baselineRevision;
    private bool disposed;

    public ProjectExternalChangeService(ProjectDataStore gameData, ProjectSaveService save)
    {
        this.gameData = gameData;
        this.save = save;
        baseline = new Dictionary<string, string>(gameData.InitialDiskSnapshot.Fingerprints, ProjectDiskSnapshot.PathComparer);
        EditorFileSaveBatch.FilesCommitted += acceptWrites;
    }

    public bool IsSynchronizing { get; private set; }
    public event EventHandler<ExternalFilesSynchronizedEventArgs>? Synchronized;

    public async Task<bool> SynchronizeAsync(
        Func<IReadOnlyList<ExternalFileChange>, Task<IReadOnlySet<string>?>> resolveConflicts,
        CancellationToken cancellationToken = default, IProgress<EditorOperationProgress>? progress = null)
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        if (IsSynchronizing)
            return false;
        using CancellationTokenSource request = CancellationTokenSource.CreateLinkedTokenSource(lifetime.Token, cancellationToken);
        CancellationToken token = request.Token;
        IsSynchronizing = true;
        try
        {
            save.FlushPendingChanges();
            for (int attempt = 0; attempt < 3; attempt++)
            {
                token.ThrowIfCancellationRequested();
                long documentRevision = gameData.Documents.Revision;
                Dictionary<string, string> previous;
                long revision;
                lock (sync)
                {
                    previous = new(baseline, ProjectDiskSnapshot.PathComparer);
                    revision = baselineRevision;
                }
                ProjectDiskSnapshot current = await Task.Run(() => ProjectDiskSnapshot.Capture(gameData.ProjectPath, token,
                    progress: progress), token);
                token.ThrowIfCancellationRequested();
                lock (sync)
                    if (revision != baselineRevision)
                        continue;
                ExternalFileChange[] changes = await Task.Run(() => previous.Keys.Concat(current.Fingerprints.Keys)
                    .Distinct(ProjectDiskSnapshot.PathComparer).OrderBy(path => path, ProjectDiskSnapshot.PathComparer)
                    .Where(path => previous.GetValueOrDefault(path) != current.Fingerprints.GetValueOrDefault(path))
                    .Select(path => new ExternalFileChange(path, previous.GetValueOrDefault(path),
                        current.Fingerprints.GetValueOrDefault(path), current.Data.GetValueOrDefault(path))).ToArray(), token);
                if (changes.Length == 0)
                    return true;
                HashSet<string> pending = save.PendingInputPaths.ToHashSet(ProjectDiskSnapshot.PathComparer);
                EditorDocument[] dirty = gameData.Documents.All.Where(document => document.IsModified
                    || pending.Contains(document.Path) || pending.Contains(document.SavedPath)).ToArray();
                HashSet<string> dirtyPaths = dirty.SelectMany(document => new[] { document.Path, document.SavedPath })
                    .ToHashSet(ProjectDiskSnapshot.PathComparer);
                ExternalFileChange[] conflicts = changes.Where(change => dirtyPaths.Contains(change.Path)).ToArray();
                List<EditorDocument[]> renamedGroups = [];
                HashSet<EditorDocument> grouped = [];
                foreach (EditorDocument world in dirty.Where(document => document.Section == "WorldMaps"
                    && !ProjectDiskSnapshot.PathComparer.Equals(document.Path, document.SavedPath)))
                {
                    string source = Path.GetDirectoryName(world.SavedPath)!;
                    string destination = Path.GetDirectoryName(world.Path)!;
                    EditorDocument[] group = dirty.Where(document => ReferenceEquals(document, world)
                        || document.Section == "Maps" && (ProjectDiskSnapshot.IsWithin(document.SavedPath, source)
                            || ProjectDiskSnapshot.IsWithin(document.Path, destination))).ToArray();
                    renamedGroups.Add(group);
                    grouped.UnionWith(group);
                }
                renamedGroups.AddRange(dirty.Where(document => !grouped.Contains(document)
                    && !ProjectDiskSnapshot.PathComparer.Equals(document.Path, document.SavedPath)).Select(document => new[] { document }));
                IReadOnlySet<string>? selected = conflicts.Length == 0
                    ? new HashSet<string>(ProjectDiskSnapshot.PathComparer) : await resolveConflicts(conflicts);
                token.ThrowIfCancellationRequested();
                if (selected is null)
                    return false;
                HashSet<string> keep = new(selected, ProjectDiskSnapshot.PathComparer);
                HashSet<EditorDocument> relatedDocuments = [];
                foreach (EditorDocument[] group in renamedGroups)
                {
                    HashSet<string> paths = group.SelectMany(document => new[] { document.Path, document.SavedPath })
                        .ToHashSet(ProjectDiskSnapshot.PathComparer);
                    ExternalFileChange[] affected = conflicts.Where(change => paths.Contains(change.Path)).ToArray();
                    if (affected.Length == 0)
                        continue;
                    if (affected.Select(change => keep.Contains(change.Path)).Distinct().Count() != 1)
                        throw new InvalidDataException(LocaleService.Get("EXTERNAL_RENAME_SELECTION") + Environment.NewLine
                            + string.Join(Environment.NewLine, affected.Select(change => change.Path)));
                    relatedDocuments.UnionWith(group);
                    if (affected.Any(change => keep.Contains(change.Path)))
                        keep.UnionWith(paths);
                }
                foreach (EditorDocument document in dirty.Where(document => keep.Contains(document.Path) || keep.Contains(document.SavedPath)))
                {
                    keep.Add(document.Path);
                    keep.Add(document.SavedPath);
                }
                ProjectDiskSnapshot confirmed = await Task.Run(() => ProjectDiskSnapshot.Capture(gameData.ProjectPath, token,
                    false, progress, "EXTERNAL_RECHECKING"), token);
                bool confirmedMatches = await Task.Run(() => current.Matches(confirmed), token);
                token.ThrowIfCancellationRequested();
                lock (sync)
                    if (revision != baselineRevision || !confirmedMatches)
                        continue;
                if (gameData.Documents.Revision != documentRevision)
                    continue;
                Dictionary<string, ExternalFileChange> related = changes.ToDictionary(change => change.Path, ProjectDiskSnapshot.PathComparer);
                foreach (EditorDocument document in relatedDocuments)
                    foreach (string path in new[] { document.SavedPath, document.Path })
                        related.TryAdd(path, new ExternalFileChange(path, previous.GetValueOrDefault(path),
                            current.Fingerprints.GetValueOrDefault(path), current.Data.GetValueOrDefault(path)));
                await gameData.ApplyDiskChangesAsync(related.Values.ToArray(), keep, progress, token, async () =>
                {
                    ProjectDiskSnapshot latest = await Task.Run(() => ProjectDiskSnapshot.Capture(gameData.ProjectPath, token,
                        false, progress, "EXTERNAL_RECHECKING"), token);
                    bool matches = await Task.Run(() => current.Matches(latest), token);
                    lock (sync)
                        return revision == baselineRevision && matches;
                });
                lock (sync)
                {
                    baseline.Clear();
                    foreach (KeyValuePair<string, string> pair in current.Fingerprints)
                        baseline[pair.Key] = pair.Value;
                    baselineRevision++;
                }
                progress?.Report(new("EDIT_OPERATION_APPLYING", Completed: changes.Length, Total: changes.Length));
                await EditorUiBatch.YieldAsync();
                Synchronized?.Invoke(this, new ExternalFilesSynchronizedEventArgs(changes.Select(change => change.Path).ToArray()));
                return true;
            }
            throw new IOException(LocaleService.Get("EDIT_OPERATION_INPUTS_CHANGED"));
        }
        finally
        {
            IsSynchronizing = false;
        }
    }

    internal bool IsCurrent()
    {
        Dictionary<string, string> expected;
        lock (sync)
            expected = new(baseline, ProjectDiskSnapshot.PathComparer);
        ProjectDiskSnapshot current = ProjectDiskSnapshot.Capture(gameData.ProjectPath, lifetime.Token, false);
        return ProjectDiskSnapshot.Equal(expected, current.Fingerprints);
    }

    internal void AcceptFileOperations(IEnumerable<string> paths)
    {
        Dictionary<string, string?> fingerprints = new(ProjectDiskSnapshot.PathComparer);
        string[] roots = paths.Select(Path.GetFullPath).ToArray();
        lock (sync)
            foreach (string path in baseline.Keys.Where(path => roots.Any(root => ProjectDiskSnapshot.IsWithin(path, root))).ToArray())
                fingerprints[path] = null;
        foreach (string path in roots)
        {
            if (File.Exists(path))
                fingerprints[path] = ProjectDiskSnapshot.ReadFingerprint(path);
            else if (Directory.Exists(path))
                foreach (string file in Directory.EnumerateFiles(path, "*", SearchOption.AllDirectories))
                    fingerprints[file] = ProjectDiskSnapshot.ReadFingerprint(file);
        }
        acceptWrites(fingerprints);
    }

    private void acceptWrites(IReadOnlyDictionary<string, string?> fingerprints)
    {
        lock (sync)
        {
            if (disposed)
                return;
            foreach (KeyValuePair<string, string?> pair in fingerprints)
            {
                if (DataConfig.isAnimationCache(pair.Key))
                    continue;
                if (!ProjectDiskSnapshot.IsWithin(pair.Key, Path.Combine(gameData.ProjectPath, "Data"))
                    && !ProjectDiskSnapshot.IsWithin(pair.Key, Path.Combine(gameData.ProjectPath, "Assets")))
                    continue;
                if (pair.Value is null)
                    baseline.Remove(pair.Key);
                else
                    baseline[pair.Key] = pair.Value;
            }
            baselineRevision++;
        }
    }

    public void Dispose()
    {
        lock (sync)
            disposed = true;
        EditorFileSaveBatch.FilesCommitted -= acceptWrites;
        lifetime.Cancel();
        lifetime.Dispose();
    }
}
