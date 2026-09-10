using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class NativeBuildStateService : IDisposable
{
    private readonly object stateLock = new();
    private readonly string projectPath;
    private readonly string buildDirectory;
    private readonly string successfulBuildPath;
    private readonly FileSystemWatcher watcher;
    private readonly SemaphoreSlim checkLock = new(1, 1);
    private readonly CancellationTokenSource lifetime = new();
    private volatile bool disposed;

    public NativeBuildStateService(string projectPath)
    {
        this.projectPath = Path.GetFullPath(projectPath);
        buildDirectory = Path.Combine(this.projectPath, "build");
        successfulBuildPath = Path.Combine(buildDirectory, "NativeBuild-Debug.json");
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

    public async Task<bool> CheckAsync(CancellationToken cancellationToken = default)
    {
        if (disposed)
            return false;
        using CancellationTokenSource cancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken, lifetime.Token);
        await checkLock.WaitAsync(cancellation.Token);
        try
        {
            string executable = OperatingSystem.IsWindows() ? "ScriptTools.exe" : "ScriptTools";
            string? toolPath = EditorRuntimePaths.FindFile("tools", executable)
                ?? EditorRuntimePaths.FindFile(".tools", "ScriptTools", executable);
            if (toolPath is null)
                return setResult(false, "ScriptTools was not found in the editor installation.");
            ProcessStartInfo startInfo = new()
            {
                FileName = toolPath,
                WorkingDirectory = projectPath,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                StandardOutputEncoding = Encoding.UTF8,
                StandardErrorEncoding = Encoding.UTF8,
            };
            startInfo.ArgumentList.Add("native-build-state");
            startInfo.ArgumentList.Add("check");
            startInfo.ArgumentList.Add(projectPath);
            startInfo.ArgumentList.Add("Debug");
            startInfo.Environment["PYTHONUTF8"] = "1";
            startInfo.Environment["PYTHONIOENCODING"] = "utf-8";
            using Process process = new() { StartInfo = startInfo };
            process.Start();
            Task<string> output = process.StandardOutput.ReadToEndAsync(cancellation.Token);
            Task<string> error = process.StandardError.ReadToEndAsync(cancellation.Token);
            using CancellationTokenRegistration registration = cancellation.Token.Register(() => stopProcess(process));
            await process.WaitForExitAsync(cancellation.Token);
            string outputText = await output;
            string errorText = await error;
            if (process.ExitCode != 0)
                return setResult(false, errorText.Trim());
            using JsonDocument document = JsonDocument.Parse(outputText);
            bool current = document.RootElement.GetProperty("current").GetBoolean();
            string detail = document.RootElement.GetProperty("detail").GetString() ?? string.Empty;
            return setResult(current, detail);
        }
        catch (Exception exception) when (exception is Win32Exception or IOException or JsonException
            or InvalidOperationException or KeyNotFoundException)
        {
            return setResult(false, exception.Message);
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
            || path.Equals(buildDirectory, StringComparison.OrdinalIgnoreCase);
    }

    private static void stopProcess(Process process)
    {
        try
        {
            if (!process.HasExited)
                process.Kill(true);
        }
        catch (Exception exception) when (exception is Win32Exception or InvalidOperationException)
        {
        }
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
