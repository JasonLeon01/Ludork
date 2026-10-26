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
    private readonly LuaEnumService enums;
    private readonly bool strictReads;
    private readonly CancellationToken cancellationToken;
    private readonly LuaValidationReadCache? readCache;
    private readonly LuaReadDependencyCapture? dependencyCapture;
    private readonly Dictionary<string, CachedMetadataFile> fileCache = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, CachedScriptMixinMetadata> scriptMixinCache = new(StringComparer.OrdinalIgnoreCase);
    private CachedMetadataFileSet? metadataFileSet;
    private Dictionary<string, LuaMetadataFileStamp>? readDependencyStamps;
    private bool readDependenciesConsistent = true;
    private int readScopeDepth;
    private int strictReadDepth;
    private int preparedReadDepth;
    private long revision;

    public LuaMetadataFileCache(string projectPath, LuaEnumService enums, bool strictReads = false,
        CancellationToken cancellationToken = default, LuaValidationReadCache? readCache = null,
        LuaReadDependencyCapture? dependencyCapture = null)
    {
        this.enums = enums;
        this.strictReads = strictReads;
        this.cancellationToken = cancellationToken;
        this.readCache = readCache;
        this.dependencyCapture = dependencyCapture;
        ProjectPath = Path.GetFullPath(projectPath);
        scriptsPath = Path.Combine(ProjectPath, "Scripts");
    }

    public string ProjectPath { get; }

    public long CacheRevision => revision;
    public bool IsReading => readScopeDepth != 0;
    public event Action? Invalidated;

    public IDisposable BeginRead(bool requireValid = false)
    {
        if (readScopeDepth == 0)
        {
            EnsureCurrent();
            readDependencyStamps = new Dictionary<string, LuaMetadataFileStamp>(StringComparer.OrdinalIgnoreCase);
            readDependenciesConsistent = true;
        }
        if (requireValid)
        {
            foreach (KeyValuePair<string, CachedMetadataFile> entry in fileCache)
                if (entry.Value.Error is Exception error)
                    throw new InvalidDataException($"Could not read metadata: {entry.Key}: {error.Message}", error);
            strictReadDepth++;
        }
        readScopeDepth++;
        return new MetadataReadScope(this, requireValid);
    }

    public IDisposable BeginPreparedRead()
    {
        if (metadataFileSet is null)
            throw new InvalidOperationException("Metadata must be captured before a prepared read.");
        if (readScopeDepth == 0)
        {
            readDependencyStamps = new Dictionary<string, LuaMetadataFileStamp>(StringComparer.OrdinalIgnoreCase);
            readDependenciesConsistent = true;
        }
        preparedReadDepth++;
        readScopeDepth++;
        return new MetadataReadScope(this, false, true);
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
            bool cachedByMixin = scriptMixinCache.TryGetValue(path, out CachedScriptMixinMetadata? cachedMixin);
            if (!cachedByFile && !cachedByMixin && preparedReadDepth == 0)
                coveredByReadSnapshot = false;
            if (readDependencyStamps?.TryGetValue(path, out LuaMetadataFileStamp readStamp) == true)
            {
                stamps[path] = readStamp;
                if (preparedReadDepth == 0 && readStamp != getFileStamp(path))
                    consistent = false;
                continue;
            }
            if (fileCache.TryGetValue(path, out CachedMetadataFile? cachedFile))
            {
                stamps[path] = cachedFile.Stamp;
                if (preparedReadDepth == 0 && cachedFile.Stamp != getFileStamp(path))
                    consistent = false;
                continue;
            }
            if (cachedMixin is not null)
            {
                stamps[path] = cachedMixin.Stamp;
                if (preparedReadDepth == 0 && cachedMixin.Stamp != getFileStamp(path))
                    consistent = false;
            }
            else
            {
                stamps[path] = preparedReadDepth == 0 ? getFileStamp(path) : default;
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
            && scriptMixinCache.TryGetValue(metadataPath, out CachedScriptMixinMetadata? scopedCached))
        {
            trackReadDependency(metadataPath, scopedCached.Stamp);
            return scopedCached.Metadata;
        }
        if (preparedReadDepth != 0)
        {
            scriptMixinCache[metadataPath] = new CachedScriptMixinMetadata(metadataPath, default, null);
            return null;
        }
        LuaMetadataFileStamp stamp = getFileStamp(metadataPath);
        trackReadDependency(metadataPath, stamp);
        if (scriptMixinCache.TryGetValue(metadataPath, out CachedScriptMixinMetadata? cached))
        {
            if (cached.Stamp == stamp)
                return cached.Metadata;
            Clear();
            stamp = getFileStamp(metadataPath);
            trackReadDependency(metadataPath, stamp);
        }
        if (!stamp.Exists)
        {
            scriptMixinCache[metadataPath] = new CachedScriptMixinMetadata(metadataPath, stamp, null);
            return null;
        }

        string moduleName = ScriptMixinPaths.GetModuleName(normalized);
        string typeName = ScriptMixinPaths.GetTypeName(normalized);
        LuaTypeMetadata metadata = readCache is null
            ? LuaMetadataParser.ReadScriptMixin(metadataPath, moduleName, typeName, enums.Read)
            : readCache.GetOrAdd(new MixinParseKey(metadataPath, moduleName, typeName, stamp),
                () => LuaMetadataParser.ReadScriptMixin(metadataPath, moduleName, typeName, readCache.CreateEnums().Read));
        scriptMixinCache[metadataPath] = new CachedScriptMixinMetadata(metadataPath, stamp, metadata);
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

    public void AdoptPreparedState(LuaMetadataFileCache prepared)
    {
        if (IsReading || prepared.IsReading)
            throw new InvalidOperationException("Metadata state cannot change during a read.");
        Clear();
        foreach (KeyValuePair<string, CachedMetadataFile> entry in prepared.fileCache)
            fileCache.Add(entry.Key, entry.Value);
        foreach (KeyValuePair<string, CachedScriptMixinMetadata> entry in prepared.scriptMixinCache)
            scriptMixinCache.Add(entry.Key, entry.Value);
        metadataFileSet = prepared.metadataFileSet;
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
        dependencyCapture?.Record(path, stamp);
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
        metadataFileSet ??= readCache is null ? captureMetadataFileSet()
            : readCache.GetOrAdd(scriptsPath, captureMetadataFileSet);
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

    private static LuaMetadataFileStamp getFileStamp(string path) => LuaMetadataFileStamp.Read(path);

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
            return readCachedTypes(path, scopedCached);
        }
        if (preparedReadDepth != 0)
        {
            IReadOnlyDictionary<string, LuaTypeMetadata> missing = new Dictionary<string, LuaTypeMetadata>(StringComparer.Ordinal);
            fileCache[path] = new CachedMetadataFile(default, moduleName, missing, null);
            return missing;
        }
        LuaMetadataFileStamp stamp = getFileStamp(path);
        trackReadDependency(path, stamp);
        if (fileCache.TryGetValue(path, out CachedMetadataFile? cached))
        {
            if (cached.Stamp == stamp && string.Equals(cached.ModuleName, moduleName, StringComparison.Ordinal))
                return readCachedTypes(path, cached);
            Clear();
            stamp = getFileStamp(path);
            trackReadDependency(path, stamp);
        }

        IReadOnlyDictionary<string, LuaTypeMetadata> types;
        Exception? readError = null;
        if (!stamp.Exists)
        {
            types = new Dictionary<string, LuaTypeMetadata>(StringComparer.Ordinal);
        }
        else
        {
            try
            {
                types = readCache is null
                    ? LuaMetadataParser.ReadFile(path, moduleName, enums.Read)
                    : readCache.GetOrAdd(new MetadataParseKey(path, moduleName, stamp),
                        () => LuaMetadataParser.ReadFile(path, moduleName, readCache.CreateEnums().Read));
            }
            catch (Exception exception) when (exception is InterpreterException or InvalidDataException or IOException or UnauthorizedAccessException)
            {
                if (strictReads || strictReadDepth != 0)
                    throw new InvalidDataException($"Could not read metadata: {path}: {exception.Message}", exception);
                readError = exception;
                types = new Dictionary<string, LuaTypeMetadata>(StringComparer.Ordinal);
            }
        }

        fileCache[path] = new CachedMetadataFile(stamp, moduleName, types, readError);
        return types;
    }

    private IReadOnlyDictionary<string, LuaTypeMetadata> readCachedTypes(string path, CachedMetadataFile cached)
    {
        if ((strictReads || strictReadDepth != 0) && cached.Error is Exception error)
            throw new InvalidDataException($"Could not read metadata: {path}: {error.Message}", error);
        return cached.Types;
    }

    private readonly record struct DirectoryStamp(bool Exists, DateTime ModifiedAt);

    private sealed record MetadataParseKey(string Path, string ModuleName, LuaMetadataFileStamp Stamp);

    private sealed record MixinParseKey(string Path, string ModuleName, string TypeName, LuaMetadataFileStamp Stamp);

    private sealed record CachedMetadataFile(
        LuaMetadataFileStamp Stamp,
        string ModuleName,
        IReadOnlyDictionary<string, LuaTypeMetadata> Types, Exception? Error);

    private sealed record CachedScriptMixinMetadata(
        string Path,
        LuaMetadataFileStamp Stamp,
        LuaTypeMetadata? Metadata);

    private sealed record CachedMetadataFileSet(
        IReadOnlyList<string> Paths,
        IReadOnlyDictionary<string, DirectoryStamp> DirectoryStamps);

    private sealed class MetadataReadScope(LuaMetadataFileCache owner, bool requireValid, bool prepared = false) : IDisposable
    {
        private LuaMetadataFileCache? service = owner;

        public void Dispose()
        {
            LuaMetadataFileCache? current = service;
            if (current is null)
                return;
            service = null;
            if (requireValid)
                current.strictReadDepth--;
            if (prepared)
                current.preparedReadDepth--;
            current.endRead();
        }
    }
}
