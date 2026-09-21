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
    private static readonly IReadOnlySet<string> RemovableTypes = new HashSet<string>(StringComparer.Ordinal)
    {
        "tileset", "autoTile", "blueprint", "commonFunction", "animation", "particle", "curve", "textConfig",
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
        Dictionary<string, ResourceCleanupFileFingerprint> fingerprints = new(StringComparer.Ordinal);
        for (int index = 0; index < files.Count; index++)
        {
            token.ThrowIfCancellationRequested();
            string path = files[index];
            progress?.Report(new ResourceCleanupProgress("Fingerprint", index, files.Count, path));
            try
            {
                fingerprints[path] = ResourceCleanupFileSystem.Fingerprint(root, path, token);
            }
            catch (Exception exception) when (ResourceCleanupFileSystem.IsReadFailure(exception))
            {
                issues.Add(new ResourceCleanupIssue(path, exception.Message));
            }
        }
        Dictionary<string, HashSet<string>> edges = files.ToDictionary(
            path => path, _ => new HashSet<string>(StringComparer.Ordinal), StringComparer.Ordinal);
        HashSet<string> candidates = new(StringComparer.Ordinal);
        HashSet<string> roots = new(StringComparer.Ordinal);
        if (issues.Count == 0)
        {
            readData(root, files, edges, issues, progress, token);
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
            readLua(root, files, roots, issues, progress, token);
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
        ResourceCleanupSnapshot snapshot = new(root, report, fingerprints, order);
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
        for (int index = 0; index < current.Count; index++)
        {
            token.ThrowIfCancellationRequested();
            string path = current[index];
            progress?.Report(new ResourceCleanupProgress("Validation", index, current.Count, path));
            validateFingerprint(snapshot, path, token);
        }
    }

    public string ValidateCandidate(ResourceCleanupSnapshot snapshot, string relativePath, CancellationToken token)
    {
        token.ThrowIfCancellationRequested();
        if (!snapshot.Report.CanTrash || !snapshot.DeletionOrder.Contains(relativePath, StringComparer.Ordinal))
            throw new InvalidDataException("The file is not an approved cleanup candidate.");
        validateFingerprint(snapshot, relativePath, token);
        return ResourceCleanupFileSystem.ResolveSafePath(snapshot.ProjectPath, relativePath);
    }

    private static void validateFingerprint(ResourceCleanupSnapshot snapshot, string relativePath, CancellationToken token)
    {
        if (!snapshot.Fingerprints.TryGetValue(relativePath, out ResourceCleanupFileFingerprint? previous)
            || previous != ResourceCleanupFileSystem.Fingerprint(snapshot.ProjectPath, relativePath, token))
            throw new InvalidDataException($"The project changed; scan again: {relativePath}");
    }

    private static void readData(
        string root,
        IReadOnlyList<string> files,
        Dictionary<string, HashSet<string>> edges,
        ICollection<ResourceCleanupIssue> issues,
        IProgress<ResourceCleanupProgress>? progress,
        CancellationToken token)
    {
        string[] documents = files.Where(path => path.StartsWith("Data/", StringComparison.Ordinal)
            && path.EndsWith(".json", StringComparison.OrdinalIgnoreCase)).ToArray();
        for (int index = 0; index < documents.Length; index++)
        {
            token.ThrowIfCancellationRequested();
            string path = documents[index];
            progress?.Report(new ResourceCleanupProgress("References", index, documents.Length, path));
            try
            {
                JsonNode? data = JsonNode.Parse(File.ReadAllText(ResourceCleanupFileSystem.ResolveSafePath(root, path)));
                readJsonReferences(data, edges[path], token);
            }
            catch (Exception exception) when (ResourceCleanupFileSystem.IsReadFailure(exception))
            {
                issues.Add(new ResourceCleanupIssue(path, exception.Message));
            }
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
            foreach (KeyValuePair<string, JsonObject> blueprint in SnapshotJson.ToDictionary(data.Blueprints.BlueprintsData))
            {
                token.ThrowIfCancellationRequested();
                ResolvedBlueprintClass resolved = resolver.ResolveBlueprint(blueprint.Value, blueprint.Key);
                if (resolved.ScriptMixinError is string error)
                    issues.Add(new ResourceCleanupIssue($"Data/Blueprints/{blueprint.Key}.json", error));
            }
            IReadOnlyList<ReferenceNode> nodes = index.GetAllNodes();
            Dictionary<string, string> nodePaths = new(StringComparer.Ordinal);
            for (int offset = 0; offset < nodes.Count; offset++)
            {
                token.ThrowIfCancellationRequested();
                ReferenceNode node = nodes[offset];
                string absolute = index.GetNodePath(node.Id);
                if (absolute.Length == 0)
                    continue;
                string path = Path.GetRelativePath(root, absolute).Replace('\\', '/');
                ResourceCleanupFileSystem.ResolveSafePath(root, path);
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
        foreach (string path in keepPaths)
        {
            token.ThrowIfCancellationRequested();
            try
            {
                string normalized = ResourceCleanupFileSystem.NormalizeKeepPath(path);
                ResourceCleanupFileSystem.ResolveSafePath(root, normalized);
                string prefix = normalized + "/";
                foreach (string file in files)
                {
                    if (file == normalized || file.StartsWith(prefix, StringComparison.Ordinal))
                        roots.Add(file);
                }
            }
            catch (Exception exception) when (ResourceCleanupFileSystem.IsReadFailure(exception))
            {
                issues.Add(new ResourceCleanupIssue(path, exception.Message));
            }
        }
    }

    private static void readLua(
        string root,
        IReadOnlyList<string> files,
        ISet<string> roots,
        ICollection<ResourceCleanupIssue> issues,
        IProgress<ResourceCleanupProgress>? progress,
        CancellationToken token)
    {
        string[] scripts = files.Where(ResourceCleanupLuaScanner.IsHandwrittenRuntime).ToArray();
        for (int index = 0; index < scripts.Length; index++)
        {
            token.ThrowIfCancellationRequested();
            string path = scripts[index];
            progress?.Report(new ResourceCleanupProgress("Lua", index, scripts.Length, path));
            try
            {
                string source = File.ReadAllText(ResourceCleanupFileSystem.ResolveSafePath(root, path));
                roots.UnionWith(ResourceCleanupLuaScanner.ReadReferences(source, token));
            }
            catch (Exception exception) when (ResourceCleanupFileSystem.IsReadFailure(exception))
            {
                issues.Add(new ResourceCleanupIssue(path, exception.Message));
            }
        }
    }
}
