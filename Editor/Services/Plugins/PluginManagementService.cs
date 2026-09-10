using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.Plugins;

public sealed class PluginManagementService
{
    private readonly PluginEnvironment environment;
    private readonly PluginRegistryStore store;
    private readonly SemaphoreSlim gate = new(1, 1);
    private readonly List<string> startupDiagnostics = [];
    private PluginRegistryState state = new(
        false,
        true,
        new PluginRegistryDocument(),
        string.Empty);
    private bool initialized;

    internal PluginManagementService(PluginEnvironment environment)
    {
        this.environment = environment;
        store = new PluginRegistryStore(environment);
    }

    public PluginEnvironment Environment => environment;

    public string RegistryDiagnostic => state.Diagnostic;

    public IReadOnlyList<string> StartupDiagnostics => startupDiagnostics;

    internal PluginRegistryDocument Registry => state.Document.Clone();

    internal async Task InitializeAsync(CancellationToken cancellationToken)
    {
        await gate.WaitAsync(cancellationToken);
        try
        {
            if (initialized)
                return;
            state = await store.LoadAsync(cancellationToken);
            if (state.IsValid)
                await processPendingDeletesAsync(cancellationToken);
            initialized = true;
        }
        finally
        {
            gate.Release();
        }
    }

    public async Task<PluginImportPreview> PreviewImportAsync(
        string sourcePath,
        CancellationToken cancellationToken = default)
    {
        await ensureInitializedAsync(cancellationToken);
        await gate.WaitAsync(cancellationToken);
        try
        {
            if (!state.IsValid)
                return failedPreview(sourcePath, state.Diagnostic);
            return validateImport(sourcePath);
        }
        finally
        {
            gate.Release();
        }
    }

    public async Task<PluginManagementResult> ImportAsync(
        string sourcePath,
        CancellationToken cancellationToken = default)
    {
        await ensureInitializedAsync(cancellationToken);
        await gate.WaitAsync(cancellationToken);
        try
        {
            if (!state.IsValid)
                return PluginManagementResult.Failed(state.Diagnostic);

            PluginImportPreview preview = validateImport(sourcePath);
            if (!preview.Success)
                return PluginManagementResult.Failed(preview.Error);
            PluginPackage package = PluginPackageInspector.Inspect(preview.SourcePath);
            string directory = Path.GetFileName(
                Path.TrimEndingDirectorySeparator(package.SourcePath));
            string targetPath = Path.Combine(environment.PluginsDirectory, directory);
            bool alreadyManaged = pathsEqual(
                Path.GetDirectoryName(package.SourcePath),
                environment.PluginsDirectory);
            bool copied = false;

            if (!alreadyManaged)
            {
                Directory.CreateDirectory(environment.PluginsDirectory);
                string stagingPath = Path.Combine(
                    environment.PluginsDirectory,
                    $".ludork-import-{Guid.NewGuid():N}");
                try
                {
                    copyDirectory(package.SourcePath, stagingPath);
                    PluginPackage stagedPackage =
                        PluginPackageInspector.Inspect(stagingPath);
                    if (!string.Equals(
                            stagedPackage.Manifest.Id,
                            package.Manifest.Id,
                            StringComparison.Ordinal)
                        || !string.Equals(
                            stagedPackage.Manifest.Version,
                            package.Manifest.Version,
                            StringComparison.Ordinal))
                    {
                        throw new InvalidDataException(
                            "Copied plugin manifest does not match the selected directory.");
                    }
                    validateCompilation(
                        stagedPackage,
                        state.Document.Plugins.Count,
                        cancellationToken);
                    if (Directory.Exists(targetPath) || File.Exists(targetPath))
                    {
                        throw new IOException(
                            $"Plugin target already exists: {targetPath}");
                    }
                    Directory.Move(stagingPath, targetPath);
                    copied = true;
                }
                finally
                {
                    if (Directory.Exists(stagingPath))
                        Directory.Delete(stagingPath, true);
                }
            }
            else
            {
                validateCompilation(
                    package,
                    state.Document.Plugins.Count,
                    cancellationToken);
            }

            PluginRegistryDocument updated = state.Document.Clone();
            updated.Plugins.Add(new PluginRegistryEntry
            {
                Id = package.Manifest.Id,
                Directory = directory,
            });
            try
            {
                await store.SaveAsync(updated, cancellationToken);
            }
            catch
            {
                if (copied && Directory.Exists(targetPath))
                {
                    PluginPackageInspector.EnsureDirectoryTreeIsSafe(targetPath);
                    Directory.Delete(targetPath, true);
                }
                throw;
            }
            state = new PluginRegistryState(true, true, updated, string.Empty);
            return PluginManagementResult.Completed(true);
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or InvalidDataException
            or JsonException
            or PluginCompilationException)
        {
            return PluginManagementResult.Failed(exception.Message);
        }
        finally
        {
            gate.Release();
        }
    }

