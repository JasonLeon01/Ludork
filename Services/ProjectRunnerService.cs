using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;
using Ludork.Plugin.Abstractions;

namespace Ludork.Services;

public enum ProjectRunState
{
    Idle,
    Building,
    Running,
}

public enum ProjectRunFailure
{
    None,
    ProjectInvalid,
    PluginPreparationFailed,
    BuildToolMissing,
    BuildFailed,
    ExecutableMissing,
    LaunchFailed,
    GameFailed,
    EmbeddedHandleUnavailable,
    ProtocolMismatch,
}

public enum ProjectWindowMode
{
    Embedded,
    Individual,
}

public sealed record ProjectRunOptions(
    bool IsStandaloneProject,
    ProjectWindowMode WindowMode,
    nint WindowHandle);

public sealed record RuntimeInputEvent(
    string Type,
    string? Key = null,
    string? Button = null,
    int? X = null,
    int? Y = null,
    double? Delta = null,
    bool? Alt = null,
    bool? Control = null,
    bool? Shift = null,
    bool? System = null);

public sealed record ProjectRunResult(
    bool Success,
    bool Cancelled,
    ProjectRunFailure Failure,
    string Detail)
{
    public static ProjectRunResult Completed() => new(true, false, ProjectRunFailure.None, string.Empty);
    public static ProjectRunResult CancelledResult() => new(false, true, ProjectRunFailure.None, string.Empty);
    public static ProjectRunResult Failed(ProjectRunFailure failure, string detail) => new(false, false, failure, detail);
}

