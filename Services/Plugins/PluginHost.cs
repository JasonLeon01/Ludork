using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.Plugins;

public sealed class PluginHost : IEditorPluginRuntime, IDisposable
{
    private readonly string editorLanguage;
    private readonly List<PluginRuntimeEntry> runtimePlugins = [];
    private readonly SemaphoreSlim initializationGate = new(1, 1);
    private bool initialized;
    private bool disposed;

    public PluginHost(string editorLanguage, PluginEnvironment? environment = null)
    {
        this.editorLanguage = string.IsNullOrWhiteSpace(editorLanguage)
            ? "en_GB"
            : editorLanguage;
        Environment = environment ?? PluginPaths.Resolve();
        Management = new PluginManagementService(Environment);
    }

    public PluginEnvironment Environment { get; }

    public PluginManagementService Management { get; }

    public bool IsInitialized => initialized;

    public IReadOnlyList<PluginRuntimeInfo> Plugins =>
        runtimePlugins.Select(plugin => plugin.ToInfo()).ToArray();

    public IReadOnlyList<RegisteredPluginMenuCommand> MenuCommands =>
        runtimePlugins
            .Where(plugin => plugin.Status == PluginRuntimeStatus.Loaded)
            .SelectMany(plugin => plugin.MenuCommands.Select(command =>
                new RegisteredPluginMenuCommand(
                    plugin.RegistryEntry.Id,
                    plugin.Name,
                    plugin.SourcePath,
                    PluginPaths.GetPluginDataDirectory(
                        Environment,
                        plugin.RegistryEntry.Id),
                    command)))
            .OrderBy(command => command.Command.Location)
            .ThenBy(command => command.Command.Order)
            .ToArray();

    public IReadOnlyList<RegisteredPluginMapContextMenuCommand> MapContextMenuCommands =>
        runtimePlugins
            .Where(plugin => plugin.Status == PluginRuntimeStatus.Loaded)
            .SelectMany(plugin => plugin.MapContextMenuCommands.Select(command =>
                new RegisteredPluginMapContextMenuCommand(
                    plugin.RegistryEntry.Id,
                    plugin.Name,
                    plugin.SourcePath,
                    PluginPaths.GetPluginDataDirectory(
                        Environment,
                        plugin.RegistryEntry.Id),
                    command)))
            .OrderBy(command => command.Command.Order)
            .ToArray();

    public IReadOnlyList<RegisteredTextHintProvider> TextHintProviders =>
        runtimePlugins
            .Where(plugin => plugin.Status == PluginRuntimeStatus.Loaded)
            .SelectMany(plugin => plugin.TextHintProviders.Select(provider =>
                new RegisteredTextHintProvider(
                    plugin.RegistryEntry.Id,
                    plugin.Name,
                    provider)))
            .ToArray();

    public IReadOnlyList<RegisteredProjectOperationHook> BeforeRunHooks =>
        runtimePlugins
            .Where(plugin => plugin.Status == PluginRuntimeStatus.Loaded)
            .SelectMany(plugin => plugin.BeforeRunHooks.Select(hook =>
                new RegisteredProjectOperationHook(
                    plugin.RegistryEntry.Id,
                    plugin.Name,
                    hook)))
            .ToArray();

    public IReadOnlyList<RegisteredProjectOperationHook> BeforePackHooks =>
        runtimePlugins
            .Where(plugin => plugin.Status == PluginRuntimeStatus.Loaded)
            .SelectMany(plugin => plugin.BeforePackHooks.Select(hook =>
                new RegisteredProjectOperationHook(
                    plugin.RegistryEntry.Id,
                    plugin.Name,
                    hook)))
            .ToArray();

    public bool HasStartupFailures =>
        Management.RegistryDiagnostic.Length != 0
        || Management.StartupDiagnostics.Count != 0
        || runtimePlugins.Any(plugin => plugin.Status != PluginRuntimeStatus.Loaded);

