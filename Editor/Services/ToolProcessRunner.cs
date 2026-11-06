using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.ExceptionServices;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal static class ToolProcessRunner
{
    public static Task<ToolProcessResult> CaptureAsync(
        ProcessStartInfo startInfo, CancellationToken cancellationToken, TimeSpan? timeout = null)
    {
        return executeAsync(startInfo, null, null, null, cancellationToken, timeout, null, null);
    }

    public static Task<ToolProcessResult> StreamAsync(
        ProcessStartInfo startInfo,
        Func<StreamWriter, CancellationToken, Task>? writeInput,
        Action<string> writeOutput,
        Action<string> writeError,
        CancellationToken cancellationToken,
        Action<Process>? started = null,
        Action<Process>? finished = null)
    {
        return executeAsync(startInfo, writeInput, writeOutput, writeError, cancellationToken, null, started, finished);
    }

    private static async Task<ToolProcessResult> executeAsync(
        ProcessStartInfo startInfo,
        Func<StreamWriter, CancellationToken, Task>? writeInput,
        Action<string>? writeOutput,
        Action<string>? writeError,
        CancellationToken cancellationToken,
        TimeSpan? timeout,
        Action<Process>? started,
        Action<Process>? finished)
    {
        cancellationToken.ThrowIfCancellationRequested();
        using CancellationTokenSource cancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        if (timeout is not null)
            cancellation.CancelAfter(timeout.Value);
        using Process process = new() { StartInfo = startInfo };
        if (!process.Start())
            throw new InvalidOperationException("The tool process could not start: " + startInfo.FileName);
        Task<string> output = readAsync(process, process.StandardOutput, writeOutput);
        Task<string> error = readAsync(process, process.StandardError, writeError);
        Task input = Task.CompletedTask;
        Exception? failure = null;
        using CancellationTokenRegistration registration = cancellation.Token.Register(() => stopProcess(process));
        try
        {
            started?.Invoke(process);
            if (startInfo.RedirectStandardInput)
                input = writeAsync(process, writeInput, cancellation.Token);
            await Task.WhenAll(process.WaitForExitAsync(), output, error, input).ConfigureAwait(false);
        }
        catch (Exception exception)
        {
            failure = exception;
        }
        finally
        {
            stopProcess(process);
            await process.WaitForExitAsync(CancellationToken.None).ConfigureAwait(false);
            try
            {
                await Task.WhenAll(output, error, input).ConfigureAwait(false);
            }
            catch (Exception exception)
            {
                failure ??= exception;
            }
            finished?.Invoke(process);
        }
        cancellationToken.ThrowIfCancellationRequested();
        if (cancellation.IsCancellationRequested)
            throw new TimeoutException("The tool process exceeded its time limit.", failure);
        if (failure is not null)
            ExceptionDispatchInfo.Capture(failure).Throw();
        return new(process.ExitCode, output.Result, error.Result);
    }

    private static async Task<string> readAsync(Process process, StreamReader reader, Action<string>? writeOutput)
    {
        try
        {
            if (writeOutput is null)
                return await reader.ReadToEndAsync().ConfigureAwait(false);
            while (await reader.ReadLineAsync().ConfigureAwait(false) is string line)
                writeOutput(line);
            return string.Empty;
        }
        catch
        {
            stopProcess(process);
            throw;
        }
    }

    private static async Task writeAsync(
        Process process, Func<StreamWriter, CancellationToken, Task>? writeInput, CancellationToken cancellationToken)
    {
        try
        {
            process.StandardInput.NewLine = "\n";
            if (writeInput is not null)
                await writeInput(process.StandardInput, cancellationToken).ConfigureAwait(false);
            await process.StandardInput.FlushAsync(cancellationToken).ConfigureAwait(false);
            process.StandardInput.Close();
        }
        catch
        {
            stopProcess(process);
            throw;
        }
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
}