    public async Task<PluginManagementResult> UnregisterAsync(
        string pluginId,
        bool deleteSource,
        CancellationToken cancellationToken = default)
    {
        await ensureInitializedAsync(cancellationToken);
        await gate.WaitAsync(cancellationToken);
        try
        {
            if (!state.IsValid)
                return PluginManagementResult.Failed(state.Diagnostic);
            PluginRegistryEntry? entry = state.Document.Plugins.FirstOrDefault(
                value => string.Equals(value.Id, pluginId, StringComparison.Ordinal));
            if (entry is null)
                return PluginManagementResult.Failed($"Plugin is not registered: {pluginId}");

            PluginRegistryDocument updated = state.Document.Clone();
            updated.Plugins.RemoveAll(
                value => string.Equals(value.Id, pluginId, StringComparison.Ordinal));
            if (deleteSource && !updated.PendingDelete.Contains(
                    entry.Directory,
                    directoryComparer()))
            {
                PluginPackageInspector.EnsureManagedDirectoryIsSafe(
                    environment.PluginsDirectory,
                    entry.Directory);
                updated.PendingDelete.Add(entry.Directory);
            }

            await store.SaveAsync(updated, cancellationToken);
            state = new PluginRegistryState(true, true, updated, string.Empty);
            return PluginManagementResult.Completed(true);
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or InvalidDataException)
        {
            return PluginManagementResult.Failed(exception.Message);
        }
        finally
        {
            gate.Release();
        }
    }

    public async Task<IReadOnlyList<PluginManagementItem>> GetItemsAsync(
        IReadOnlyList<PluginRuntimeInfo> runtimePlugins,
        CancellationToken cancellationToken = default)
    {
        await ensureInitializedAsync(cancellationToken);
        await gate.WaitAsync(cancellationToken);
        try
        {
            List<PluginManagementItem> items = [];
            HashSet<string> consumedRuntime = new(StringComparer.Ordinal);
            foreach (PluginRegistryEntry entry in state.Document.Plugins)
            {
                PluginRuntimeInfo? runtime = runtimePlugins.FirstOrDefault(
                    value => string.Equals(value.Id, entry.Id, StringComparison.Ordinal)
                        && directoriesEqual(value.Directory, entry.Directory));
                if (runtime is not null)
                {
                    consumedRuntime.Add(runtime.Id);
                    items.Add(toManagementItem(runtime, true));
                    continue;
                }

                string sourcePath = Path.Combine(environment.PluginsDirectory, entry.Directory);
                try
                {
                    PluginPackage package = PluginPackageInspector.Inspect(sourcePath);
                    if (!string.Equals(package.Manifest.Id, entry.Id, StringComparison.Ordinal))
                    {
                        throw new InvalidDataException(
                            $"Registry ID '{entry.Id}' does not match manifest ID '{package.Manifest.Id}'.");
                    }
                    items.Add(new PluginManagementItem(
                        entry.Id,
                        package.Manifest.Name,
                        package.Manifest.Version,
                        entry.Directory,
                        sourcePath,
                        PluginRuntimeStatus.PendingRestart,
                        string.Empty,
                        true));
                }
                catch (Exception exception) when (
                    exception is IOException
                    or UnauthorizedAccessException
                    or InvalidDataException
                    or JsonException)
                {
                    items.Add(new PluginManagementItem(
                        entry.Id,
                        entry.Id,
                        string.Empty,
                        entry.Directory,
                        sourcePath,
                        PluginRuntimeStatus.ManifestInvalid,
                        exception.Message,
                        true));
                }
            }

            foreach (PluginRuntimeInfo runtime in runtimePlugins)
            {
                if (consumedRuntime.Contains(runtime.Id))
                    continue;
                bool pendingDeletion = state.Document.PendingDelete.Any(
                    directory => directoriesEqual(directory, runtime.Directory));
                items.Add(new PluginManagementItem(
                    runtime.Id,
                    runtime.Name,
                    runtime.Version,
                    runtime.Directory,
                    runtime.SourcePath,
                    pendingDeletion
                        ? PluginRuntimeStatus.PendingDeletion
                        : PluginRuntimeStatus.UnregisteredPendingRestart,
                    runtime.Diagnostic,
                    false));
            }

            foreach (string directory in state.Document.PendingDelete)
            {
                bool represented = items.Any(item => directoriesEqual(item.Directory, directory));
                if (represented)
                    continue;
                items.Add(new PluginManagementItem(
                    directory,
                    directory,
                    string.Empty,
                    directory,
                    Path.Combine(environment.PluginsDirectory, directory),
                    PluginRuntimeStatus.PendingDeletion,
                    string.Empty,
                    false));
            }
            return items;
        }
        finally
        {
            gate.Release();
        }
    }

