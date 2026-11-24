using Ludork.Models;
using Ludork.Plugin.Abstractions;
using MoonSharp.Interpreter;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;

namespace Ludork.Services;

internal sealed class ResourceCleanupScanner
{
    private const int BufferSize = 128 * 1024;
    private static readonly IReadOnlySet<string> RemovableTypes = new HashSet<string>(StringComparer.Ordinal)
    {
        "material", "tileset", "autoTile", "blueprint", "commonFunction", "animation", "particle", "curve", "textConfig",
    };

    public ResourceCleanupSnapshot Scan(
        string projectPath,
        IReadOnlyList<string> nativeKeepPaths,
        IReadOnlyList<string> userKeepPaths,
        IProgress<ResourceCleanupProgress>? progress,
        CancellationToken token)
    {
        string root = Path.GetFullPath(projectPath);
        List<ResourceCleanupIssue> issues = [];
        IReadOnlyList<string> files = ResourceCleanupFileSystem.EnumerateInputs(root, issues, progress, token);
        FileFingerprintCache cache = new(StringComparer.Ordinal);
        bool analyze = issues.Count == 0;
        Action<string> reportFile = createProgressReporter("Fingerprint", files.Count, progress);
        FileRead[] reads = EditorValidationBatch.RunFiles(files, static () => new byte[BufferSize], (buffer, path) =>
        {
            reportFile(path);
            return readInput(root, path, cache, buffer, analyze, token);
        }, cancellationToken: token);
        Dictionary<string, ResourceCleanupFileFingerprint> fingerprints = new(StringComparer.Ordinal);
        foreach (FileRead read in reads)
        {
            if (read.Fingerprint is ResourceCleanupFileFingerprint fingerprint)
                fingerprints.Add(read.Path, fingerprint);
            else
                issues.Add(read.Issue!);
        }
        Dictionary<string, HashSet<string>> edges = files.ToDictionary(
            path => path, _ => new HashSet<string>(StringComparer.Ordinal), StringComparer.Ordinal);
        HashSet<string> candidates = new(StringComparer.Ordinal);
        HashSet<string> roots = new(StringComparer.Ordinal);
        if (issues.Count == 0)
        {
            applyReferences(reads, "Data/", (path, targets) => edges[path].UnionWith(targets),
                issues, "References", progress, token);
            if (issues.Count == 0)
                buildIndex(root, edges, candidates, issues, progress, token);
        }
        foreach (string file in files)
        {
            if (file.StartsWith("Assets/", StringComparison.Ordinal))
                candidates.Add(file);
            else if (file.StartsWith("Data/", StringComparison.Ordinal) && !candidates.Contains(file))
                roots.Add(file);
        }
        applyKeepPaths(root, nativeKeepPaths.Concat(userKeepPaths), files, roots, issues, token);
        if (issues.Count == 0)
            applyReferences(reads, "Scripts/", (_, targets) => roots.UnionWith(targets),
                issues, "Lua", progress, token);
        progress?.Report(new ResourceCleanupProgress("Reachability", 0, edges.Count, string.Empty));
        HashSet<string> reachable = new(StringComparer.Ordinal);
        Queue<string> pending = new(roots);
        while (pending.TryDequeue(out string? current))
        {
            token.ThrowIfCancellationRequested();
            if (!reachable.Add(current) || !edges.TryGetValue(current, out HashSet<string>? targets))
                continue;
            foreach (string target in targets)
                pending.Enqueue(target);
        }
        candidates.ExceptWith(reachable);
        candidates.IntersectWith(fingerprints.Keys);
        IReadOnlyList<string> order = ResourceCleanupOrder.Create(candidates, edges, token);
        ResourceCleanupCandidate[] items = order.Select(path => new ResourceCleanupCandidate(
            path, path.StartsWith("Assets/", StringComparison.Ordinal) ? "Assets" : "Data",
            fingerprints[path].Size)).ToArray();
        ResourceCleanupReport report = new(Guid.NewGuid().ToString("N"), items, issues.ToArray());
        ResourceCleanupSnapshot snapshot = new(root, report, fingerprints, order, cache);
        if (issues.Count == 0)
        {
            try
            {
                Validate(snapshot, progress, token);
            }
            catch (Exception exception) when (ResourceCleanupFileSystem.IsReadFailure(exception))
            {
                issues.Add(new ResourceCleanupIssue(string.Empty, exception.Message));
                snapshot = snapshot with { Report = report with { Issues = issues.ToArray() } };
            }
        }
        return snapshot;
    }

