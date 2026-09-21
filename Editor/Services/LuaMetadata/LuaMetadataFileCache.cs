using Ludork.Models;
using MoonSharp.Interpreter;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;

namespace Ludork.Services;

internal sealed class LuaMetadataFileCache
{
    private readonly string scriptsPath;
    private readonly bool strictReads;
    private readonly CancellationToken cancellationToken;
    private readonly Dictionary<string, CachedMetadataFile> fileCache = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, CachedScriptMixinMetadata> scriptMixinCache = new(StringComparer.OrdinalIgnoreCase);
    private CachedMetadataFileSet? metadataFileSet;
    private Dictionary<string, LuaMetadataFileStamp>? readDependencyStamps;
    private bool readDependenciesConsistent = true;
    private int readScopeDepth;
    private long revision;

    public LuaMetadataFileCache(string projectPath, bool strictReads = false, CancellationToken cancellationToken = default)
    {
        this.strictReads = strictReads;
        this.cancellationToken = cancellationToken;
        ProjectPath = Path.GetFullPath(projectPath);
        scriptsPath = Path.Combine(ProjectPath, "Scripts");
    }

    public string ProjectPath { get; }

    public long CacheRevision => revision;
    public bool IsReading => readScopeDepth != 0;
    public event Action? Invalidated;

    public IDisposable BeginRead()
    {
        if (readScopeDepth == 0)
        {
            EnsureCurrent();
            readDependencyStamps = new Dictionary<string, LuaMetadataFileStamp>(StringComparer.OrdinalIgnoreCase);
            readDependenciesConsistent = true;
        }
        readScopeDepth++;
        return new MetadataReadScope(this);
    }

    internal LuaMetadataDependencySnapshot CaptureDependencies(
        IEnumerable<LuaTypeReference> types,
        IEnumerable<string> scriptMixins)
    {
        HashSet<string> paths = new(StringComparer.OrdinalIgnoreCase);
        foreach (LuaTypeReference type in types)
        {
            if (!string.IsNullOrWhiteSpace(type.ModuleName))
                paths.Add(GetMetadataPath(type.ModuleName));
        }
        foreach (string scriptMixin in scriptMixins)
        {
            string normalized = ScriptMixinPaths.Normalize(scriptMixin);
            if (!string.IsNullOrEmpty(normalized))
                paths.Add(ScriptMixinPaths.GetMetadataPath(ProjectPath, normalized));
        }
        Dictionary<string, LuaMetadataFileStamp> stamps = new(StringComparer.OrdinalIgnoreCase);
        bool consistent = readDependenciesConsistent;
        bool coveredByReadSnapshot = true;
        foreach (string path in paths)
        {
            bool cachedByFile = fileCache.ContainsKey(path);
            bool cachedByMixin = scriptMixinCache.Values.Any(
                cached => string.Equals(cached.Path, path, StringComparison.OrdinalIgnoreCase));
            if (!cachedByFile && !cachedByMixin)
                coveredByReadSnapshot = false;
            if (readDependencyStamps?.TryGetValue(path, out LuaMetadataFileStamp readStamp) == true)
            {
                stamps[path] = readStamp;
                if (readStamp != getFileStamp(path))
                    consistent = false;
                continue;
            }
            if (fileCache.TryGetValue(path, out CachedMetadataFile? cachedFile))
            {
                stamps[path] = cachedFile.Stamp;
                if (cachedFile.Stamp != getFileStamp(path))
                    consistent = false;
                continue;
            }
            CachedScriptMixinMetadata? cachedMixin = scriptMixinCache.Values.FirstOrDefault(
                cached => string.Equals(cached.Path, path, StringComparison.OrdinalIgnoreCase));
            if (cachedMixin is not null)
            {
                stamps[path] = cachedMixin.Stamp;
                if (cachedMixin.Stamp != getFileStamp(path))
                    consistent = false;
            }
            else
            {
                stamps[path] = getFileStamp(path);
            }
        }
        return new LuaMetadataDependencySnapshot(stamps, consistent, coveredByReadSnapshot);
    }

    internal bool AreDependenciesCurrent(LuaMetadataDependencySnapshot dependencies)
    {
        if (!dependencies.IsConsistent)
            return false;
        if (readScopeDepth != 0 && dependencies.CoveredByReadSnapshot)
            return true;
        foreach (KeyValuePair<string, LuaMetadataFileStamp> entry in dependencies.Stamps)
        {
            if (entry.Value != getFileStamp(entry.Key))
                return false;
        }
        return true;
    }

    internal void RestartDependencyTracking()
    {
        if (readScopeDepth == 0)
            return;
        readDependencyStamps = new Dictionary<string, LuaMetadataFileStamp>(StringComparer.OrdinalIgnoreCase);
        readDependenciesConsistent = true;
    }

