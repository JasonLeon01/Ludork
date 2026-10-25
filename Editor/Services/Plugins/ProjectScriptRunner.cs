using Avalonia.Threading;
using Ludork.Plugin.Abstractions;
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.Plugins;

internal sealed class ProjectScriptRunner(
    ProjectDataStore gameData,
    ProjectConfigService projectConfig,
    ProjectRunnerService projectRunner)
{
    public async Task<PluginProjectScriptResult> RunAsync(
        string scriptPath, JsonObject request, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (!Path.IsPathFullyQualified(scriptPath) || !File.Exists(scriptPath)
            || Path.GetExtension(scriptPath).ToLowerInvariant() is not ".lua" and not ".luac")
            return failed("The project evaluation script must be an existing absolute Lua file path.");
        if (await hasUnsavedInputsAsync())
            return failed("Save the project's Blueprint, script and data changes before evaluating Actors.");
        if (!projectConfig.IsStandalone && !await projectRunner.NativeBuildState.CheckAsync(cancellationToken))
            return failed("Construct the project before evaluating Actors. " + projectRunner.NativeBuildState.Detail);
        string executableName = OperatingSystem.IsWindows() ? "Main.exe" : "Main";
        string executable = projectConfig.IsStandalone
            ? Path.Combine(gameData.ProjectPath, executableName)
            : Path.Combine(gameData.ProjectPath, "bin", "Debug", executableName);
        if (!File.Exists(executable))
            return failed("The project runtime executable is missing. Construct the project first.");
        string inputRevision = await captureInputRevisionAsync();
        string directory = Path.Combine(Path.GetTempPath(), "LudorkPluginScript-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(directory);
        try
        {
            ProjectScriptInputs inputs = await Task.Run(
                () => ProjectScriptInputs.Capture(gameData.ProjectPath, scriptPath, cancellationToken), cancellationToken);
            string requestPath = Path.Combine(directory, "request.json");
            string resultPath = Path.Combine(directory, "result.json");
            await File.WriteAllTextAsync(requestPath, request.ToJsonString(), new UTF8Encoding(false), cancellationToken);
            ProcessStartInfo startInfo = new()
            {
                FileName = executable,
                WorkingDirectory = gameData.ProjectPath,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                StandardOutputEncoding = Encoding.UTF8,
                StandardErrorEncoding = Encoding.UTF8,
                CreateNoWindow = true,
            };
            startInfo.ArgumentList.Add(scriptPath);
            startInfo.ArgumentList.Add(requestPath);
            startInfo.ArgumentList.Add(resultPath);
            foreach (string name in new[]
            {
                "LUDORK_COMMAND_PORT", "LUDORK_WINDOW_MODE", "LUDORK_WINDOW_HANDLE",
                "LUDORK_LIVE_DEBUG", "INDIVIDUAL", "WINDOWHANDLE",
            })
                startInfo.Environment.Remove(name);
            startInfo.Environment["LUDORK_EDITOR"] = "1";
            using Process process = new() { StartInfo = startInfo };
            if (!process.Start())
                return failed("The project evaluation process could not start.");
            Task<string> output = process.StandardOutput.ReadToEndAsync();
            Task<string> error = process.StandardError.ReadToEndAsync();
            using CancellationTokenSource timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            timeout.CancelAfter(TimeSpan.FromSeconds(60));
            try
            {
                await process.WaitForExitAsync(timeout.Token);
            }
            catch (OperationCanceledException)
            {
                if (!process.HasExited)
                    process.Kill(true);
                await process.WaitForExitAsync(CancellationToken.None);
                await Task.WhenAll(output, error);
                cancellationToken.ThrowIfCancellationRequested();
                return failed("Project evaluation exceeded its 60-second time limit.");
            }
            string standardOutput = await output;
            string standardError = await error;
            if (process.ExitCode != 0)
            {
                string detail = string.IsNullOrWhiteSpace(standardError) ? standardOutput : standardError;
                if (detail.Length > 8192)
                    detail = detail[^8192..];
                return failed("Project evaluation failed: " + detail.Trim());
            }
            if (!File.Exists(resultPath) || new FileInfo(resultPath).Length > 32 * 1024 * 1024)
                return failed("Project evaluation did not return a valid result file.");
            JsonNode? result = JsonNode.Parse(await File.ReadAllTextAsync(resultPath, cancellationToken));
            if (result is not JsonObject data)
                return failed("Project evaluation must return a JSON object.");
            ProjectScriptInputs currentInputs = await Task.Run(
                () => ProjectScriptInputs.Capture(gameData.ProjectPath, scriptPath, cancellationToken), cancellationToken);
            if (await hasUnsavedInputsAsync()
                || !string.Equals(inputRevision, await captureInputRevisionAsync(), StringComparison.Ordinal)
                || !inputs.Matches(currentInputs))
                return failed("Project inputs changed during evaluation. Save the project and evaluate again.");
            return new PluginProjectScriptResult(true, data, string.Empty);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or JsonException or Win32Exception)
        {
            return failed(exception.Message);
        }
        finally
        {
            Directory.Delete(directory, true);
        }
    }

    private async Task<bool> hasUnsavedInputsAsync()
    {
        return await Dispatcher.UIThread.InvokeAsync(() =>
            gameData.Documents.ModifiedDocuments.Any(document => document.Section is not "Maps" and not "WorldMaps"));
    }

    private async Task<string> captureInputRevisionAsync()
    {
        return await Dispatcher.UIThread.InvokeAsync(() => string.Join("\n", gameData.Documents.All
            .Where(document => document.Section is not "Maps" and not "WorldMaps")
            .OrderBy(document => document.Path, StringComparer.Ordinal)
            .Select(document => $"{document.Section}\0{document.Key}\0{document.Path}\0{document.Revision}")));
    }

    private static PluginProjectScriptResult failed(string error) => new(false, null, error);
}
