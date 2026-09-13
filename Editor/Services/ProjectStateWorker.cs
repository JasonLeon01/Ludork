using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class ProjectStateWorker : IDisposable
{
    private const int ProtocolVersion = 1;
    private readonly string projectPath;
    private readonly SemaphoreSlim requestLock = new(1, 1);
    private readonly CancellationTokenSource lifetime = new();
    private Process? process;
    private Task<string>? errorOutput;
    private string? toolPath;
    private string? toolIdentity;
    private long requestId;
    private volatile bool disposed;

    public ProjectStateWorker(string projectPath)
    {
        this.projectPath = Path.GetFullPath(projectPath);
        _ = warmUpAsync();
    }

    public async Task<(JsonElement Result, string ToolIdentity)> ExecuteAsync(
        string operation, CancellationToken cancellationToken, string? configuration = null)
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        using CancellationTokenSource cancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken, lifetime.Token);
        await requestLock.WaitAsync(cancellation.Token).ConfigureAwait(false);
        try
        {
            string path = EditorRuntimePaths.FindScriptTools()
                ?? throw new IOException("ScriptTools was not found in the editor installation.");
            string identity = await hashToolAsync(path, cancellation.Token).ConfigureAwait(false);
            if (process is null || process.HasExited || toolPath != path || toolIdentity != identity)
            {
                resetProcess();
                startProcess(path, identity, cancellation.Token);
            }
            Process active = process!;
            using CancellationTokenRegistration registration = cancellation.Token.Register(() => stopProcess(active));
            long id = ++requestId;
            string request = JsonSerializer.Serialize(new { version = ProtocolVersion, id, operation, configuration });
            await active.StandardInput.WriteLineAsync(request.AsMemory(), cancellation.Token).ConfigureAwait(false);
            await active.StandardInput.FlushAsync(cancellation.Token).ConfigureAwait(false);
            string? line = await active.StandardOutput.ReadLineAsync(cancellation.Token).ConfigureAwait(false);
            if (line is null)
            {
                string detail = errorOutput is { IsCompletedSuccessfully: true } ? errorOutput.Result.Trim() : string.Empty;
                throw new IOException("Project state checker exited without a response. " + detail);
            }
            using JsonDocument document = JsonDocument.Parse(line);
            JsonElement response = document.RootElement;
            if (response.GetProperty("id").GetInt64() != id)
                throw new InvalidDataException("Project state checker returned an unexpected response.");
            if (response.TryGetProperty("error", out JsonElement error))
                throw new InvalidDataException(error.GetString());
            JsonElement result = response.GetProperty("result").Clone();
            validateResult(operation, result);
            if (await hashToolAsync(path, cancellation.Token).ConfigureAwait(false) != identity)
                throw new IOException("ScriptTools changed during the check. Try the operation again.");
            cancellation.Token.ThrowIfCancellationRequested();
            return (result, identity);
        }
        catch (OperationCanceledException)
        {
            resetProcess();
            throw;
        }
        catch (Exception exception) when (exception is Win32Exception or IOException or UnauthorizedAccessException
            or InvalidDataException or JsonException or InvalidOperationException or FormatException
            or System.Collections.Generic.KeyNotFoundException)
        {
            resetProcess();
            cancellation.Token.ThrowIfCancellationRequested();
            throw new ProjectStateCheckException("Project state check failed: " + exception.Message, exception);
        }
        finally
        {
            requestLock.Release();
        }
    }

    private static void validateResult(string operation, JsonElement result)
    {
        if (result.ValueKind != JsonValueKind.Object)
            throw new InvalidDataException("Project state checker returned an invalid result.");
        if (operation == "native-check")
        {
            result.GetProperty("current").GetBoolean();
            if (result.GetProperty("detail").ValueKind != JsonValueKind.String)
                throw new InvalidDataException("Project state checker returned an invalid native build detail.");
        }
        else if (operation == "ui-manifest")
        {
            foreach (JsonProperty input in result.GetProperty("inputs").EnumerateObject())
            {
                if (input.Value.ValueKind != JsonValueKind.String)
                    throw new InvalidDataException("Project state checker returned an invalid input fingerprint.");
            }
            foreach (JsonElement output in result.GetProperty("outputPaths").EnumerateArray())
            {
                if (output.ValueKind != JsonValueKind.String)
                    throw new InvalidDataException("Project state checker returned an invalid output path.");
            }
        }
    }

    private async Task warmUpAsync()
    {
        try
        {
            await ExecuteAsync("ready", lifetime.Token).ConfigureAwait(false);
        }
        catch (Exception exception) when (exception is OperationCanceledException or ProjectStateCheckException or ObjectDisposedException)
        {
        }
    }

    private void startProcess(string path, string identity, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        UTF8Encoding utf8 = new(false);
        ProcessStartInfo startInfo = new()
        {
            FileName = path,
            WorkingDirectory = projectPath,
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardInputEncoding = utf8,
            StandardOutputEncoding = utf8,
            StandardErrorEncoding = utf8,
        };
        startInfo.ArgumentList.Add("project-state-worker");
        startInfo.ArgumentList.Add(projectPath);
        startInfo.Environment["PYTHONUTF8"] = "1";
        startInfo.Environment["PYTHONIOENCODING"] = "utf-8";
        process = new Process { StartInfo = startInfo };
        process.Start();
        errorOutput = process.StandardError.ReadToEndAsync();
        toolPath = path;
        toolIdentity = identity;
    }

    private static Task<string> hashToolAsync(string path, CancellationToken cancellationToken)
    {
        return Task.Run(async () =>
        {
            string root = Path.GetDirectoryName(path)!;
            string[] files = Directory.GetFiles(root, "*", SearchOption.AllDirectories)
                .OrderBy(file => Path.GetRelativePath(root, file).Replace(Path.DirectorySeparatorChar, '/'), StringComparer.Ordinal)
                .ToArray();
            using IncrementalHash digest = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
            foreach (string file in files)
            {
                cancellationToken.ThrowIfCancellationRequested();
                string relative = Path.GetRelativePath(root, file).Replace(Path.DirectorySeparatorChar, '/');
                digest.AppendData(Encoding.UTF8.GetBytes(relative + "\0"));
                await using FileStream stream = new(file, FileMode.Open, FileAccess.Read, FileShare.Read | FileShare.Delete,
                    65536, FileOptions.Asynchronous | FileOptions.SequentialScan);
                digest.AppendData(await SHA256.HashDataAsync(stream, cancellationToken).ConfigureAwait(false));
            }
            return Convert.ToHexStringLower(digest.GetHashAndReset());
        }, cancellationToken);
    }

    private void resetProcess()
    {
        if (process is not null)
        {
            stopProcess(process);
            process.Dispose();
        }
        process = null;
        errorOutput = null;
        toolPath = null;
        toolIdentity = null;
    }

    private static void stopProcess(Process active)
    {
        try
        {
            if (!active.HasExited)
                active.Kill(true);
        }
        catch (Exception exception) when (exception is Win32Exception or InvalidOperationException)
        {
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        lifetime.Cancel();
        _ = disposeProcessAsync();
    }

    private async Task disposeProcessAsync()
    {
        await requestLock.WaitAsync().ConfigureAwait(false);
        try
        {
            resetProcess();
        }
        finally
        {
            requestLock.Release();
        }
    }
}
