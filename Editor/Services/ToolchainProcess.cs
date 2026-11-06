using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;

namespace Ludork.Services;

internal static class ToolchainProcess
{
    public static ProcessStartInfo CreateStartInfo(
        ToolchainTool role, IEnumerable<string> arguments, string? workingDirectory = null)
    {
        string tool = EditorRuntimePaths.FindToolchainTool(role)
            ?? throw new FileNotFoundException("Toolchain was not found in the editor installation.");
        ProcessStartInfo startInfo = new()
        {
            FileName = tool,
            WorkingDirectory = workingDirectory ?? Path.GetDirectoryName(tool)!,
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8,
        };
        foreach (string argument in arguments)
            startInfo.ArgumentList.Add(argument);
        startInfo.Environment["PYTHONUTF8"] = "1";
        startInfo.Environment["PYTHONIOENCODING"] = "utf-8";
        return startInfo;
    }
}
