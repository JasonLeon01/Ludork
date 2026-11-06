using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
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
        cancellationToken.ThrowIfCancellationRequested();
        try
        {
            ProcessStartInfo startInfo = ToolchainProcess.CreateStartInfo(
                ToolchainTool.Assets, ["ui-assets", operation, projectPath], projectPath);
            ToolProcessResult result = await ToolProcessRunner.CaptureAsync(startInfo, cancellationToken);
            string outputText = result.StandardOutput.Trim();
            string errorText = result.StandardError.Trim();
            if (outputText.Length != 0)
                writeOutput?.Invoke(outputText);
            if (errorText.Length != 0)
                writeOutput?.Invoke(errorText);
            return new SaveResult(result.ExitCode == 0,
                result.ExitCode == 0 ? outputText : errorText.Length != 0 ? errorText : outputText);
        }
        catch (Exception exception) when (exception is Win32Exception or IOException or InvalidOperationException)
        {
            return new SaveResult(false, exception.Message);
        }
    }

}
