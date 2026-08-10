using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;

namespace Ludork.Services.Plugins;

internal sealed class PluginRegistrar : IPluginRegistrar
{
    private readonly List<PluginMenuCommand> menuCommands = [];
    private readonly List<PluginMapContextMenuCommand> mapContextMenuCommands = [];
    private readonly List<ITextHintProvider> textHintProviders = [];
    private readonly List<IProjectOperationHook> beforeRunHooks = [];
    private readonly List<IProjectOperationHook> beforePackHooks = [];

    public PluginRegistrar(
        string pluginDirectory,
        string pluginDataDirectory,
        string editorLanguage)
    {
        PluginDirectory = pluginDirectory;
        PluginDataDirectory = pluginDataDirectory;
        EditorLanguage = editorLanguage;
    }

    public string PluginDirectory { get; }

    public string PluginDataDirectory { get; }

    public string EditorLanguage { get; }

    public IReadOnlyList<PluginMenuCommand> MenuCommands => menuCommands;

    public IReadOnlyList<PluginMapContextMenuCommand> MapContextMenuCommands =>
        mapContextMenuCommands;

    public IReadOnlyList<ITextHintProvider> TextHintProviders => textHintProviders;

    public IReadOnlyList<IProjectOperationHook> BeforeRunHooks => beforeRunHooks;

    public IReadOnlyList<IProjectOperationHook> BeforePackHooks => beforePackHooks;

    public void RegisterMenuCommand(PluginMenuCommand command)
    {
        ArgumentNullException.ThrowIfNull(command);
        validateCommand(command.Id, command.Label, command.Handler);
        if (!Enum.IsDefined(command.Location))
            throw new InvalidOperationException($"Plugin menu command '{command.Id}' has an invalid location.");
        ensureCommandIdAvailable(command.Id);
        menuCommands.Add(command);
    }

    public void RegisterMapContextMenuCommand(PluginMapContextMenuCommand command)
    {
        ArgumentNullException.ThrowIfNull(command);
        validateCommand(command.Id, command.Label, command.Handler);
        ensureCommandIdAvailable(command.Id);
        mapContextMenuCommands.Add(command);
    }

    public void RegisterTextHintProvider(ITextHintProvider provider)
    {
        ArgumentNullException.ThrowIfNull(provider);
        textHintProviders.Add(provider);
    }

    public void RegisterBeforeRunHook(IProjectOperationHook hook)
    {
        ArgumentNullException.ThrowIfNull(hook);
        beforeRunHooks.Add(hook);
    }

    public void RegisterBeforePackHook(IProjectOperationHook hook)
    {
        ArgumentNullException.ThrowIfNull(hook);
        beforePackHooks.Add(hook);
    }

    private void ensureCommandIdAvailable(string commandId)
    {
        foreach (PluginMenuCommand existing in menuCommands)
        {
            if (string.Equals(existing.Id, commandId, StringComparison.Ordinal))
                throw new InvalidOperationException($"Duplicate plugin command ID: {commandId}");
        }
        foreach (PluginMapContextMenuCommand existing in mapContextMenuCommands)
        {
            if (string.Equals(existing.Id, commandId, StringComparison.Ordinal))
                throw new InvalidOperationException($"Duplicate plugin command ID: {commandId}");
        }
    }

    private static void validateCommand(
        string id,
        string label,
        Delegate handler)
    {
        if (string.IsNullOrWhiteSpace(id))
            throw new InvalidOperationException("Plugin command ID is required.");
        if (string.IsNullOrWhiteSpace(label))
            throw new InvalidOperationException($"Plugin command '{id}' has no label.");
        if (handler is null)
            throw new InvalidOperationException($"Plugin command '{id}' has no handler.");
    }
}