    public void Validate(
        ResourceCleanupSnapshot snapshot,
        IProgress<ResourceCleanupProgress>? progress,
        CancellationToken token)
    {
        if (snapshot.Report.Issues.Count != 0)
            throw new InvalidDataException("The scan is incomplete; resolve its errors and scan again.");
        List<ResourceCleanupIssue> issues = [];
        IReadOnlyList<string> current = ResourceCleanupFileSystem.EnumerateInputs(snapshot.ProjectPath, issues, progress, token);
        if (issues.Count != 0)
            throw new InvalidDataException(issues[0].Message);
        if (!current.SequenceEqual(snapshot.Fingerprints.Keys.OrderBy(path => path, StringComparer.Ordinal), StringComparer.Ordinal))
            throw new InvalidDataException("Project files were added or deleted; scan again.");
        Action<string> report = createProgressReporter("Validation", current.Count, progress);
        EditorValidationBatch.RunFiles(current, static () => new byte[BufferSize], (buffer, path) =>
        {
            report(path);
            validateFingerprint(snapshot, path, buffer, token);
            return true;
        }, cancellationToken: token);
    }

    public string ValidateCandidate(ResourceCleanupSnapshot snapshot, string relativePath, CancellationToken token)
    {
        token.ThrowIfCancellationRequested();
        if (!snapshot.Report.CanTrash || !snapshot.DeletionOrder.Contains(relativePath, StringComparer.Ordinal))
            throw new InvalidDataException("The file is not an approved cleanup candidate.");
        validateFingerprint(snapshot, relativePath, new byte[BufferSize], token, forceRead: true);
        return ResourceCleanupFileSystem.ResolveSafePath(snapshot.ProjectPath, relativePath);
    }

    private static void validateFingerprint(ResourceCleanupSnapshot snapshot, string relativePath, byte[] buffer,
        CancellationToken token, bool forceRead = false)
    {
        if (!snapshot.Fingerprints.TryGetValue(relativePath, out ResourceCleanupFileFingerprint? previous)
            || previous != ResourceCleanupFileSystem.ReadFile(snapshot.ProjectPath, relativePath,
                snapshot.FileCache, buffer, token, forceRead: forceRead).Fingerprint)
            throw new InvalidDataException($"The project changed; scan again: {relativePath}");
    }

    private sealed record FileRead(string Path, ResourceCleanupFileFingerprint? Fingerprint,
        IReadOnlySet<string>? References, ResourceCleanupIssue? Issue);

    private static FileRead readInput(string root, string path, FileFingerprintCache cache, byte[] buffer,
        bool analyze, CancellationToken token)
    {
        ResourceCleanupFileFingerprint? fingerprint = null;
        try
        {
            bool json = analyze && path.StartsWith("Data/", StringComparison.Ordinal)
                && path.EndsWith(".json", StringComparison.OrdinalIgnoreCase);
            bool lua = analyze && ResourceCleanupLuaScanner.IsHandwrittenRuntime(path);
            (fingerprint, byte[]? content) = ResourceCleanupFileSystem.ReadFile(root, path, cache, buffer,
                token, includeContent: json || lua);
            if (content is null)
                return new(path, fingerprint, null, null);
            using MemoryStream stream = new(content, writable: false);
            using StreamReader reader = new(stream);
            string source = reader.ReadToEnd();
            if (lua)
                return new(path, fingerprint, ResourceCleanupLuaScanner.ReadReferences(source, token), null);
            HashSet<string> targets = new(StringComparer.Ordinal);
            readJsonReferences(JsonNode.Parse(source), targets, token);
            return new(path, fingerprint, targets, null);
        }
        catch (Exception exception) when (ResourceCleanupFileSystem.IsReadFailure(exception))
        {
            return new(path, fingerprint, null, new ResourceCleanupIssue(path, exception.Message));
        }
    }

