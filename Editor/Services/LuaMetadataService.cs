using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;

namespace Ludork.Services;

public sealed class LuaMetadataService
{
    private readonly LuaMetadataFileCache files;
    private readonly LuaMetadataHierarchy hierarchy;
    private readonly LuaMetadataCatalog catalog;

    public LuaMetadataService(string projectPath, bool strictReads = false, CancellationToken cancellationToken = default, LuaEnumService? enums = null)
        : this(projectPath, strictReads, cancellationToken, enums, null)
    {
    }

    internal LuaMetadataService(string projectPath, bool strictReads, CancellationToken cancellationToken,
        LuaEnumService? enums, LuaValidationReadCache? readCache, LuaReadDependencyCapture? dependencyCapture = null)
    {
        Enums = enums ?? new LuaEnumService(projectPath);
        files = new LuaMetadataFileCache(projectPath, Enums, strictReads, cancellationToken, readCache, dependencyCapture);
        hierarchy = new LuaMetadataHierarchy(files, readCache);
        catalog = new LuaMetadataCatalog(files, hierarchy);
    }

    public LuaEnumService Enums { get; }

    public string ProjectPath => files.ProjectPath;
    internal long CacheRevision => files.CacheRevision;

    public long Revision
    {
        get
        {
            ensureCacheCurrent();
            return files.CacheRevision;
        }
    }

    public IDisposable BeginRead()
    {
        ensureHierarchyCurrent();
        return files.BeginRead();
    }

    internal IDisposable BeginReferenceRead()
    {
        ensureHierarchyCurrent();
        return files.BeginRead(requireValid: true);
    }

    internal IDisposable BeginPreparedRead()
    {
        IDisposable metadataRead = files.BeginPreparedRead();
        return new PreparedReadScope(metadataRead, Enums.BeginPreparedRead());
    }

    internal LuaMetadataDependencySnapshot CaptureDependencies(
        IEnumerable<LuaTypeReference> types,
        IEnumerable<string> scriptMixins)
    {
        return files.CaptureDependencies(types, scriptMixins);
    }

    internal bool AreDependenciesCurrent(LuaMetadataDependencySnapshot dependencies)
    {
        return files.AreDependenciesCurrent(dependencies);
    }

    internal void RestartDependencyTracking()
    {
        files.RestartDependencyTracking();
    }

    public bool IsTypeAssignable(string source, string target)
    {
        LuaMetadataType sourceType = LuaMetadataType.Parse(source);
        LuaMetadataType targetType = LuaMetadataType.Parse(target);
        if (sourceType.IsAssignableTo(targetType, resolveEnum: Enums.Read))
            return true;
        using IDisposable read = BeginRead();
        return sourceType.IsAssignableTo(targetType, hierarchy.IsDerivedType, Enums.Read);
    }

