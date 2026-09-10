using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;

namespace Ludork.Services.Plugins;

internal sealed class PluginRuntimeEntry : IDisposable
{
    private readonly object diagnosticLock = new();
    private readonly List<string> diagnostics = [];

    public PluginRuntimeEntry(
        int registryIndex,
        PluginRegistryEntry registryEntry,
        string sourcePath)
    {
        RegistryIndex = registryIndex;
        RegistryEntry = registryEntry;
        SourcePath = sourcePath;
        Name = registryEntry.Id;
    }

    public int RegistryIndex { get; }

    public PluginRegistryEntry RegistryEntry { get; }

    public string SourcePath { get; }

    public string Name { get; set; }

    public string Version { get; set; } = string.Empty;

    public PluginRuntimeStatus Status { get; set; } = PluginRuntimeStatus.ManifestInvalid;

    public PluginAssemblyLoadContext? LoadContext { get; set; }

    public IEditorPlugin? Instance { get; set; }

    public IReadOnlyList<PluginMenuCommand> MenuCommands { get; set; } = [];

    public IReadOnlyList<PluginMapContextMenuCommand> MapContextMenuCommands { get; set; } = [];

    public IReadOnlyList<ITextHintProvider> TextHintProviders { get; set; } = [];

    public IReadOnlyList<IProjectOperationHook> BeforeRunHooks { get; set; } = [];

    public IReadOnlyList<IProjectOperationHook> BeforePackHooks { get; set; } = [];

    public string Diagnostic
    {
        get
        {
            lock (diagnosticLock)
                return string.Join(Environment.NewLine, diagnostics);
        }
    }

    public void AddDiagnostic(string diagnostic)
    {
        if (string.IsNullOrWhiteSpace(diagnostic))
            return;
        lock (diagnosticLock)
        {
            if (!diagnostics.Contains(diagnostic))
                diagnostics.Add(diagnostic);
        }
    }

    public PluginRuntimeInfo ToInfo()
    {
        return new PluginRuntimeInfo(
            RegistryEntry.Id,
            Name,
            Version,
            RegistryEntry.Directory,
            SourcePath,
            Status,
            Diagnostic);
    }

    public void Dispose()
    {
        Instance = null;
        MenuCommands = [];
        MapContextMenuCommands = [];
        TextHintProviders = [];
        BeforeRunHooks = [];
        BeforePackHooks = [];
        LoadContext?.Unload();
        LoadContext = null;
    }
}