public sealed partial class ProjectRunnerService : IDisposable
{
    private const int BridgeProtocolVersion = 2;
    private const int MaximumBridgeMessageSize = 64 * 1024;
    private const int MaximumInputEventsPerBatch = 128;
    private const string PerformanceSamplePrefix = "__LUDORK_PERF__:";
    private static readonly UTF8Encoding utf8 = new(false);
    private static readonly JsonSerializerOptions bridgeJsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };
    private readonly object processLock = new();
    private readonly SemaphoreSlim commandWriteLock = new(1, 1);
    private readonly string projectPath;
    private readonly ProjectOperationPipeline operationPipeline;
    private Process? activeProcess;
    private CancellationTokenSource? runCancellation;
    private TcpClient? commandClient;
    private long runGeneration;
    private bool disposed;

    public ProjectRunnerService(string projectPath, IEditorPluginRuntime? pluginRuntime = null)
    {
        if (!Path.IsPathFullyQualified(projectPath))
            throw new ArgumentException(nameof(projectPath));
        this.projectPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(projectPath));
        operationPipeline = new ProjectOperationPipeline(this.projectPath, pluginRuntime);
    }

    public ProjectRunState State { get; private set; }
    public bool CanSendCommand { get; private set; }
    public long RunGeneration => Interlocked.Read(ref runGeneration);
    public event EventHandler<string>? OutputReceived;
    public event EventHandler<PerformanceSample>? PerformanceSampleReceived;
    public event EventHandler<ProjectRunState>? StateChanged;
    public event EventHandler<bool>? CommandAvailabilityChanged;

    public async Task<ProjectRunResult> StartAsync(ProjectRunOptions options)
    {
        if (disposed || State != ProjectRunState.Idle)
            return ProjectRunResult.CancelledResult();

        if (options.WindowMode == ProjectWindowMode.Embedded
            && (!OperatingSystem.IsWindows() || options.WindowHandle == nint.Zero))
        {
            return ProjectRunResult.Failed(
                ProjectRunFailure.EmbeddedHandleUnavailable,
                options.WindowHandle.ToString());
        }

        string? projectError = validateProject(options.IsStandaloneProject);
        if (projectError is not null)
            return ProjectRunResult.Failed(ProjectRunFailure.ProjectInvalid, projectError);

        Interlocked.Increment(ref runGeneration);

        CancellationTokenSource cancellation = new();
        lock (processLock)
            runCancellation = cancellation;

        try
        {
            setState(ProjectRunState.Building);
            PluginResult preparation = await operationPipeline.ExecuteAsync(
                ProjectOperationKind.Run,
                writeOutput,
                cancellation.Token);
            if (!preparation.Success)
            {
                return ProjectRunResult.Failed(
                    ProjectRunFailure.PluginPreparationFailed,
                    preparation.Error);
            }

            if (!options.IsStandaloneProject)
            {
                string buildScriptName = OperatingSystem.IsWindows()
                    ? "build_cpp.bat"
                    : "build_cpp.sh";
                string? buildScript = EditorRuntimePaths.FindFile("tools", buildScriptName);
                if (buildScript is null)
                    return ProjectRunResult.Failed(ProjectRunFailure.BuildToolMissing, "tools/" + buildScriptName);

                writeOutput($"> {buildScript} \"{projectPath}\" Debug");
                ProcessStartInfo buildStartInfo = createBuildStartInfo(buildScript);
                int buildExitCode = await runProcessAsync(buildStartInfo, cancellation.Token);
                if (cancellation.IsCancellationRequested)
                    return ProjectRunResult.CancelledResult();
                if (buildExitCode != 0)
                    return ProjectRunResult.Failed(ProjectRunFailure.BuildFailed, buildExitCode.ToString());
            }

            string executableName = OperatingSystem.IsWindows() ? "Main.exe" : "Main";
            string executablePath = options.IsStandaloneProject
                ? Path.Combine(projectPath, executableName)
                : Path.Combine(projectPath, "bin", "Debug", executableName);
            executablePath = Path.GetFullPath(executablePath);
            if (!File.Exists(executablePath))
                return ProjectRunResult.Failed(ProjectRunFailure.ExecutableMissing, executablePath);
            if (cancellation.IsCancellationRequested)
                return ProjectRunResult.CancelledResult();

            int commandPort = reserveCommandPort();
            setState(ProjectRunState.Running);
            writeOutput($"> {executablePath}");
            ProcessStartInfo gameStartInfo = createGameStartInfo(executablePath, commandPort, options);
            GameProcessResult gameResult = await runGameProcessAsync(gameStartInfo, commandPort, cancellation.Token);
            if (cancellation.IsCancellationRequested)
                return ProjectRunResult.CancelledResult();
            if (gameResult.ProtocolFailure is not null)
                return ProjectRunResult.Failed(ProjectRunFailure.ProtocolMismatch, gameResult.ProtocolFailure);
            return gameResult.ExitCode == 0
                ? ProjectRunResult.Completed()
                : ProjectRunResult.Failed(ProjectRunFailure.GameFailed, gameResult.ExitCode.ToString());
        }
        catch (OperationCanceledException)
        {
            return ProjectRunResult.CancelledResult();
        }
        catch (Win32Exception exception)
        {
            return ProjectRunResult.Failed(ProjectRunFailure.LaunchFailed, exception.Message);
        }
        catch (InvalidOperationException exception)
        {
            return ProjectRunResult.Failed(ProjectRunFailure.LaunchFailed, exception.Message);
        }
        finally
        {
            closeCommandConnection();
            lock (processLock)
            {
                if (ReferenceEquals(runCancellation, cancellation))
                    runCancellation = null;
            }
            cancellation.Dispose();
            setState(ProjectRunState.Idle);
        }
    }

    public async Task<bool> SendCommandAsync(string command)
    {
        return await SendCommandAsync(command, RunGeneration);
    }

    public async Task<bool> SendCommandAsync(string command, long expectedRunGeneration)
    {
        string line = command.Trim();
        if (disposed || expectedRunGeneration != RunGeneration
            || line.Length == 0 || line.Contains('\r') || line.Contains('\n'))
            return false;

        BridgeMessage message = new(BridgeProtocolVersion, "command", line);
        return await sendBridgeMessageAsync(message, expectedRunGeneration);
    }

    public async Task<bool> SetPerformanceMonitoringAsync(
        bool enabled,
        long expectedRunGeneration)
    {
        if (disposed || expectedRunGeneration != RunGeneration)
            return false;
        BridgeMessage message = new(
            BridgeProtocolVersion,
            "control",
            Name: "performanceMonitor",
            Enabled: enabled);
        return await sendBridgeMessageAsync(message, expectedRunGeneration);
    }

    public async Task<bool> SendInputBatchAsync(IReadOnlyList<RuntimeInputEvent> events)
    {
        if (events.Count == 0)
            return true;
        for (int offset = 0; offset < events.Count; offset += MaximumInputEventsPerBatch)
        {
            int count = Math.Min(MaximumInputEventsPerBatch, events.Count - offset);
            RuntimeInputEvent[] batch = new RuntimeInputEvent[count];
            for (int index = 0; index < count; index++)
                batch[index] = events[offset + index];
            BridgeMessage message = new(BridgeProtocolVersion, "input", Events: batch);
            if (!await sendBridgeMessageAsync(message))
                return false;
        }
        return true;
    }

    public Task StopAsync()
    {
        return StopAsync(RunGeneration);
    }

    internal async Task StopAsync(long expectedRunGeneration)
    {
        Process? process;
        CancellationTokenSource? cancellation;
        lock (processLock)
        {
            if (expectedRunGeneration != RunGeneration)
                return;
            process = activeProcess;
            cancellation = runCancellation;
        }
        if (process is null)
        {
            cancelRun(cancellation, expectedRunGeneration);
            return;
        }

        bool sent = State == ProjectRunState.Running
            && await sendBridgeMessageAsync(
                new(BridgeProtocolVersion, "shutdown"),
                expectedRunGeneration);
        if (sent)
        {
            bool exited = false;
            using CancellationTokenSource timeout = new(TimeSpan.FromMilliseconds(200));
            try
            {
                await process.WaitForExitAsync(timeout.Token);
                exited = true;
            }
            catch (OperationCanceledException)
            {
            }
            if (exited)
                return;
        }
        if (expectedRunGeneration != RunGeneration)
            return;
        cancelRun(cancellation, expectedRunGeneration);
        stopProcess(process);
    }

    private void cancelRun(
        CancellationTokenSource? cancellation,
        long expectedRunGeneration)
    {
        lock (processLock)
        {
            if (expectedRunGeneration == RunGeneration
                && ReferenceEquals(runCancellation, cancellation))
            {
                cancellation?.Cancel();
            }
        }
    }

    private async Task<bool> sendBridgeMessageAsync(BridgeMessage message)
    {
        return await sendBridgeMessageAsync(message, null);
    }

    private async Task<bool> sendBridgeMessageAsync(BridgeMessage message, long? expectedRunGeneration)
    {
        byte[] json = JsonSerializer.SerializeToUtf8Bytes(message, bridgeJsonOptions);
        if (json.Length > MaximumBridgeMessageSize)
            return false;
        byte[] payload = new byte[json.Length + 1];
        json.CopyTo(payload, 0);
        payload[^1] = (byte)'\n';
        await commandWriteLock.WaitAsync();
        try
        {
            if (expectedRunGeneration is long generation && generation != RunGeneration)
                return false;
            TcpClient? client;
            lock (processLock)
                client = commandClient;
            if (client is null || !CanSendCommand)
                return false;
            await client.GetStream().WriteAsync(payload.AsMemory());
            return true;
        }
        catch (IOException)
        {
            closeCommandConnection();
            return false;
        }
        catch (SocketException)
        {
            closeCommandConnection();
            return false;
        }
        catch (ObjectDisposedException)
        {
            closeCommandConnection();
            return false;
        }
        finally
        {
            commandWriteLock.Release();
        }
    }

    public void Stop()
    {
        CancellationTokenSource? cancellation;
        Process? process;
        lock (processLock)
        {
            cancellation = runCancellation;
            process = activeProcess;
            cancellation?.Cancel();
        }
        closeCommandConnection();
        if (process is not null)
            stopProcess(process);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        Stop();
    }

    private string? validateProject(bool standalone)
    {
        if (!Directory.Exists(projectPath))
            return projectPath;
        string projectFilePath = Path.Combine(projectPath, "Main.proj");
        if (!File.Exists(projectFilePath))
            return projectFilePath;
        if (!standalone)
        {
            string cmakePath = Path.Combine(projectPath, "CMakeLists.txt");
            if (!File.Exists(cmakePath))
                return cmakePath;
        }
        return null;
    }

    private ProcessStartInfo createBuildStartInfo(string buildScript)
    {
        ProcessStartInfo startInfo = new()
        {
            FileName = OperatingSystem.IsWindows()
                ? Environment.GetEnvironmentVariable("COMSPEC") ?? "cmd.exe"
                : "/bin/bash",
            WorkingDirectory = projectPath,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardOutputEncoding = utf8,
            StandardErrorEncoding = utf8,
            CreateNoWindow = true,
        };
        startInfo.Environment["PYTHONUTF8"] = "1";
        startInfo.Environment["PYTHONIOENCODING"] = "utf-8";
        if (OperatingSystem.IsWindows())
        {
            startInfo.ArgumentList.Add("/d");
            startInfo.ArgumentList.Add("/c");
            startInfo.ArgumentList.Add("call");
        }
        startInfo.ArgumentList.Add(Path.GetFullPath(buildScript));
        startInfo.ArgumentList.Add(projectPath);
        startInfo.ArgumentList.Add("Debug");
        return startInfo;
    }

    private ProcessStartInfo createGameStartInfo(
        string executablePath,
        int commandPort,
        ProjectRunOptions options)
    {
        ProcessStartInfo startInfo = new()
        {
            FileName = executablePath,
            WorkingDirectory = projectPath,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardOutputEncoding = utf8,
            StandardErrorEncoding = utf8,
            CreateNoWindow = true,
        };
        startInfo.Environment.Remove("LUDORK_WINDOW_MODE");
        startInfo.Environment.Remove("LUDORK_WINDOW_HANDLE");
        startInfo.Environment.Remove("INDIVIDUAL");
        startInfo.Environment.Remove("WINDOWHANDLE");
        startInfo.Environment["LUDORK_COMMAND_PORT"] = commandPort.ToString(CultureInfo.InvariantCulture);
        startInfo.Environment["LUDORK_EDITOR"] = "1";
        startInfo.Environment["LUDORK_WINDOW_MODE"] = options.WindowMode == ProjectWindowMode.Individual
            ? "individual"
            : "embedded";
        if (options.WindowMode == ProjectWindowMode.Embedded)
        {
            startInfo.Environment["LUDORK_WINDOW_HANDLE"] = unchecked((nuint)options.WindowHandle)
                .ToString(CultureInfo.InvariantCulture);
        }
        return startInfo;
    }

    private async Task<int> runProcessAsync(ProcessStartInfo startInfo, CancellationToken cancellationToken)
    {
        using Process process = createProcess(startInfo);
        cancellationToken.ThrowIfCancellationRequested();
        if (!process.Start())
            throw new InvalidOperationException(startInfo.FileName);
        setActiveProcess(process);
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();

        try
        {
            await process.WaitForExitAsync(cancellationToken);
            process.WaitForExit();
            return process.ExitCode;
        }
        catch (OperationCanceledException)
        {
            stopProcess(process);
            throw;
        }
        finally
        {
            clearActiveProcess(process);
        }
    }

    private async Task<GameProcessResult> runGameProcessAsync(
        ProcessStartInfo startInfo,
        int commandPort,
        CancellationToken cancellationToken)
    {
        using Process process = createProcess(startInfo);
        using CancellationTokenSource commandCancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        cancellationToken.ThrowIfCancellationRequested();
        if (!process.Start())
            throw new InvalidOperationException(startInfo.FileName);
        setActiveProcess(process);
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();
        TaskCompletionSource<string> protocolFailure = new(TaskCreationOptions.RunContinuationsAsynchronously);
        Task commandTask = maintainCommandConnectionAsync(
            commandPort,
            protocolFailure,
            commandCancellation.Token);

        try
        {
            Task processExitTask = process.WaitForExitAsync(cancellationToken);
            Task completed = await Task.WhenAny(processExitTask, protocolFailure.Task);
            if (ReferenceEquals(completed, protocolFailure.Task))
            {
                string detail = await protocolFailure.Task;
                stopProcess(process);
                await process.WaitForExitAsync();
                process.WaitForExit();
                return new(process.ExitCode, detail);
            }
            await processExitTask;
            process.WaitForExit();
            return new(process.ExitCode, null);
        }
        catch (OperationCanceledException)
        {
            stopProcess(process);
            throw;
        }
        finally
        {
            commandCancellation.Cancel();
            closeCommandConnection();
            await commandTask;
            clearActiveProcess(process);
        }
    }

    private Process createProcess(ProcessStartInfo startInfo)
    {
        Process process = new()
        {
            StartInfo = startInfo,
            EnableRaisingEvents = true,
        };
        process.OutputDataReceived += (_, args) =>
        {
            if (args.Data is not null)
                writeOutput(args.Data);
        };
        process.ErrorDataReceived += (_, args) =>
        {
            if (args.Data is not null)
                writeOutput(args.Data);
        };
        return process;
    }

    private async Task maintainCommandConnectionAsync(
        int port,
        TaskCompletionSource<string> protocolFailure,
        CancellationToken cancellationToken)
    {
        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                TcpClient client = new(AddressFamily.InterNetwork);
                try
                {
                    await client.ConnectAsync(IPAddress.Loopback, port, cancellationToken);
                }
                catch (SocketException)
                {
                    client.Dispose();
                    await Task.Delay(50, cancellationToken);
                    continue;
                }

                client.NoDelay = true;
                NetworkStream stream = client.GetStream();
                ReadyMessageResult readyMessage;
                try
                {
                    readyMessage = await readReadyMessageAsync(stream, cancellationToken);
                }
                catch (IOException)
                {
                    client.Dispose();
                    continue;
                }
                if (readyMessage.Error is not null)
                {
                    client.Dispose();
                    protocolFailure.TrySetResult(readyMessage.Error);
                    return;
                }
                if (readyMessage.Line is null)
                {
                    client.Dispose();
                    continue;
                }
                string? readyError = validateReadyMessage(readyMessage.Line);
                if (readyError is not null)
                {
                    client.Dispose();
                    protocolFailure.TrySetResult(readyError);
                    return;
                }
                setCommandConnection(client);
                try
                {
                    byte[] probe = new byte[1];
                    while (!cancellationToken.IsCancellationRequested)
                    {
                        int read = await stream.ReadAsync(probe.AsMemory(), cancellationToken);
                        if (read == 0)
                            break;
                    }
                }
                catch (IOException)
                {
                }
                catch (SocketException)
                {
                }
                catch (ObjectDisposedException)
                {
                }
                finally
                {
                    clearCommandConnection(client);
                    client.Dispose();
                }

                if (!cancellationToken.IsCancellationRequested)
                    await Task.Delay(50, cancellationToken);
            }
        }
        catch (OperationCanceledException)
        {
        }
    }

    private static async Task<ReadyMessageResult> readReadyMessageAsync(
        NetworkStream stream,
        CancellationToken cancellationToken)
    {
        List<byte> bytes = new();
        byte[] buffer = new byte[1];
        while (true)
        {
            int read = await stream.ReadAsync(buffer.AsMemory(), cancellationToken);
            if (read == 0)
                return new(null, null);
            if (buffer[0] == (byte)'\n')
            {
                int length = bytes.Count;
                if (length > 0 && bytes[^1] == (byte)'\r')
                    length--;
                return new(utf8.GetString(bytes.ToArray(), 0, length), null);
            }
            if (bytes.Count >= MaximumBridgeMessageSize)
                return new(null, "ready message exceeds the size limit");
            bytes.Add(buffer[0]);
        }
    }

    private static string? validateReadyMessage(string line)
    {
        if (utf8.GetByteCount(line) > MaximumBridgeMessageSize)
            return "ready message exceeds the size limit";
        try
        {
            using JsonDocument document = JsonDocument.Parse(line);
            JsonElement root = document.RootElement;
            if (root.ValueKind != JsonValueKind.Object)
                return "ready message must be a JSON object";
            if (!root.TryGetProperty("v", out JsonElement version)
                || version.ValueKind != JsonValueKind.Number
                || !version.TryGetInt32(out int parsedVersion)
                || parsedVersion != BridgeProtocolVersion)
            {
                return $"expected protocol {BridgeProtocolVersion}";
            }
            if (!root.TryGetProperty("type", out JsonElement type)
                || type.ValueKind != JsonValueKind.String
                || type.GetString() != "ready")
            {
                return "runtime did not send a ready message";
            }
            return null;
        }
        catch (JsonException exception)
        {
            return exception.Message;
        }
    }

    private static int reserveCommandPort()
    {
        TcpListener listener = new(IPAddress.Loopback, 0);
        listener.Start();
        int port = ((IPEndPoint)listener.LocalEndpoint).Port;
        listener.Stop();
        return port;
    }

    private void setActiveProcess(Process process)
    {
        lock (processLock)
            activeProcess = process;
    }

    private void clearActiveProcess(Process process)
    {
        lock (processLock)
        {
            if (ReferenceEquals(activeProcess, process))
                activeProcess = null;
        }
    }

    private void setCommandConnection(TcpClient client)
    {
        lock (processLock)
            commandClient = client;
        setCanSendCommand(true);
    }

    private void clearCommandConnection(TcpClient client)
    {
        bool changed = false;
        lock (processLock)
        {
            if (ReferenceEquals(commandClient, client))
            {
                commandClient = null;
                changed = true;
            }
        }
        if (changed)
            setCanSendCommand(false);
    }

    private void closeCommandConnection()
    {
        TcpClient? client;
        lock (processLock)
        {
            client = commandClient;
            commandClient = null;
        }
        client?.Dispose();
        setCanSendCommand(false);
    }

    private void stopProcess(Process process)
    {
        try
        {
            if (!process.HasExited)
                process.Kill(true);
        }
        catch (InvalidOperationException)
        {
        }
        catch (Win32Exception)
        {
        }
    }

    private sealed record BridgeMessage(
        int V,
        string Type,
        string? Command = null,
        IReadOnlyList<RuntimeInputEvent>? Events = null,
        string? Name = null,
        bool? Enabled = null);

    private sealed record GameProcessResult(int ExitCode, string? ProtocolFailure);

    private sealed record ReadyMessageResult(string? Line, string? Error);
}
