using Ludork.Models;
using System;
using System.Collections.Concurrent;
using System.Threading;

namespace Ludork.Services;

internal sealed class LuaValidationReadCache(
    string projectPath, ProjectEnumCatalog projectEnums, CancellationToken cancellationToken = default)
{
    private readonly ConcurrentDictionary<(Type Type, object Key), Lazy<object>> entries = new();

    internal LuaMetadataService CreateMetadata(bool strictReads = false)
        => new(projectPath, strictReads, cancellationToken, CreateEnums(), this);

    internal LuaEnumService CreateEnums() => new(projectPath, () => projectEnums, this);

    internal T GetOrAdd<T>(object key, Func<T> create)
    {
        cancellationToken.ThrowIfCancellationRequested();
        T result = (T)entries.GetOrAdd((typeof(T), key), _ => new Lazy<object>(
            () => create()!, LazyThreadSafetyMode.ExecutionAndPublication)).Value;
        cancellationToken.ThrowIfCancellationRequested();
        return result;
    }

    internal LuaEnumDefinition ReadEnumModule(string moduleName)
        => GetOrAdd(moduleName, () => new LuaEnumService(projectPath, () => projectEnums).ReadModule(moduleName));

    internal LuaStubTypeHierarchy GetStubHierarchy()
        => GetOrAdd("stub", () => new LuaStubTypeHierarchy(
            System.IO.Path.Combine(projectPath, "Scripts", "stub"), cancellationToken));
}