    public string StartupFailureSummary
    {
        get
        {
            List<string> diagnostics = [];
            if (Management.RegistryDiagnostic.Length != 0)
                diagnostics.Add(Management.RegistryDiagnostic);
            diagnostics.AddRange(Management.StartupDiagnostics);
            foreach (PluginRuntimeEntry plugin in runtimePlugins)
            {
                if (plugin.Status == PluginRuntimeStatus.Loaded)
                    continue;
                diagnostics.Add(
                    $"{plugin.RegistryEntry.Id} ({plugin.Status}): {plugin.Diagnostic}");
            }
            return string.Join(
                System.Environment.NewLine + System.Environment.NewLine,
                diagnostics);
        }
    }

    public async Task InitializeAsync(CancellationToken cancellationToken = default)
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        await initializationGate.WaitAsync(cancellationToken);
        try
        {
            if (initialized)
                return;
            await Management.InitializeAsync(cancellationToken);
            PluginRegistryDocument registry = Management.Registry;
            HashSet<string> commandIds = new(StringComparer.Ordinal);
            for (int index = 0; index < registry.Plugins.Count; index++)
            {
                cancellationToken.ThrowIfCancellationRequested();
                PluginRegistryEntry registryEntry = registry.Plugins[index];
                PluginRuntimeEntry runtime = loadPlugin(
                    registryEntry,
                    index,
                    commandIds,
                    cancellationToken);
                runtimePlugins.Add(runtime);
            }
            initialized = true;
        }
        finally
        {
            initializationGate.Release();
        }
    }

    public string? ResolveTextHint(TextHintContext context)
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        foreach (PluginRuntimeEntry plugin in runtimePlugins)
        {
            if (plugin.Status != PluginRuntimeStatus.Loaded)
                continue;
            foreach (ITextHintProvider provider in plugin.TextHintProviders)
            {
                try
                {
                    string? result = provider.Resolve(context);
                    if (!string.IsNullOrWhiteSpace(result))
                        return result;
                }
                catch (Exception exception)
                {
                    plugin.AddDiagnostic(
                        $"Text hint provider failed: {formatException(exception)}");
                }
            }
        }
        return null;
    }

    public async Task<PluginResult> ExecuteBeforeProjectOperationAsync(
        ProjectOperationContext context)
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        IReadOnlyList<RegisteredProjectOperationHook> hooks = context.Operation switch
        {
            ProjectOperationKind.Run => BeforeRunHooks,
            ProjectOperationKind.Pack => BeforePackHooks,
            _ => [],
        };
        foreach (RegisteredProjectOperationHook registration in hooks)
        {
            context.CancellationToken.ThrowIfCancellationRequested();
            try
            {
                PluginResult result = await registration.Hook.ExecuteAsync(context);
                if (result.Success)
                    continue;
                string error = result.Error.Length == 0
                    ? "Plugin hook reported failure."
                    : result.Error;
                string message = $"[{registration.PluginName}] {error}";
                addRuntimeDiagnostic(registration.PluginId, message);
                context.Output.WriteLine(message);
                return PluginResult.Failed(message);
            }
            catch (OperationCanceledException) when (context.CancellationToken.IsCancellationRequested)
            {
                throw;
            }
            catch (Exception exception)
            {
                string message =
                    $"[{registration.PluginName}] {formatException(exception)}";
                addRuntimeDiagnostic(registration.PluginId, message);
                context.Output.WriteLine(message);
                return PluginResult.Failed(message);
            }
        }
        return PluginResult.Completed();
    }

    public async Task<IReadOnlyList<PluginManagementItem>> GetManagementItemsAsync(
        CancellationToken cancellationToken = default)
    {
        return await Management.GetItemsAsync(Plugins, cancellationToken);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        foreach (PluginRuntimeEntry plugin in runtimePlugins)
            plugin.Dispose();
        runtimePlugins.Clear();
        initializationGate.Dispose();
    }

    private PluginRuntimeEntry loadPlugin(
        PluginRegistryEntry registryEntry,
        int registryIndex,
        HashSet<string> commandIds,
        CancellationToken cancellationToken)
    {
        string sourcePath = Path.Combine(Environment.PluginsDirectory, registryEntry.Directory);
        PluginRuntimeEntry runtime = new(registryIndex, registryEntry, sourcePath);
        PluginCompilation? compilation = null;
        try
        {
            PluginPackageInspector.EnsureManagedDirectoryIsSafe(
                Environment.PluginsDirectory,
                registryEntry.Directory);
            PluginPackage package = PluginPackageInspector.Inspect(sourcePath);
            runtime.Name = package.Manifest.Name;
            runtime.Version = package.Manifest.Version;
            if (!string.Equals(package.Manifest.Id, registryEntry.Id, StringComparison.Ordinal))
            {
                throw new InvalidDataException(
                    $"Registry ID '{registryEntry.Id}' does not match manifest ID '{package.Manifest.Id}'.");
            }

            try
            {
                compilation = PluginCompiler.Compile(
                    package,
                    registryIndex,
                    cancellationToken);
            }
            catch (PluginCompilationException exception)
            {
                runtime.Status = PluginRuntimeStatus.CompileFailed;
                runtime.AddDiagnostic(exception.Message);
                return runtime;
            }

            runtime.AddDiagnostic(compilation.Diagnostics);
            ConstructorInfo constructor = PluginEntryPoint.GetConstructor(
                package,
                compilation.Assembly);

            IEditorPlugin instance = (IEditorPlugin)constructor.Invoke(null);
            string pluginDataDirectory = PluginPaths.GetPluginDataDirectory(
                Environment,
                package.Manifest.Id);
            Directory.CreateDirectory(pluginDataDirectory);
            PluginRegistrar registrar = new(
                sourcePath,
                pluginDataDirectory,
                editorLanguage);
            instance.Register(registrar);
            foreach (PluginMenuCommand command in registrar.MenuCommands)
            {
                if (commandIds.Contains(command.Id))
                {
                    throw new InvalidOperationException(
                        $"Plugin command ID is already registered: {command.Id}");
                }
            }
            foreach (PluginMapContextMenuCommand command in registrar.MapContextMenuCommands)
            {
                if (commandIds.Contains(command.Id))
                {
                    throw new InvalidOperationException(
                        $"Plugin command ID is already registered: {command.Id}");
                }
            }
            foreach (PluginMenuCommand command in registrar.MenuCommands)
                commandIds.Add(command.Id);
            foreach (PluginMapContextMenuCommand command in registrar.MapContextMenuCommands)
                commandIds.Add(command.Id);
            runtime.LoadContext = compilation.LoadContext;
            runtime.Instance = instance;
            runtime.MenuCommands = registrar.MenuCommands;
            runtime.MapContextMenuCommands = registrar.MapContextMenuCommands;
            runtime.TextHintProviders = registrar.TextHintProviders;
            runtime.BeforeRunHooks = registrar.BeforeRunHooks;
            runtime.BeforePackHooks = registrar.BeforePackHooks;
            runtime.Status = PluginRuntimeStatus.Loaded;
            compilation = null;
            return runtime;
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or InvalidDataException
            or JsonException)
        {
            runtime.Status = PluginRuntimeStatus.ManifestInvalid;
            runtime.AddDiagnostic(formatException(exception));
            return runtime;
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception exception)
        {
            runtime.Status = PluginRuntimeStatus.InitializationFailed;
            runtime.AddDiagnostic(formatException(exception));
            return runtime;
        }
        finally
        {
            compilation?.LoadContext.Unload();
        }
    }

    private void addRuntimeDiagnostic(string pluginId, string diagnostic)
    {
        PluginRuntimeEntry? plugin = runtimePlugins.FirstOrDefault(
            value => string.Equals(
                value.RegistryEntry.Id,
                pluginId,
                StringComparison.Ordinal));
        plugin?.AddDiagnostic(diagnostic);
    }

    private static string formatException(Exception exception)
    {
        Exception current = exception;
        while (current is TargetInvocationException invocation
            && invocation.InnerException is not null)
        {
            current = invocation.InnerException;
        }
        return current.ToString();
    }
}
