using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading.Tasks;

namespace Ludork.Services;

public static class UiAssetGenerationService
{
    public static ProcessStartInfo? CreateStartInfo(string projectPath)
    {
        string executable = OperatingSystem.IsWindows() ? "ScriptTools.exe" : "ScriptTools";
        string? toolPath = EditorRuntimePaths.FindFile("tools", executable)
            ?? EditorRuntimePaths.FindFile(".tools", "ScriptTools", executable);
        if (toolPath is null)
            return null;

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
        startInfo.ArgumentList.Add("generate");
        startInfo.ArgumentList.Add(projectPath);
        startInfo.Environment["PYTHONUTF8"] = "1";
        startInfo.Environment["PYTHONIOENCODING"] = "utf-8";
        return startInfo;
    }

    public static SaveResult Generate(string projectPath)
    {
        ProcessStartInfo? startInfo = CreateStartInfo(projectPath);
        if (startInfo is null)
            return new SaveResult(false, "ScriptTools was not found in the editor installation.");

        try
        {
            using Process? process = Process.Start(startInfo);
            if (process is null)
                return new SaveResult(false, "UI asset generation could not be started.");
            Task<string> output = process.StandardOutput.ReadToEndAsync();
            Task<string> error = process.StandardError.ReadToEndAsync();
            process.WaitForExit();
            string outputText = output.GetAwaiter().GetResult().Trim();
            string errorText = error.GetAwaiter().GetResult().Trim();
            if (process.ExitCode == 0)
                return new SaveResult(true, string.Empty);
            return new SaveResult(false, errorText.Length != 0 ? errorText
                : outputText.Length != 0 ? outputText : "UI asset generation failed.");
        }
        catch (Win32Exception exception)
        {
            return new SaveResult(false, exception.Message);
        }
        catch (IOException exception)
        {
            return new SaveResult(false, exception.Message);
        }
    }
}