    private async Task ensureInitializedAsync(CancellationToken cancellationToken)
    {
        if (!initialized)
            await InitializeAsync(cancellationToken);
    }

    private PluginImportPreview validateImport(string sourcePath)
    {
        try
        {
            PluginPackage package = PluginPackageInspector.Inspect(sourcePath);
            string source = package.SourcePath;
            string directory = Path.GetFileName(
                Path.TrimEndingDirectorySeparator(source));
            if (!PluginPackageInspector.IsSafeDirectoryName(directory))
                throw new InvalidDataException($"Invalid plugin directory name: {directory}");

            bool sourceInsideManagedRoot = pathIsInside(
                source,
                environment.PluginsDirectory);
            bool sourceIsImmediateChild = pathsEqual(
                Path.GetDirectoryName(source),
                environment.PluginsDirectory);
            if (sourceInsideManagedRoot && !sourceIsImmediateChild)
            {
                throw new InvalidDataException(
                    "A plugin inside the managed Plugins directory must be an immediate child directory.");
            }
            if (pathIsInside(environment.PluginsDirectory, source))
            {
                throw new InvalidDataException(
                    "The managed Plugins directory cannot be inside the imported plugin directory.");
            }

            if (state.Document.Plugins.Any(entry =>
                    string.Equals(
                        entry.Id,
                        package.Manifest.Id,
                        StringComparison.Ordinal)))
            {
                throw new InvalidDataException(
                    $"Plugin ID is already registered: {package.Manifest.Id}");
            }
            if (state.Document.Plugins.Any(entry =>
                    directoriesEqual(entry.Directory, directory)))
            {
                throw new InvalidDataException(
                    $"Plugin directory is already registered: {directory}");
            }
            if (state.Document.PendingDelete.Any(value =>
                    directoriesEqual(value, directory)))
            {
                throw new InvalidDataException(
                    $"Plugin directory is pending deletion: {directory}");
            }

            string targetPath = Path.Combine(
                environment.PluginsDirectory,
                directory);
            if (!sourceIsImmediateChild
                && (Directory.Exists(targetPath) || File.Exists(targetPath)))
            {
                throw new InvalidDataException(
                    $"Plugin target already exists: {targetPath}");
            }

            return new PluginImportPreview(
                true,
                package.Manifest.Id,
                package.Manifest.Name,
                package.Manifest.Version,
                package.SourcePath,
                string.Empty);
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or InvalidDataException
            or JsonException)
        {
            return failedPreview(sourcePath, exception.Message);
        }
    }