    private static void readJsonReferences(JsonNode? node, ISet<string> targets, CancellationToken token)
    {
        token.ThrowIfCancellationRequested();
        if (node is JsonValue value && value.TryGetValue(out string? text))
        {
            if (ResourceCleanupLuaScanner.ExplicitReference(text) is string path)
                targets.Add(path);
        }
        else if (node is JsonObject record)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in record)
            {
                if (ResourceCleanupLuaScanner.ExplicitReference(pair.Key) is string path)
                    targets.Add(path);
                readJsonReferences(pair.Value, targets, token);
            }
        }
        else if (node is JsonArray array)
        {
            foreach (JsonNode? child in array)
                readJsonReferences(child, targets, token);
        }
    }

    private static void buildIndex(
        string root,
        Dictionary<string, HashSet<string>> edges,
        ISet<string> candidates,
        ICollection<ResourceCleanupIssue> issues,
        IProgress<ResourceCleanupProgress>? progress,
        CancellationToken token)
    {
        try
        {
            token.ThrowIfCancellationRequested();
            progress?.Report(new ResourceCleanupProgress("References", 0, 0, string.Empty));
            Action<string> reportProgress = path => progress?.Report(
                new ResourceCleanupProgress("References", 0, 0, path));
            using ProjectDataStore data = new(root, cacheMapCatalog: false, token, reportProgress);
            foreach (string path in data.InvalidLoadPaths)
                issues.Add(new ResourceCleanupIssue(path, "The data file could not be parsed or validated."));
            LuaMetadataService metadata = new(root, strictReads: true, token);
            using IDisposable metadataRead = metadata.BeginRead();
            using BlueprintClassResolver resolver = new(data, metadata);
            using ReferenceIndexService index = new(data, metadata, resolver, token, reportProgress);
            IReadOnlyDictionary<string, JsonObject> blueprints = SnapshotJson.ToDictionary(data.Blueprints.BlueprintsData);
            ProjectEnumCatalog enums = data.GetProjectEnumCatalog();
            LuaValidationReadCache reads = new(root, enums, token);
            string[] keys = blueprints.Keys.ToArray();
            string?[] mixinErrors = EditorValidationBatch.RunFiles(keys,
                () => new BlueprintValidationWorker(root, blueprints, enums, reads, strictReads: true),
                (worker, key) => worker.Resolver.ResolveBlueprint(blueprints[key], key).ScriptMixinError,
                worker => worker.Dispose(), token);
            for (int offset = 0; offset < keys.Length; offset++)
            {
                if (mixinErrors[offset] is string error)
                    issues.Add(new ResourceCleanupIssue($"Data/Blueprints/{keys[offset]}.json", error));
            }
            IReadOnlyList<ReferenceNode> nodes = index.GetAllNodes();
            string[] absolutePaths = new string[nodes.Count];
            List<int> assetOffsets = [];
            for (int offset = 0; offset < nodes.Count; offset++)
            {
                if (nodes[offset].Type == "asset")
                    assetOffsets.Add(offset);
                else
                    absolutePaths[offset] = index.GetNodePath(nodes[offset].Id);
            }
            EditorValidationBatch.ForEachFile(assetOffsets,
                offset => absolutePaths[offset] = index.GetNodePath(nodes[offset].Id), token);
            string[] uniquePaths = absolutePaths.Where(path => path.Length != 0).Distinct(StringComparer.Ordinal).ToArray();
            EditorValidationBatch.ForEachFile(uniquePaths, path =>
                ResourceCleanupFileSystem.ResolveSafePath(root, Path.GetRelativePath(root, path).Replace('\\', '/'), allowMissing: true), token);
            Dictionary<string, string> nodePaths = new(StringComparer.Ordinal);
            for (int offset = 0; offset < nodes.Count; offset++)
            {
                token.ThrowIfCancellationRequested();
                ReferenceNode node = nodes[offset];
                string absolute = absolutePaths[offset];
                if (absolute.Length == 0)
                    continue;
                string path = Path.GetRelativePath(root, absolute).Replace('\\', '/');
                if (!edges.ContainsKey(path))
                    continue;
                nodePaths[node.Id] = path;
                if (RemovableTypes.Contains(node.Type) && index.GetNodeIdForPath(absolute) == node.Id)
                    candidates.Add(path);
                progress?.Report(new ResourceCleanupProgress("References", offset, nodes.Count, path));
            }
            foreach (ReferenceRecord reference in index.GetAllReferences())
            {
                token.ThrowIfCancellationRequested();
                if (nodePaths.TryGetValue(reference.Source, out string? source)
                    && nodePaths.TryGetValue(reference.Target, out string? target) && source != target)
                    edges[source].Add(target);
            }
        }
        catch (Exception exception) when (ResourceCleanupFileSystem.IsReadFailure(exception)
            || exception is InterpreterException or InvalidOperationException or FormatException)
        {
            issues.Add(new ResourceCleanupIssue("Data", exception.Message));
        }
    }

    private static void applyKeepPaths(
        string root,
        IEnumerable<string> keepPaths,
        IReadOnlyList<string> files,
        ISet<string> roots,
        ICollection<ResourceCleanupIssue> issues,
        CancellationToken token)
    {
        HashSet<string> protectedPaths = new(StringComparer.Ordinal);
        (string? Path, ResourceCleanupIssue? Issue)[] checkedPaths =
            EditorValidationBatch.RunFiles<string, (string?, ResourceCleanupIssue?)>(keepPaths.ToArray(), path =>
        {
            try
            {
                string normalized = ResourceCleanupFileSystem.NormalizeKeepPath(path);
                ResourceCleanupFileSystem.ResolveSafePath(root, normalized, allowMissing: true);
                return (normalized, null);
            }
            catch (Exception exception) when (ResourceCleanupFileSystem.IsReadFailure(exception))
            {
                return (null, new ResourceCleanupIssue(path, exception.Message));
            }
        }, token);
        foreach ((string? path, ResourceCleanupIssue? issue) in checkedPaths)
        {
            if (issue is not null)
                issues.Add(issue);
            else
                protectedPaths.Add(path!);
        }
        foreach (string file in files)
        {
            token.ThrowIfCancellationRequested();
            string path = file;
            while (true)
            {
                if (protectedPaths.Contains(path))
                {
                    roots.Add(file);
                    break;
                }
                int separator = path.LastIndexOf('/');
                if (separator < 0)
                    break;
                path = path[..separator];
            }
        }
    }

    private static void applyReferences(FileRead[] reads, string prefix, Action<string, IReadOnlySet<string>> apply,
        ICollection<ResourceCleanupIssue> issues, string stage, IProgress<ResourceCleanupProgress>? progress,
        CancellationToken token)
    {
        FileRead[] sources = reads.Where(read => (read.References is not null || read.Issue is not null)
            && read.Path.StartsWith(prefix, StringComparison.Ordinal)).ToArray();
        for (int index = 0; index < sources.Length; index++)
        {
            token.ThrowIfCancellationRequested();
            FileRead source = sources[index];
            progress?.Report(new ResourceCleanupProgress(stage, index, sources.Length, source.Path));
            if (source.Issue is not null)
                issues.Add(source.Issue);
            else
                apply(source.Path, source.References!);
        }
    }

    private static Action<string> createProgressReporter(string stage, int total, IProgress<ResourceCleanupProgress>? progress)
    {
        if (progress is null)
            return static _ => { };
        object progressLock = new();
        int completed = 0;
        return path =>
        {
            lock (progressLock)
                progress.Report(new ResourceCleanupProgress(stage, completed++, total, path));
        };
    }
}
