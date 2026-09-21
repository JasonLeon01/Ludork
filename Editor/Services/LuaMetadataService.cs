using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace Ludork.Services;

public sealed class LuaMetadataService
{
    private readonly LuaMetadataFileCache files;
    private readonly LuaMetadataHierarchy hierarchy;
    private readonly LuaMetadataCatalog catalog;

    public LuaMetadataService(string projectPath, bool strictReads = false, CancellationToken cancellationToken = default)
    {
        files = new LuaMetadataFileCache(projectPath, strictReads, cancellationToken);
        hierarchy = new LuaMetadataHierarchy(files);
        catalog = new LuaMetadataCatalog(files, hierarchy);
    }

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
        ensureCacheCurrent();
        return files.BeginRead();
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
        if (sourceType.IsAssignableTo(targetType))
            return true;
        using IDisposable read = BeginRead();
        return sourceType.IsAssignableTo(targetType, hierarchy.IsDerivedType);
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
        ensureCacheCurrent();
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

    private void ensureCacheCurrent()
    {
        if (files.IsReading)
            return;
        if (!hierarchy.IsCurrent)
        {
            files.Clear();
            return;
        }
        files.EnsureCurrent();
    }
}
