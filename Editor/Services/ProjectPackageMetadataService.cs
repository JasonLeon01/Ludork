using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed record ProjectPackageMetadataResult(int ExitCode, string Output, string Error);

public static class ProjectPackageMetadataService
{
    public static async Task<ProjectPackageMetadataResult> ExecuteAsync(
        string operation,
        string? projectPath,
        string version,
        bool dev,
        CancellationToken cancellationToken)
    {
        string? toolPath = EditorRuntimePaths.FindScriptTools();
        if (toolPath is null)
            return new(-1, string.Empty, "ScriptTools was not found in the editor installation.");
        ProcessStartInfo startInfo = new()
        {
            FileName = toolPath,
            WorkingDirectory = projectPath ?? Path.GetDirectoryName(toolPath)!,
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8,
        };
        startInfo.ArgumentList.Add("packaging-constants");
        startInfo.ArgumentList.Add(operation);
        if (projectPath is not null)
            startInfo.ArgumentList.Add(projectPath);
        startInfo.ArgumentList.Add("--version");
        startInfo.ArgumentList.Add(version);
        startInfo.ArgumentList.Add(dev ? "--dev" : "--release");
        startInfo.Environment["PYTHONUTF8"] = "1";
        startInfo.Environment["PYTHONIOENCODING"] = "utf-8";
        cancellationToken.ThrowIfCancellationRequested();
        try
        {
            using Process process = new() { StartInfo = startInfo };
            process.Start();
            Task<string> output = process.StandardOutput.ReadToEndAsync();
            Task<string> error = process.StandardError.ReadToEndAsync();
            using CancellationTokenRegistration registration = cancellationToken.Register(() => stopProcess(process));
            await process.WaitForExitAsync().ConfigureAwait(false);
            string outputText = (await output.ConfigureAwait(false)).Trim();
            string errorText = (await error.ConfigureAwait(false)).Trim();
            cancellationToken.ThrowIfCancellationRequested();
            return new(process.ExitCode, outputText, errorText);
        }
        catch (Exception exception) when (exception is Win32Exception or IOException or InvalidOperationException)
        {
            return new(-1, string.Empty, exception.Message);
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