    private async Task processPendingDeletesAsync(CancellationToken cancellationToken)
    {
        if (state.Document.PendingDelete.Count == 0)
            return;

        PluginRegistryDocument updated = state.Document.Clone();
        List<string> remaining = [];
        foreach (string directory in state.Document.PendingDelete)
        {
            cancellationToken.ThrowIfCancellationRequested();
            try
            {
                PluginPackageInspector.EnsureManagedDirectoryIsSafe(
                    environment.PluginsDirectory,
                    directory);
                string path = Path.Combine(environment.PluginsDirectory, directory);
                if (Directory.Exists(path))
                {
                    PluginPackageInspector.EnsureDirectoryTreeIsSafe(path);
                    Directory.Delete(path, true);
                }
                else if (File.Exists(path))
                {
                    throw new InvalidDataException(
                        $"Pending plugin path is not a directory: {path}");
                }
            }
            catch (Exception exception) when (
                exception is IOException
                or UnauthorizedAccessException
                or InvalidDataException)
            {
                remaining.Add(directory);
                startupDiagnostics.Add(
                    $"Failed to delete plugin directory '{directory}': {exception.Message}");
            }
        }

        updated.PendingDelete.Clear();
        updated.PendingDelete.AddRange(remaining);
        if (updated.PendingDelete.SequenceEqual(
                state.Document.PendingDelete,
                directoryComparer()))
        {
            return;
        }

        try
        {
            await store.SaveAsync(updated, cancellationToken);
            state = new PluginRegistryState(true, true, updated, string.Empty);
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or InvalidDataException)
        {
            startupDiagnostics.Add(
                $"Failed to update pending plugin deletions: {exception.Message}");
        }
    }

    private static void validateCompilation(
        PluginPackage package,
        int registryIndex,
        CancellationToken cancellationToken)
    {
        PluginCompilation compilation = PluginCompiler.Compile(
            package,
            registryIndex,
            cancellationToken);
        try
        {
            PluginEntryPoint.GetConstructor(package, compilation.Assembly);
        }
        finally
        {
            compilation.LoadContext.Unload();
        }
    }

    private static void copyDirectory(string sourceRoot, string targetRoot)
    {
        PluginPackageInspector.EnsureDirectoryTreeIsSafe(sourceRoot);
        Directory.CreateDirectory(targetRoot);
        Queue<(string Source, string Target)> pending = new();
        pending.Enqueue((sourceRoot, targetRoot));
        while (pending.Count != 0)
        {
            (string source, string target) = pending.Dequeue();
            foreach (string sourcePath in Directory.EnumerateFileSystemEntries(source))
            {
                FileAttributes attributes = File.GetAttributes(sourcePath);
                if ((attributes & FileAttributes.ReparsePoint) != 0)
                    throw new InvalidDataException($"Symbolic links are not allowed: {sourcePath}");
                string targetPath = Path.Combine(target, Path.GetFileName(sourcePath));
                if ((attributes & FileAttributes.Directory) != 0)
                {
                    Directory.CreateDirectory(targetPath);
                    pending.Enqueue((sourcePath, targetPath));
                }
                else
                {
                    File.Copy(sourcePath, targetPath, false);
                }
            }
        }
    }

    private static PluginImportPreview failedPreview(string sourcePath, string error)
    {
        return new PluginImportPreview(
            false,
            string.Empty,
            string.Empty,
            string.Empty,
            sourcePath,
            error);
    }

    private static PluginManagementItem toManagementItem(
        PluginRuntimeInfo runtime,
        bool canUninstall)
    {
        return new PluginManagementItem(
            runtime.Id,
            runtime.Name,
            runtime.Version,
            runtime.Directory,
            runtime.SourcePath,
            runtime.Status,
            runtime.Diagnostic,
            canUninstall);
    }

    private static StringComparer directoryComparer()
    {
        return OperatingSystem.IsWindows()
            ? StringComparer.OrdinalIgnoreCase
            : StringComparer.Ordinal;
    }

    private static bool directoriesEqual(string left, string right)
    {
        return directoryComparer().Equals(left, right);
    }

    private static bool pathsEqual(string? left, string? right)
    {
        if (left is null || right is null)
            return false;
        string leftPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(left));
        string rightPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(right));
        StringComparison comparison = OperatingSystem.IsWindows()
            ? StringComparison.OrdinalIgnoreCase
            : StringComparison.Ordinal;
        return string.Equals(leftPath, rightPath, comparison);
    }

    private static bool pathIsInside(string path, string possibleParent)
    {
        string fullPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
        string fullParent = Path.TrimEndingDirectorySeparator(Path.GetFullPath(possibleParent));
        if (pathsEqual(fullPath, fullParent))
            return true;
        string parentWithSeparator = fullParent + Path.DirectorySeparatorChar;
        StringComparison comparison = OperatingSystem.IsWindows()
            ? StringComparison.OrdinalIgnoreCase
            : StringComparison.Ordinal;
        return fullPath.StartsWith(parentWithSeparator, comparison);
    }
}
