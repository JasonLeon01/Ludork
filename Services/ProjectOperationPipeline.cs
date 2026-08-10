using Ludork.Plugin.Abstractions;
using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class ProjectOperationPipeline
{
    private readonly IEditorPluginRuntime? runtime;
    private readonly string projectPath;

    public ProjectOperationPipeline(string projectPath, IEditorPluginRuntime? runtime = null)
    {
        this.projectPath = Path.GetFullPath(projectPath);
        this.runtime = runtime ?? TextHintService.Runtime;
    }

    public async Task<PluginResult> ExecuteAsync(
        ProjectOperationKind operation,
        Action<string> writeOutput,
        CancellationToken cancellationToken,
        IProjectPackaging? packaging = null)
    {
        if (runtime is null)
            return PluginResult.Completed();
        ProjectOperationContext context = new(
            projectPath,
            LocaleService.CurrentLanguage,
            operation,
            packaging,
            new DelegatePluginOutput(writeOutput),
            TextHintService.Refresh,
            cancellationToken);
        try
        {
            return await runtime.ExecuteBeforeProjectOperationAsync(context);
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            throw;
        }
        catch (Exception exception)
        {
            writeOutput(exception.Message);
            return PluginResult.Failed(exception.Message);
        }
    }

    private sealed class DelegatePluginOutput : IPluginOutput
    {
        private readonly Action<string> writeOutput;

        public DelegatePluginOutput(Action<string> writeOutput)
        {
            this.writeOutput = writeOutput;
        }

        public void WriteLine(string text)
        {
            writeOutput(text);
        }
    }
}
