using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;

namespace Ludork.Services;

internal sealed class LuaMetadataHierarchy
{
    private readonly LuaMetadataFileCache files;
    private readonly Dictionary<string, IReadOnlyList<LuaTypeMetadata>> mroCache = new(StringComparer.Ordinal);
    private LuaStubTypeHierarchy? stubTypeHierarchy;

    public LuaMetadataHierarchy(LuaMetadataFileCache files)
    {
        this.files = files;
        files.Invalidated += clear;
    }

    public bool IsCurrent => stubTypeHierarchy is null || stubTypeHierarchy.IsCurrent();

    private void clear()
    {
        mroCache.Clear();
        stubTypeHierarchy = null;
    }

    public void AdoptPreparedState(LuaMetadataHierarchy prepared)
    {
        mroCache.Clear();
        foreach (KeyValuePair<string, IReadOnlyList<LuaTypeMetadata>> entry in prepared.mroCache)
            mroCache.Add(entry.Key, entry.Value);
        stubTypeHierarchy = prepared.stubTypeHierarchy;
    }

    public bool IsDerivedType(string source, string target)
    {
        stubTypeHierarchy ??= new LuaStubTypeHierarchy(Path.Combine(files.ProjectPath, "Scripts", "stub"));
        Stack<string> pending = new();
        HashSet<string> visited = new(StringComparer.Ordinal);
        pending.Push(source);
        while (pending.TryPop(out string? current))
        {
            if (string.Equals(current, target, StringComparison.Ordinal))
                return true;
            if (!visited.Add(current))
                continue;
            LuaTypeMetadata? metadata = files.GetType(LuaTypeReference.Parse(current));
            if (metadata is not null)
            {
                foreach (LuaTypeReference parent in metadata.Bases)
                    pending.Push(parent.WithDefaultModule(metadata.Type.ModuleName).QualifiedName);
            }
            foreach (string parent in stubTypeHierarchy.GetBases(current))
                pending.Push(parent);
        }
        return false;
    }

    public IReadOnlyDictionary<string, IReadOnlySet<string>> CaptureAncestors(
        IReadOnlyList<LuaTypeMetadata> types, CancellationToken cancellationToken)
    {
        stubTypeHierarchy ??= new LuaStubTypeHierarchy(
            Path.Combine(files.ProjectPath, "Scripts", "stub"), cancellationToken);
        Dictionary<string, HashSet<string>> directBases = new(StringComparer.Ordinal);
        foreach (string name in stubTypeHierarchy.TypeNames)
        {
            cancellationToken.ThrowIfCancellationRequested();
            directBases[name] = new HashSet<string>(stubTypeHierarchy.GetBases(name), StringComparer.Ordinal);
        }
        foreach (LuaTypeMetadata metadata in types)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (!directBases.TryGetValue(metadata.Type.QualifiedName, out HashSet<string>? parents))
            {
                parents = new HashSet<string>(StringComparer.Ordinal);
                directBases[metadata.Type.QualifiedName] = parents;
            }
            parents.UnionWith(metadata.Bases.Select(parent => parent.WithDefaultModule(metadata.Type.ModuleName).QualifiedName));
        }
        Dictionary<string, IReadOnlySet<string>> result = new(StringComparer.Ordinal);
        foreach (string name in directBases.Keys)
        {
            cancellationToken.ThrowIfCancellationRequested();
            HashSet<string> ancestors = new(StringComparer.Ordinal);
            Stack<string> pending = new(directBases[name]);
            while (pending.TryPop(out string? current))
            {
                cancellationToken.ThrowIfCancellationRequested();
                if (!ancestors.Add(current) || !directBases.TryGetValue(current, out HashSet<string>? parents))
                    continue;
                foreach (string parent in parents)
                    pending.Push(parent);
            }
            result[name] = ancestors;
        }
        return result;
    }

    private IReadOnlyList<LuaTypeMetadata> resolveMro(
        LuaTypeReference type,
        Dictionary<string, IReadOnlyList<LuaTypeMetadata>> resolved,
        HashSet<string> resolving
    )
    {
        string key = type.QualifiedName;
        if (resolved.TryGetValue(key, out IReadOnlyList<LuaTypeMetadata>? existing))
            return existing;
        if (mroCache.TryGetValue(key, out IReadOnlyList<LuaTypeMetadata>? cached))
            return cached;
        LuaTypeMetadata? current = files.GetType(type);
        if (current is null || !resolving.Add(key))
            return Array.Empty<LuaTypeMetadata>();

        List<IReadOnlyList<LuaTypeMetadata>> baseMros = [];
        List<LuaTypeMetadata> directBases = [];
        foreach (LuaTypeReference baseReference in current.Bases)
        {
            LuaTypeMetadata? directBase = files.GetType(baseReference, current.Type.ModuleName);
            if (directBase is null)
                continue;
            IReadOnlyList<LuaTypeMetadata> baseMro = resolveMro(directBase.Type, resolved, resolving);
            if (baseMro.Count == 0)
                baseMro = [directBase];
            baseMros.Add(baseMro);
            directBases.Add(directBase);
        }
        resolving.Remove(key);

        List<LuaTypeMetadata> result = [current];
        List<List<LuaTypeMetadata>> sequences = baseMros.Select(mro => mro.ToList()).ToList();
        if (directBases.Count != 0)
            sequences.Add(directBases.ToList());
        mergeC3(result, sequences);
        IReadOnlyList<LuaTypeMetadata> materialized = result.ToArray();
        resolved[key] = materialized;
        return materialized;
    }

    public IReadOnlyList<LuaTypeMetadata> ResolveMro(LuaTypeReference type)
    {
        string key = type.QualifiedName;
        if (mroCache.TryGetValue(key, out IReadOnlyList<LuaTypeMetadata>? cached))
            return cached;
        Dictionary<string, IReadOnlyList<LuaTypeMetadata>> resolved = new(StringComparer.Ordinal);
        HashSet<string> resolving = new(StringComparer.Ordinal);
        IReadOnlyList<LuaTypeMetadata> result = resolveMro(type, resolved, resolving);
        foreach (KeyValuePair<string, IReadOnlyList<LuaTypeMetadata>> entry in resolved)
            mroCache[entry.Key] = entry.Value;
        if (!mroCache.ContainsKey(key))
            mroCache[key] = result;
        return result;
    }

    private static void mergeC3(List<LuaTypeMetadata> result, List<List<LuaTypeMetadata>> sequences)
    {
        while (sequences.Any(sequence => sequence.Count != 0))
        {
            sequences.RemoveAll(sequence => sequence.Count == 0);
            LuaTypeMetadata? candidate = sequences
                .Select(sequence => sequence[0])
                .FirstOrDefault(head => sequences.All(sequence => sequence.Skip(1).All(item => item.Type != head.Type)));
            if (candidate is null)
            {
                foreach (LuaTypeMetadata item in sequences.SelectMany(sequence => sequence))
                {
                    if (result.All(existing => existing.Type != item.Type))
                        result.Add(item);
                }
                return;
            }

            if (result.All(existing => existing.Type != candidate.Type))
                result.Add(candidate);
            foreach (List<LuaTypeMetadata> sequence in sequences)
            {
                if (sequence.Count != 0 && sequence[0].Type == candidate.Type)
                    sequence.RemoveAt(0);
            }
        }
    }

}