    public LuaMetadataTypeSnapshot CaptureTypeSnapshot(CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        using IDisposable enumRead = Enums.BeginSnapshotRead(cancellationToken);
        using IDisposable metadataRead = BeginReferenceRead();
        List<LuaTypeMetadata> metadataTypes = [];
        foreach (LuaTypeMetadata metadata in files.ReadAllTypes())
        {
            cancellationToken.ThrowIfCancellationRequested();
            metadataTypes.Add(metadata);
        }
        string mixinsRoot = ScriptMixinPaths.GetMixinsRoot(ProjectPath);
        if (Directory.Exists(mixinsRoot))
        {
            foreach (string path in Directory.EnumerateFiles(mixinsRoot, "*_meta.lua", SearchOption.AllDirectories))
            {
                cancellationToken.ThrowIfCancellationRequested();
                string scriptPath = Path.GetRelativePath(mixinsRoot, path)[..^"_meta.lua".Length] + ".lua";
                if (files.LoadScriptMixinMetadata(scriptPath) is LuaTypeMetadata metadata)
                    metadataTypes.Add(metadata);
            }
        }
        Dictionary<string, LuaMetadataType> types = new(StringComparer.Ordinal);
        HashSet<string> enumModules = new(Enums.EnumerateModules(), StringComparer.Ordinal);
        foreach (LuaTypeMetadata metadata in metadataTypes)
        {
            cancellationToken.ThrowIfCancellationRequested();
            addType(metadata.Type.Schema);
            foreach (BlueprintFieldMetadata field in metadata.Fields.Values)
                addType(field.Type.Schema);
            foreach (LuaNodeMemberMetadata member in metadata.Members.Values)
            {
                foreach (LuaNodeParameterMetadata parameter in member.Parameters)
                    addType(parameter.Type.Schema);
                foreach (LuaNodeReturnMetadata returnValue in member.Returns)
                    addType(returnValue.Type.Schema);
            }
        }
        Dictionary<string, LuaEnumDefinition> enums = new(StringComparer.Ordinal);
        foreach (string module in enumModules)
        {
            foreach (string valueType in new[] { string.Empty, "string", "bool", "int", "float" })
            {
                cancellationToken.ThrowIfCancellationRequested();
                LuaMetadataType schema = LuaMetadataType.Parse(valueType.Length == 0
                    ? $"Enum[{module}]" : $"Enum[{module}, {valueType}]");
                string name = schema.ToString();
                types[name] = schema;
                LuaEnumDefinition definition = Enums.Read(schema);
                enums[name] = definition with { Options = [] };
            }
        }
        IReadOnlyDictionary<string, IReadOnlySet<string>> ancestors = hierarchy.CaptureAncestors(metadataTypes, cancellationToken);
        foreach (KeyValuePair<string, IReadOnlySet<string>> type in ancestors)
        {
            addType(LuaMetadataType.Parse(type.Key));
            foreach (string parent in type.Value)
                addType(LuaMetadataType.Parse(parent));
        }
        catalog.EnumerateTypes();
        catalog.EnumerateNodeMembers(LuaNodeMemberKind.Function);
        cancellationToken.ThrowIfCancellationRequested();
        return new LuaMetadataTypeSnapshot(types, enums, ancestors, cancellationToken);

        void addType(LuaMetadataType type)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (!types.TryAdd(type.ToString(), type))
                return;
            if (type.Kind == LuaMetadataTypeKind.Enum)
                enumModules.Add(type.Name);
            foreach (LuaMetadataType argument in type.Arguments)
                addType(argument);
        }
    }

    public LuaTypeMetadata? GetType(string qualifiedTypeName)
    {
        return GetType(LuaTypeReference.Parse(qualifiedTypeName));
    }

    public LuaTypeMetadata? GetType(LuaTypeReference type, string? defaultModule = null)
    {
        ensureCacheCurrent();
        return files.GetType(type, defaultModule);
    }

    public LuaTypeMetadata? GetRuntimeClassType(string classReference)
    {
        ensureCacheCurrent();
        LuaTypeReference reference = LuaTypeReference.Parse(classReference);
        IReadOnlyDictionary<string, LuaTypeMetadata> moduleTypes = files.GetFileTypes(
            files.GetMetadataPath(reference.QualifiedName),
            reference.QualifiedName);
        return moduleTypes.Values.FirstOrDefault(type => type.ModuleReturn) ?? files.GetType(reference);
    }

    public string GetRuntimeClassReference(LuaTypeReference type)
    {
        LuaTypeMetadata? metadata = GetType(type);
        return metadata?.ModuleReturn == true && metadata.Type.ModuleName is string moduleName
            ? moduleName
            : type.QualifiedName;
    }

    public LuaTypeMetadata? LoadScriptMixinMetadata(string scriptPath)
    {
        ensureHierarchyCurrent();
        return files.LoadScriptMixinMetadata(scriptPath);
    }

    public IReadOnlyList<LuaTypeMetadata> ResolveMro(string qualifiedTypeName)
    {
        return ResolveMro(LuaTypeReference.Parse(qualifiedTypeName));
    }

    public IReadOnlyList<LuaTypeMetadata> ResolveMro(LuaTypeReference type)
    {
        ensureCacheCurrent();
        return hierarchy.ResolveMro(type);
    }

    public IReadOnlyList<LuaNodeMemberMetadata> GetNodeMembers(
        string qualifiedTypeName,
        LuaNodeMemberKind? kind = null,
        bool includeInherited = true
    )
    {
        return GetNodeMembers(LuaTypeReference.Parse(qualifiedTypeName), kind, includeInherited);
    }

    public IReadOnlyList<LuaNodeMemberMetadata> GetNodeMembers(
        LuaTypeReference type,
        LuaNodeMemberKind? kind = null,
        bool includeInherited = true)
    {
        ensureCacheCurrent();
        return catalog.GetNodeMembers(type, kind, includeInherited);
    }

    public IReadOnlyList<LuaNodeMemberMetadata> EnumerateNodeMembers(LuaNodeMemberKind? kind = null)
    {
        ensureCacheCurrent();
        return catalog.EnumerateNodeMembers(kind);
    }

    public IReadOnlyList<LuaTypeMetadata> EnumerateTypes()
    {
        ensureCacheCurrent();
        return catalog.EnumerateTypes();
    }

    public void ClearCache()
    {
        files.Clear();
    }

    internal void AdoptPreparedState(LuaMetadataService prepared)
    {
        ArgumentNullException.ThrowIfNull(prepared);
        if (ReferenceEquals(this, prepared))
            throw new ArgumentException("Prepared metadata must come from a separate service.", nameof(prepared));
        if (!string.Equals(ProjectPath, prepared.ProjectPath, StringComparison.Ordinal))
            throw new ArgumentException("Prepared metadata must belong to the same project.", nameof(prepared));
        files.AdoptPreparedState(prepared.files);
        hierarchy.AdoptPreparedState(prepared.hierarchy);
        catalog.AdoptPreparedState(prepared.catalog);
        Enums.AdoptPreparedState(prepared.Enums);
    }

    private void ensureCacheCurrent()
    {
        if (files.IsReading)
            return;
        ensureHierarchyCurrent();
        files.EnsureCurrent();
    }

    private void ensureHierarchyCurrent()
    {
        if (files.IsReading)
            return;
        if (!hierarchy.IsCurrent)
            files.Clear();
    }

    private sealed class PreparedReadScope(IDisposable metadataRead, IDisposable enumRead) : IDisposable
    {
        public void Dispose()
        {
            enumRead.Dispose();
            metadataRead.Dispose();
        }
    }
}
