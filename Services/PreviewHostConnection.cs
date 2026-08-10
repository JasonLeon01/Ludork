using Ludork.Models;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public enum PreviewHostConnectionState
{
    Unavailable,
    Starting,
    Ready,
    Faulted,
}

public sealed class PreviewHostConnection : IDisposable, IAsyncDisposable
{
    public const int ProtocolVersion = 4;

    private readonly string projectPath;
    private readonly SemaphoreSlim protocolLock = new(1, 1);
    private readonly object sharedMemorySync = new();
    private readonly HashSet<string> sharedMemoryFiles = new(StringComparer.OrdinalIgnoreCase);
    private Process? process;
    private Stream? input;
    private Stream? output;
    private string? lastStandardError;
    private bool disposed;

    public PreviewHostConnection(string projectPath)
    {
        this.projectPath = Path.GetFullPath(projectPath);
    }

    public event EventHandler? StateChanged;

    public PreviewHostConnectionState State { get; private set; } = PreviewHostConnectionState.Unavailable;
    public string StatusMessage { get; private set; } = string.Empty;
    public bool IsReady => State == PreviewHostConnectionState.Ready;
    public IReadOnlySet<string> Capabilities { get; private set; } = new HashSet<string>(StringComparer.Ordinal);

    public async Task<bool> StartAsync(CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (disposed)
            return false;
        if (process is { HasExited: false } && input is not null && output is not null)
        {
            if (State == PreviewHostConnectionState.Ready)
                return true;
            if (State == PreviewHostConnectionState.Starting)
                return false;
            await stopProcessAsync();
        }
        else if (process is not null || input is not null || output is not null)
        {
            await stopProcessAsync();
        }
        string? hostPath = findHostPath();
        if (hostPath is null)
        {
            setState(PreviewHostConnectionState.Unavailable, "UiPreviewHost was not found.");
            return false;
        }

        setState(PreviewHostConnectionState.Starting, string.Empty);
        lastStandardError = null;
        ProcessStartInfo startInfo = new()
        {
            FileName = hostPath,
            Arguments = "--stdio",
            WorkingDirectory = projectPath,
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        };
        try
        {
            process = Process.Start(startInfo);
        }
        catch (Win32Exception exception)
        {
            setState(PreviewHostConnectionState.Unavailable, exception.Message);
            return false;
        }
        catch (InvalidOperationException exception)
        {
            setState(PreviewHostConnectionState.Unavailable, exception.Message);
            return false;
        }
        if (process is null)
        {
            setState(PreviewHostConnectionState.Unavailable, "UiPreviewHost could not be started.");
            return false;
        }
        process.EnableRaisingEvents = true;
        process.Exited += onProcessExited;
        process.ErrorDataReceived += onErrorDataReceived;
        process.BeginErrorReadLine();
        input = process.StandardInput.BaseStream;
        output = process.StandardOutput.BaseStream;

        JsonObject request = new()
        {
            ["type"] = "handshake",
            ["protocolVersion"] = ProtocolVersion,
            ["adapterFingerprint"] = UiControlRegistryService.AdapterFingerprint,
            ["projectPath"] = projectPath,
        };
        try
        {
            JsonObject response = await ExchangeAsync(request, cancellationToken);
            if (!string.Equals(getString(response, "type"), "handshake", StringComparison.Ordinal)
                || response["accepted"]?.GetValue<bool>() != true
                || response["protocolVersion"]?.GetValue<int>() != ProtocolVersion
                || !string.Equals(
                    getString(response, "adapterFingerprint"),
                    UiControlRegistryService.AdapterFingerprint,
                    StringComparison.Ordinal))
            {
                await stopProcessAsync();
                setState(
                    PreviewHostConnectionState.Faulted,
                    getString(response, "message", "UiPreviewHost protocol is incompatible."));
                return false;
            }
            Capabilities = response["capabilities"] is JsonArray capabilityData
                ? capabilityData.OfType<JsonValue>()
                    .Select(value => value.TryGetValue(out string? text) ? text : null)
                    .Where(value => !string.IsNullOrWhiteSpace(value))
                    .Select(value => value!)
                    .ToHashSet(StringComparer.Ordinal)
                : new HashSet<string>(StringComparer.Ordinal);
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            await stopProcessAsync();
            setState(PreviewHostConnectionState.Unavailable, string.Empty);
            throw;
        }
        catch (Exception exception) when (IsProtocolException(exception))
        {
            await stopProcessAsync();
            setState(PreviewHostConnectionState.Faulted, exception.Message);
            return false;
        }
        setState(PreviewHostConnectionState.Ready, string.Empty);
        return true;
    }

