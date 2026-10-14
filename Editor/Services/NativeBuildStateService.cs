using System;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class NativeBuildStateService : IDisposable
{
    private readonly object stateLock = new();
    private readonly string projectPath;
    private readonly string cacheDirectory;
    private readonly ProjectStateWorker stateWorker;
    private readonly string successfulBuildPath;
    private readonly FileSystemWatcher watcher;
    private readonly SemaphoreSlim checkLock = new(1, 1);
    private readonly CancellationTokenSource lifetime = new();
    private volatile bool disposed;

    public NativeBuildStateService(string projectPath, ProjectStateWorker stateWorker)
    {
        this.projectPath = Path.GetFullPath(projectPath);
        this.stateWorker = stateWorker;
        cacheDirectory = Path.Combine(this.projectPath, ProjectToolConstants.EditorCacheDirectory);
        successfulBuildPath = Path.Combine(cacheDirectory, "NativeBuild-Debug.json");
        watcher = new FileSystemWatcher(this.projectPath)
        {
            IncludeSubdirectories = true,
            NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName,
        };
        watcher.Created += onFileChanged;
        watcher.Deleted += onFileChanged;
        watcher.Renamed += onFileRenamed;
        watcher.Error += (_, _) => RefreshAvailability();
        watcher.EnableRaisingEvents = true;
        RefreshAvailability();
    }

    public bool HasSuccessfulBuild { get; private set; }
    public string Detail { get; private set; } = string.Empty;
    public event EventHandler? Changed;

    public bool HasGeneratedFiles()
    {
        if (disposed || !File.Exists(Path.Combine(projectPath, "build", "CMakeCache.txt"))
            || !Directory.Exists(Path.Combine(projectPath, "Intermediate"))
            || File.Exists(Path.Combine(cacheDirectory, "NativeBuild-Debug.pending.json"))
            || File.Exists(Path.Combine(cacheDirectory, "Preview.building")))
            return false;
        string[] modules = ["Engine", "GlobalCore", "GlobalFunctions"];
        if (modules.Any(module => !File.Exists(Path.Combine(projectPath, "Scripts", module + "_meta.lua"))
                || !File.Exists(Path.Combine(projectPath, "Scripts", "stub", module + ".d.lua")))
            || !File.Exists(Path.Combine(projectPath, "Scripts", "stub", "LuaSF.d.lua"))
            || !File.Exists(Path.Combine(cacheDirectory, "UiPreview.registry.json")))
            return false;
        try
        {
            string buildPath = Path.Combine(projectPath, "build").Replace('\\', '/');
            string buildHash = Convert.ToHexStringLower(SHA256.HashData(Encoding.UTF8.GetBytes(buildPath)))[..8];
            string intermediate = Path.Combine(projectPath, "Intermediate", "build-" + buildHash);
            if (modules.Any(module => !File.Exists(Path.Combine(intermediate, module,
                    "generated-bindings", module + ".bindings.stamp"))
                || !File.Exists(Path.Combine(intermediate, module, "generated-declarations", module + "_meta.lua"))))
                return false;
            using JsonDocument build = JsonDocument.Parse(File.ReadAllText(successfulBuildPath));
            JsonElement record = build.RootElement;
            if (!isCurrentContext(record, false)
                || !record.TryGetProperty("artifacts", out JsonElement artifacts)
                || artifacts.ValueKind != JsonValueKind.Object)
                return false;
            string executable = OperatingSystem.IsWindows() ? "Main.exe" : "Main";
            string runtimeDirectory = Path.Combine(projectPath, "bin", "Debug");
            if (!artifacts.TryGetProperty(executable, out _)
                || artifacts.EnumerateObject().Any(artifact => !isFileName(artifact.Name)
                    || !File.Exists(Path.Combine(runtimeDirectory, artifact.Name))))
                return false;
            using JsonDocument preview = JsonDocument.Parse(File.ReadAllText(Path.Combine(cacheDirectory, "Preview.json")));
            JsonElement manifest = preview.RootElement;
            if (!isCurrentContext(manifest, true)
                || !manifest.TryGetProperty("runtimeDirectory", out JsonElement previewDirectory)
                || previewDirectory.ValueKind != JsonValueKind.String
                || previewDirectory.GetString() != "bin/Debug"
                || !manifest.TryGetProperty("files", out JsonElement files)
                || files.ValueKind != JsonValueKind.Array)
                return false;
            string host = OperatingSystem.IsWindows() ? "PreviewHost.exe" : "PreviewHost";
            bool hasHost = false;
            foreach (JsonElement file in files.EnumerateArray())
            {
                if (file.ValueKind != JsonValueKind.String || file.GetString() is not string name
                    || !isFileName(name)
                    || !File.Exists(name == "UiPreview.registry.json"
                        ? Path.Combine(cacheDirectory, name) : Path.Combine(runtimeDirectory, name)))
                    return false;
                hasHost |= name == host;
            }
            return hasHost;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or JsonException)
        {
            return false;
        }
    }

    private static bool isCurrentContext(JsonElement record, bool preview)
    {
        string platform = OperatingSystem.IsWindows() ? "win32"
            : OperatingSystem.IsMacOS() ? "darwin" : OperatingSystem.IsLinux() ? "linux" : string.Empty;
        string architecture = RuntimeInformation.ProcessArchitecture switch
        {
            Architecture.X64 => preview ? "x64" : OperatingSystem.IsWindows() ? "amd64" : "x86_64",
            Architecture.Arm64 => !preview && OperatingSystem.IsLinux() ? "aarch64" : "arm64",
            Architecture.X86 => preview ? "x86" : OperatingSystem.IsWindows() ? "x86" : "i686",
            Architecture.Arm => preview ? "arm" : "armv7l",
            _ => string.Empty,
        };
        return platform.Length != 0 && architecture.Length != 0 && record.ValueKind == JsonValueKind.Object
            && record.TryGetProperty("formatVersion", out JsonElement version)
            && version.ValueKind == JsonValueKind.Number && version.TryGetInt32(out int number) && number == 1
            && record.TryGetProperty("platform", out JsonElement savedPlatform)
            && savedPlatform.ValueKind == JsonValueKind.String && savedPlatform.GetString() == platform
            && record.TryGetProperty("architecture", out JsonElement savedArchitecture)
            && savedArchitecture.ValueKind == JsonValueKind.String && savedArchitecture.GetString() == architecture
            && record.TryGetProperty("configuration", out JsonElement configuration)
            && configuration.ValueKind == JsonValueKind.String && configuration.GetString() == "Debug";
    }

    private static bool isFileName(string name) => name.Length != 0 && name is not "." and not ".."
        && name.IndexOfAny(['/', '\\', ':']) < 0 && name.IndexOfAny(Path.GetInvalidFileNameChars()) < 0;

    public async Task<bool> CheckAsync(CancellationToken cancellationToken = default)
    {
        if (disposed)
            return false;
        using CancellationTokenSource cancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken, lifetime.Token);
        await checkLock.WaitAsync(cancellation.Token);
        try
        {
            (JsonElement result, _) = await stateWorker.ExecuteAsync("native-check", cancellation.Token, "Debug");
            bool current = result.GetProperty("current").GetBoolean();
            string detail = result.GetProperty("detail").GetString() ?? string.Empty;
            return setResult(current, detail);
        }
        finally
        {
            RefreshAvailability();
            checkLock.Release();
        }
    }

    public void RefreshAvailability()
    {
        lock (stateLock)
        {
            if (disposed)
                return;
            bool hasSuccessfulBuild = File.Exists(successfulBuildPath);
            if (HasSuccessfulBuild == hasSuccessfulBuild)
                return;
            HasSuccessfulBuild = hasSuccessfulBuild;
            Changed?.Invoke(this, EventArgs.Empty);
        }
    }

    private bool setResult(bool current, string detail)
    {
        Detail = detail;
        return current;
    }

    private void onFileChanged(object sender, FileSystemEventArgs args)
    {
        if (isBuildRecordPath(args.FullPath))
            RefreshAvailability();
    }

    private void onFileRenamed(object sender, RenamedEventArgs args)
    {
        if (isBuildRecordPath(args.OldFullPath) || isBuildRecordPath(args.FullPath))
            RefreshAvailability();
    }

    private bool isBuildRecordPath(string path)
    {
        return path.Equals(successfulBuildPath, StringComparison.OrdinalIgnoreCase)
            || path.Equals(cacheDirectory, StringComparison.OrdinalIgnoreCase);
    }

    public void Dispose()
    {
        lock (stateLock)
        {
            if (disposed)
                return;
            disposed = true;
        }
        watcher.Dispose();
        lifetime.Cancel();
    }
}
