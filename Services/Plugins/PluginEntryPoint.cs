using Ludork.Plugin.Abstractions;
using System;
using System.IO;
using System.Reflection;

namespace Ludork.Services.Plugins;

internal static class PluginEntryPoint
{
    public static ConstructorInfo GetConstructor(
        PluginPackage package,
        Assembly assembly)
    {
        Type? entryType = assembly.GetType(
            package.Manifest.EntryType,
            false,
            false);
        if (entryType is null)
        {
            throw new InvalidDataException(
                $"Plugin entry type was not found: {package.Manifest.EntryType}");
        }
        if (!entryType.IsPublic && !entryType.IsNestedPublic)
            throw new InvalidDataException("Plugin entry type must be public.");
        if (entryType.IsAbstract || !typeof(IEditorPlugin).IsAssignableFrom(entryType))
        {
            throw new InvalidDataException(
                $"Plugin entry type must implement {typeof(IEditorPlugin).FullName}.");
        }
        ConstructorInfo? constructor = entryType.GetConstructor(Type.EmptyTypes);
        if (constructor is null)
        {
            throw new InvalidDataException(
                "Plugin entry type needs a public parameterless constructor.");
        }
        return constructor;
    }
}