    public LuaTypeMetadata? GetType(LuaTypeReference type, string? defaultModule = null)
    {
        LuaTypeReference resolvedType = type.WithDefaultModule(defaultModule);
        if (resolvedType.ModuleName is null)
            return null;
        string path = GetMetadataPath(resolvedType.ModuleName);
        IReadOnlyDictionary<string, LuaTypeMetadata> types = GetFileTypes(path, resolvedType.ModuleName);
        return types.TryGetValue(resolvedType.TypeName, out LuaTypeMetadata? metadata) ? metadata : null;
    }

    public LuaTypeMetadata? LoadScriptMixinMetadata(string scriptPath)
    {
        EnsureCurrent();
        string normalized = ScriptMixinPaths.Normalize(scriptPath);
        if (string.IsNullOrEmpty(normalized))
            return null;
        string metadataPath = ScriptMixinPaths.GetMetadataPath(ProjectPath, normalized);
        if (readScopeDepth != 0
            && scriptMixinCache.TryGetValue(normalized, out CachedScriptMixinMetadata? scopedCached))
        {
            trackReadDependency(metadataPath, scopedCached.Stamp);
            return scopedCached.Metadata;
        }
        LuaMetadataFileStamp stamp = getFileStamp(metadataPath);
        trackReadDependency(metadataPath, stamp);
        if (scriptMixinCache.TryGetValue(normalized, out CachedScriptMixinMetadata? cached))
        {
            if (cached.Stamp == stamp)
                return cached.Metadata;
            Clear();
            stamp = getFileStamp(metadataPath);
            trackReadDependency(metadataPath, stamp);
        }
        if (!stamp.Exists)
        {
            scriptMixinCache[normalized] = new CachedScriptMixinMetadata(metadataPath, stamp, null);
            return null;
        }

        LuaTypeMetadata metadata = LuaMetadataParser.ReadScriptMixin(
            metadataPath,
            ScriptMixinPaths.GetModuleName(normalized),
            ScriptMixinPaths.GetTypeName(normalized));
        scriptMixinCache[normalized] = new CachedScriptMixinMetadata(metadataPath, stamp, metadata);
        return metadata;
    }

    public IEnumerable<LuaTypeMetadata> ReadAllTypes()
    {
        foreach (string path in getMetadataFileSet().Paths)
        {
            foreach (LuaTypeMetadata metadata in GetFileTypes(path, getModuleName(path)).Values)
                yield return metadata;
        }
    }

    public void EnsureCurrent()
    {
        if (readScopeDepth != 0)
            return;
        foreach (KeyValuePair<string, CachedMetadataFile> entry in fileCache)
        {
            if (entry.Value.Stamp != getFileStamp(entry.Key))
            {
                Clear();
                return;
            }
        }
        foreach (CachedScriptMixinMetadata entry in scriptMixinCache.Values)
        {
            if (entry.Stamp != getFileStamp(entry.Path))
            {
                Clear();
                return;
            }
        }
        if (metadataFileSet is null || directoryStampsAreCurrent(metadataFileSet.DirectoryStamps))
            return;
        CachedMetadataFileSet currentFileSet = captureMetadataFileSet();
        if (!metadataFileSet.Paths.SequenceEqual(currentFileSet.Paths, StringComparer.OrdinalIgnoreCase))
        {
            Clear();
            return;
        }
        metadataFileSet = currentFileSet;
    }

    public void Clear()
    {
        fileCache.Clear();
        scriptMixinCache.Clear();
        metadataFileSet = null;
        revision++;
        Invalidated?.Invoke();
    }

    private void endRead()
    {
        if (readScopeDepth > 0)
            readScopeDepth--;
        if (readScopeDepth != 0)
            return;
        readDependencyStamps = null;
        readDependenciesConsistent = true;
    }

    private void trackReadDependency(string path, LuaMetadataFileStamp stamp)
    {
        if (readScopeDepth == 0 || readDependencyStamps is null)
            return;
        if (readDependencyStamps.TryGetValue(path, out LuaMetadataFileStamp existing))
        {
            if (existing != stamp)
                readDependenciesConsistent = false;
            return;
        }
        readDependencyStamps[path] = stamp;
    }

    private CachedMetadataFileSet getMetadataFileSet()
    {
        metadataFileSet ??= captureMetadataFileSet();
        return metadataFileSet;
    }

    private CachedMetadataFileSet captureMetadataFileSet()
    {
        List<string> directories = [scriptsPath];
        List<string> paths = [];
        if (Directory.Exists(scriptsPath))
        {
            directories.AddRange(Directory
                .EnumerateDirectories(scriptsPath, "*", SearchOption.AllDirectories)
                .OrderBy(path => path, StringComparer.OrdinalIgnoreCase));
            paths.AddRange(Directory
                .EnumerateFiles(scriptsPath, "*_meta.lua", SearchOption.AllDirectories)
                .Where(path => !isScriptMixinMetadata(Path.GetRelativePath(scriptsPath, path)))
                .OrderBy(path => path, StringComparer.OrdinalIgnoreCase));
        }
        Dictionary<string, DirectoryStamp> directoryStamps = new(StringComparer.OrdinalIgnoreCase);
        foreach (string directory in directories)
            directoryStamps[directory] = getDirectoryStamp(directory);
        return new CachedMetadataFileSet(paths.ToArray(), directoryStamps);
    }

