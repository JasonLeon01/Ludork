using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using System;
using System.Reflection;
using System.Runtime.Loader;

namespace Ludork.Services.Plugins;

internal sealed class PluginAssemblyLoadContext : AssemblyLoadContext
{
    private static readonly Assembly ContractAssembly = typeof(IEditorPlugin).Assembly;
    private static readonly string ContractAssemblyName =
        ContractAssembly.GetName().Name ?? "Ludork.Plugin.Abstractions";
    private static readonly Assembly AvaloniaContractAssembly =
        typeof(IAvaloniaPluginUserInterface).Assembly;
    private static readonly string AvaloniaContractAssemblyName =
        AvaloniaContractAssembly.GetName().Name ?? "Ludork.Plugin.Avalonia";

    public PluginAssemblyLoadContext(string name) : base(name, true)
    {
    }

    protected override Assembly? Load(AssemblyName assemblyName)
    {
        if (string.Equals(assemblyName.Name, ContractAssemblyName, StringComparison.Ordinal))
            return ContractAssembly;
        if (string.Equals(
                assemblyName.Name,
                AvaloniaContractAssemblyName,
                StringComparison.Ordinal))
        {
            return AvaloniaContractAssembly;
        }
        if (assemblyName.Name is not null
            && assemblyName.Name.StartsWith("Avalonia", StringComparison.Ordinal))
        {
            return loadSharedAvaloniaAssembly(assemblyName);
        }
        return null;
    }

    private static Assembly loadSharedAvaloniaAssembly(AssemblyName assemblyName)
    {
        foreach (Assembly assembly in Default.Assemblies)
        {
            if (string.Equals(
                    assembly.GetName().Name,
                    assemblyName.Name,
                    StringComparison.Ordinal))
            {
                return assembly;
            }
        }
        return Default.LoadFromAssemblyName(assemblyName);
    }

}
