using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public static class UiAssetGenerationService
{
    public static async Task<SaveResult> ExecuteAsync(
        string projectPath,
        string operation,
        Action<string>? writeOutput,
        CancellationToken cancellationToken)
    {
        string? toolPath = EditorRuntimePaths.FindScriptTools();
        if (toolPath is null)
            return new SaveResult(false, "ScriptTools was not found in the editor installation.");
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
        startInfo.ArgumentList.Add("ui-assets");
        startInfo.ArgumentList.Add(operation);
        startInfo.ArgumentList.Add(projectPath);
        startInfo.Environment["PYTHONUTF8"] = "1";
        startInfo.Environment["PYTHONIOENCODING"] = "utf-8";
        cancellationToken.ThrowIfCancellationRequested();
        try
        {
            using Process process = new() { StartInfo = startInfo };
            process.Start();
            Task<string> output = process.StandardOutput.ReadToEndAsync(cancellationToken);
            Task<string> error = process.StandardError.ReadToEndAsync(cancellationToken);
            using CancellationTokenRegistration registration = cancellationToken.Register(() => stopProcess(process));
            await process.WaitForExitAsync(cancellationToken);
            string outputText = (await output).Trim();
            string errorText = (await error).Trim();
            if (outputText.Length != 0)
                writeOutput?.Invoke(outputText);
            if (errorText.Length != 0)
                writeOutput?.Invoke(errorText);
            return new SaveResult(process.ExitCode == 0,
                process.ExitCode == 0 ? outputText : errorText.Length != 0 ? errorText : outputText);
        }
        catch (Exception exception) when (exception is Win32Exception or IOException or InvalidOperationException)
        {
            return new SaveResult(false, exception.Message);
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
