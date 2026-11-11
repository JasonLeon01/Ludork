using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed record ProjectPackageMetadataResult(int ExitCode, string Output, string Error);

public static class ProjectPackageMetadataService
{
    public static Task<ProjectPackageMetadataResult> ReadAppNameAsync(
        string projectPath,
        CancellationToken cancellationToken)
    {
        return executeAsync("app-name", projectPath, null, false, cancellationToken);
    }

    public static Task<ProjectPackageMetadataResult> ExecuteAsync(
        string operation,
        string? projectPath,
        string version,
        bool dev,
        CancellationToken cancellationToken)
    {
        return executeAsync(operation, projectPath, version, dev, cancellationToken);
    }

    private static async Task<ProjectPackageMetadataResult> executeAsync(
        string operation,
        string? projectPath,
        string? version,
        bool dev,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        try
        {
            ProcessStartInfo startInfo = ToolchainProcess.CreateStartInfo(
                ToolchainTool.Pack, ["metadata", operation], projectPath);
            if (projectPath is not null)
                startInfo.ArgumentList.Add(projectPath);
            if (version is not null)
            {
                startInfo.ArgumentList.Add("--version");
                startInfo.ArgumentList.Add(version);
                startInfo.ArgumentList.Add(dev ? "--dev" : "--release");
            }
            ToolProcessResult result = await ToolProcessRunner.CaptureAsync(startInfo, cancellationToken).ConfigureAwait(false);
            string outputText = operation == "app-name"
                ? result.StandardOutput.TrimEnd('\r', '\n') : result.StandardOutput.Trim();
            return new(result.ExitCode, outputText, result.StandardError.Trim());
        }
        catch (Exception exception) when (exception is Win32Exception or IOException or InvalidOperationException)
        {
            return new(-1, string.Empty, exception.Message);
        }
    }
}