    private static bool directoryStampsAreCurrent(
        IReadOnlyDictionary<string, DirectoryStamp> directoryStamps)
    {
        foreach (KeyValuePair<string, DirectoryStamp> entry in directoryStamps)
        {
            if (entry.Value != getDirectoryStamp(entry.Key))
                return false;
        }
        return true;
    }

    private string getModuleName(string path)
    {
        string relativePath = Path.GetRelativePath(scriptsPath, path);
        return relativePath[..^"_meta.lua".Length]
            .Replace(Path.DirectorySeparatorChar, '.')
            .Replace(Path.AltDirectorySeparatorChar, '.');
    }

    private static LuaMetadataFileStamp getFileStamp(string path)
    {
        FileInfo info = new(path);
        return info.Exists
            ? new LuaMetadataFileStamp(true, info.LastWriteTimeUtc, info.Length)
            : new LuaMetadataFileStamp(false, default, 0);
    }

    private static DirectoryStamp getDirectoryStamp(string path)
    {
        DirectoryInfo info = new(path);
        return info.Exists
            ? new DirectoryStamp(true, info.LastWriteTimeUtc)
            : new DirectoryStamp(false, default);
    }

    public string GetMetadataPath(string moduleName)
    {
        string relativePath = moduleName.Replace('.', Path.DirectorySeparatorChar) + "_meta.lua";
        return Path.Combine(scriptsPath, relativePath);
    }

    private static bool isScriptMixinMetadata(string relativePath)
    {
        string normalized = relativePath.Replace('\\', '/');
        return normalized.StartsWith("Mixins/", StringComparison.OrdinalIgnoreCase);
    }

    public IReadOnlyDictionary<string, LuaTypeMetadata> GetFileTypes(string path, string moduleName)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (readScopeDepth != 0
            && fileCache.TryGetValue(path, out CachedMetadataFile? scopedCached)
            && string.Equals(scopedCached.ModuleName, moduleName, StringComparison.Ordinal))
        {
            trackReadDependency(path, scopedCached.Stamp);
            return scopedCached.Types;
        }
        LuaMetadataFileStamp stamp = getFileStamp(path);
        trackReadDependency(path, stamp);
        if (fileCache.TryGetValue(path, out CachedMetadataFile? cached))
        {
            if (cached.Stamp == stamp && string.Equals(cached.ModuleName, moduleName, StringComparison.Ordinal))
                return cached.Types;
            Clear();
            stamp = getFileStamp(path);
            trackReadDependency(path, stamp);
        }

        IReadOnlyDictionary<string, LuaTypeMetadata> types;
        if (!stamp.Exists)
        {
            types = new Dictionary<string, LuaTypeMetadata>(StringComparer.Ordinal);
        }
        else
        {
            try
            {
                types = LuaMetadataParser.ReadFile(path, moduleName);
            }
            catch (InterpreterException) when (!strictReads)
            {
                types = new Dictionary<string, LuaTypeMetadata>(StringComparer.Ordinal);
            }
            catch (InvalidDataException) when (!strictReads)
            {
                types = new Dictionary<string, LuaTypeMetadata>(StringComparer.Ordinal);
            }
            catch (IOException) when (!strictReads)
            {
                types = new Dictionary<string, LuaTypeMetadata>(StringComparer.Ordinal);
            }
            catch (UnauthorizedAccessException) when (!strictReads)
            {
                types = new Dictionary<string, LuaTypeMetadata>(StringComparer.Ordinal);
            }
        }

        fileCache[path] = new CachedMetadataFile(stamp, moduleName, types);
        return types;
    }

    private readonly record struct DirectoryStamp(bool Exists, DateTime ModifiedAt);

    private sealed record CachedMetadataFile(
        LuaMetadataFileStamp Stamp,
        string ModuleName,
        IReadOnlyDictionary<string, LuaTypeMetadata> Types);

    private sealed record CachedScriptMixinMetadata(
        string Path,
        LuaMetadataFileStamp Stamp,
        LuaTypeMetadata? Metadata);

    private sealed record CachedMetadataFileSet(
        IReadOnlyList<string> Paths,
        IReadOnlyDictionary<string, DirectoryStamp> DirectoryStamps);

    private sealed class MetadataReadScope(LuaMetadataFileCache owner) : IDisposable
    {
        private LuaMetadataFileCache? service = owner;

        public void Dispose()
        {
            LuaMetadataFileCache? current = service;
            if (current is null)
                return;
            service = null;
            current.endRead();
        }
    }
}