    public async Task<JsonObject> ExchangeAsync(
        JsonObject request,
        CancellationToken cancellationToken = default)
    {
        await protocolLock.WaitAsync(cancellationToken);
        try
        {
            if (input is null || output is null)
                throw new IOException("UiPreviewHost is not connected.");
            byte[] payload = Encoding.UTF8.GetBytes(request.ToJsonString());
            byte[] length = new byte[sizeof(int)];
            BinaryPrimitives.WriteInt32LittleEndian(length, payload.Length);
            await input.WriteAsync(length, cancellationToken);
            await input.WriteAsync(payload, cancellationToken);
            await input.FlushAsync(cancellationToken);

            await readExactlyAsync(output, length, cancellationToken);
            int responseLength = BinaryPrimitives.ReadInt32LittleEndian(length);
            if (responseLength <= 0 || responseLength > 64 * 1024 * 1024)
                throw new InvalidDataException("UiPreviewHost returned an invalid message length.");
            byte[] responsePayload = new byte[responseLength];
            await readExactlyAsync(output, responsePayload, cancellationToken);
            return JsonNode.Parse(responsePayload) as JsonObject
                ?? throw new InvalidDataException("UiPreviewHost returned invalid JSON.");
        }
        catch (Exception exception) when (IsProtocolException(exception))
        {
            setState(PreviewHostConnectionState.Faulted, exception.Message);
            throw;
        }
        finally
        {
            protocolLock.Release();
        }
    }

    public byte[] ReadPixels(JsonObject response, int byteLength)
    {
        string base64 = getString(response, "pixels");
        if (base64.Length != 0)
        {
            byte[] pixels = Convert.FromBase64String(base64);
            if (pixels.Length != byteLength)
                throw new InvalidDataException("UiPreviewHost returned a truncated frame.");
            return pixels;
        }
        if (response["sharedMemory"] is not JsonObject sharedMemory)
            throw new InvalidDataException("UiPreviewHost did not return frame pixels.");
        string filePath = getString(sharedMemory, "filePath");
        long offset = sharedMemory["offset"]?.GetValue<long>() ?? 0;
        if (offset < 0 || byteLength < 0)
            throw new InvalidDataException("UiPreviewHost returned invalid shared memory bounds.");
        if (filePath.Length != 0)
        {
            string fullPath = validateSharedMemoryPath(filePath);
            FileInfo info = new(fullPath);
            if (!info.Exists || offset > info.Length || byteLength > info.Length - offset)
                throw new InvalidDataException("UiPreviewHost returned truncated shared memory.");
            using MemoryMappedFile fileBackedMapping = MemoryMappedFile.CreateFromFile(
                fullPath,
                FileMode.Open,
                null,
                0,
                MemoryMappedFileAccess.Read);
            using MemoryMappedViewAccessor fileBackedView = fileBackedMapping.CreateViewAccessor(
                offset,
                byteLength,
                MemoryMappedFileAccess.Read);
            byte[] fileBackedResult = new byte[byteLength];
            fileBackedView.ReadArray(0, fileBackedResult, 0, fileBackedResult.Length);
            lock (sharedMemorySync)
                sharedMemoryFiles.Add(fullPath);
            return fileBackedResult;
        }
        string name = getString(sharedMemory, "name");
        if (name.Length == 0)
            throw new InvalidDataException("UiPreviewHost returned an invalid shared memory name.");
        if (!OperatingSystem.IsWindows())
            throw new InvalidDataException("UiPreviewHost must return file-backed shared memory on this platform.");
        using MemoryMappedFile file = MemoryMappedFile.OpenExisting(name, MemoryMappedFileRights.Read);
        using MemoryMappedViewAccessor view = file.CreateViewAccessor(offset, byteLength, MemoryMappedFileAccess.Read);
        byte[] result = new byte[byteLength];
        view.ReadArray(0, result, 0, result.Length);
        return result;
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        Stream? currentInput = input;
        Process? currentProcess = process;
        input = null;
        output = null;
        process = null;
        currentInput?.Dispose();
        if (currentProcess is not null)
        {
            currentProcess.Exited -= onProcessExited;
            currentProcess.ErrorDataReceived -= onErrorDataReceived;
            if (!currentProcess.HasExited)
                currentProcess.Kill(true);
            currentProcess.WaitForExit();
            currentProcess.Dispose();
        }
        cleanupSharedMemoryFiles();
        Capabilities = new HashSet<string>(StringComparer.Ordinal);
        setState(PreviewHostConnectionState.Unavailable, string.Empty);
    }

