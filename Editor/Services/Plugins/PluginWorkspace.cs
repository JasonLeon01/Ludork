using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.Plugins;

public sealed class PluginWorkspace : IEditorPluginRuntime, IDisposable
{
    private readonly string editorLanguage;
    private readonly Dictionary<string, PluginHost> projects = new(StringComparer.Ordinal);
    private readonly SemaphoreSlim initializationGate = new(1, 1);
    private readonly object projectLock = new();

    public PluginWorkspace(string editorLanguage, PluginEnvironment? environment = null)
    {
        this.editorLanguage = editorLanguage;
        GlobalHost = new PluginHost(editorLanguage, environment);
        GlobalHost.Management.IsIdReserved = isProjectIdReserved;
    }

    public PluginHost GlobalHost { get; }

    public Task InitializeAsync(CancellationToken cancellationToken = default)
    {
        return GlobalHost.InitializeAsync(cancellationToken);
    }

    public async Task InitializeProjectAsync(string projectPath, CancellationToken cancellationToken = default)
    {
        string key = PluginPaths.NormalizeProjectPath(projectPath);
        await initializationGate.WaitAsync(cancellationToken);
        try
        {
            if (GetProjectHost(key) is not null)
                return;
            await GlobalHost.InitializeAsync(cancellationToken);
            PluginHost host = new(editorLanguage, PluginPaths.ForProject(GlobalHost.Environment, key))
            {
                GlobalHost = GlobalHost,
            };
            host.Management.IsIdReserved = id =>
                GlobalHost.Plugins.Any(value => value.Id == id)
                || GlobalHost.Management.Registry.Plugins.Any(value => value.Id == id);
            try
            {
                await host.InitializeAsync(cancellationToken);
                lock (projectLock)
                    projects.Add(key, host);
            }
            catch
            {
                host.Dispose();
                throw;
            }
        }
        finally
        {
            initializationGate.Release();
        }
    }

    public PluginHost? GetProjectHost(string? projectPath)
    {
        if (string.IsNullOrWhiteSpace(projectPath))
            return null;
        lock (projectLock)
            return projects.GetValueOrDefault(PluginPaths.NormalizeProjectPath(projectPath));
    }

    public PluginManagementService GetManagement(string? projectPath)
    {
        if (string.IsNullOrWhiteSpace(projectPath))
            return GlobalHost.Management;
        return (GetProjectHost(projectPath)
            ?? throw new InvalidOperationException("Project plugins have not been initialized.")).Management;
    }

    public IReadOnlyList<RegisteredPluginMenuCommand> GetMenuCommands(string? projectPath)
    {
        return getHosts(projectPath).SelectMany(host => host.MenuCommands)
            .OrderBy(value => value.Command.Location).ThenBy(value => value.Command.Order).ToArray();
    }

    public IReadOnlyList<RegisteredPluginMapContextMenuCommand> GetMapContextMenuCommands(string projectPath)
    {
        return getHosts(projectPath).SelectMany(host => host.MapContextMenuCommands)
            .OrderBy(value => value.Command.Order).ToArray();
    }

    public string? ResolveTextHint(TextHintContext context)
    {
        foreach (PluginHost host in getHosts(context.ProjectPath))
        {
            string? result = host.ResolveTextHint(context);
            if (!string.IsNullOrWhiteSpace(result))
                return result;
        }
        return null;
    }

    public IReadOnlyList<ProjectExportParticipant> GetExportParticipants(string projectPath)
    {
        return getHosts(projectPath).SelectMany(host => host.GetExportParticipants(projectPath)).ToArray();
    }

    public async Task<PluginResult> ExecuteBeforeProjectOperationAsync(ProjectOperationContext context)
    {
        foreach (PluginHost host in getHosts(context.ProjectPath))
        {
            PluginResult result = await host.ExecuteBeforeProjectOperationAsync(context);
            if (!result.Success)
                return result;
        }
        return PluginResult.Completed();
    }

    public void Dispose()
    {
        lock (projectLock)
        {
            foreach (PluginHost host in projects.Values)
                host.Dispose();
            projects.Clear();
        }
        GlobalHost.Dispose();
        initializationGate.Dispose();
    }

    private PluginHost[] getHosts(string? projectPath)
    {
        PluginHost? project = GetProjectHost(projectPath);
        return project is null ? [GlobalHost] : [GlobalHost, project];
    }

    private bool isProjectIdReserved(string id)
    {
        lock (projectLock)
            return projects.Values.Any(host => host.Plugins.Any(value => value.Id == id)
                || host.Management.Registry.Plugins.Any(value => value.Id == id));
    }
}