    public ValueTask DisposeAsync()
    {
        Dispose();
        return ValueTask.CompletedTask;
    }

    public static bool IsProtocolException(Exception exception)
    {
        return exception is IOException
            or InvalidDataException
            or JsonException
            or FormatException
            or OverflowException
            or EndOfStreamException;
    }

    private static async Task readExactlyAsync(
        Stream stream,
        Memory<byte> target,
        CancellationToken cancellationToken)
    {
        int offset = 0;
        while (offset < target.Length)
        {
            int count = await stream.ReadAsync(target[offset..], cancellationToken);
            if (count == 0)
                throw new EndOfStreamException("UiPreviewHost closed the protocol stream.");
            offset += count;
        }
    }

    private async Task stopProcessAsync()
    {
        Stream? currentInput = input;
        Process? currentProcess = process;
        input = null;
        output = null;
        process = null;
        if (currentInput is not null)
            await currentInput.DisposeAsync();
        if (currentProcess is null)
        {
            cleanupSharedMemoryFiles();
            return;
        }
        currentProcess.Exited -= onProcessExited;
        currentProcess.ErrorDataReceived -= onErrorDataReceived;
        if (!currentProcess.HasExited)
            currentProcess.Kill(true);
        currentProcess.WaitForExit();
        currentProcess.Dispose();
        cleanupSharedMemoryFiles();
    }

    private void onProcessExited(object? sender, EventArgs args)
    {
        string message = string.IsNullOrWhiteSpace(lastStandardError)
            ? "UiPreviewHost exited."
            : lastStandardError;
        setState(PreviewHostConnectionState.Faulted, message);
        cleanupSharedMemoryFiles();
    }

    private void onErrorDataReceived(object sender, DataReceivedEventArgs args)
    {
        if (!string.IsNullOrWhiteSpace(args.Data))
            lastStandardError = args.Data;
    }

    private void setState(PreviewHostConnectionState state, string message)
    {
        if (disposed && state != PreviewHostConnectionState.Unavailable)
            return;
        State = state;
        StatusMessage = message;
        StateChanged?.Invoke(this, EventArgs.Empty);
    }

    private static string? findHostPath()
    {
        string fileName = OperatingSystem.IsWindows() ? "UiPreviewHost.exe" : "UiPreviewHost";
        string packagedPath = Path.GetFullPath(Path.Combine(
            EditorRuntimePaths.ContentRoot,
            "tools",
            "UiPreviewHost",
            fileName));
        if (File.Exists(packagedPath))
            return packagedPath;
        string developmentOutput = Path.Combine(
            EditorRuntimePaths.DevelopmentRoot,
            ".tools",
            "UiPreviewHost",
            "bin");
        string debugPath = Path.GetFullPath(Path.Combine(
            developmentOutput,
            "Debug",
            fileName));
        if (File.Exists(debugPath))
            return debugPath;
        string releasePath = Path.GetFullPath(Path.Combine(
            developmentOutput,
            "Release",
            fileName));
        return File.Exists(releasePath) ? releasePath : null;
    }

    private static string getString(JsonObject value, string propertyName, string fallback = "")
    {
        return value[propertyName]?.GetValue<string>() ?? fallback;
    }

    private static string validateSharedMemoryPath(string value)
    {
        string path = Path.GetFullPath(value);
        string? directory = Path.GetDirectoryName(path);
        string temporary = Path.GetFullPath(Path.GetTempPath()).TrimEnd(Path.DirectorySeparatorChar);
        StringComparison comparison = OperatingSystem.IsWindows()
            ? StringComparison.OrdinalIgnoreCase
            : StringComparison.Ordinal;
        string name = Path.GetFileName(path);
        if (!string.Equals(directory?.TrimEnd(Path.DirectorySeparatorChar), temporary, comparison)
            || !name.StartsWith("LudorkUiPreview-", StringComparison.Ordinal)
            || !name.EndsWith(".bin", StringComparison.Ordinal))
        {
            throw new InvalidDataException("UiPreviewHost returned an invalid shared memory path.");
        }
        return path;
    }

    private void cleanupSharedMemoryFiles()
    {
        string[] paths;
        lock (sharedMemorySync)
        {
            paths = sharedMemoryFiles.ToArray();
            sharedMemoryFiles.Clear();
        }
        foreach (string path in paths)
        {
            try
            {
                File.Delete(path);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }
}
