using Avalonia.Controls;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Services;
using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

internal static class EditorProjectOpenWorkflow
{
    public static async Task<ProjectDataStore?> LoadAsync(Window owner, string projectPath, CancellationToken token)
    {
        ProjectConfigService config = new(projectPath);
        EditorOperationProgressWindow dialog = new(LocaleService.Get("OPEN_PROJECT"));
        EditorOperationProgressReporter progress = new();
        DispatcherTimer timer = new() { Interval = TimeSpan.FromMilliseconds(100) };
        timer.Tick += (_, _) => dialog.Update(progress.Current);
        dialog.Update(progress.Current);
        _ = dialog.ShowDialog(owner);
        timer.Start();
        try
        {
            if (config.IsStandalone)
                return await loadDataAsync(projectPath, progress, token);
            using ProjectRunnerService runner = new(projectPath);
            bool prepared = !runner.NativeBuildState.HasGeneratedFiles()
                || !await runner.ExportState.HasGeneratedFilesAsync(token);
            if (prepared)
                await prepareAsync(runner, projectPath, progress, token);
            ProjectDataStore data = await loadDataAsync(projectPath, progress, token);
            if (prepared || !data.HasBlueprintSchemaLoadErrors)
                return data;
            string details = string.Join(Environment.NewLine, data.InvalidLoadDetails);
            data.Dispose();
            bool rebuild = await ConfirmationDialog.ShowAsync(dialog,
                LocaleService.Get("PROJECT_OPEN_REBUILD_TITLE"),
                LocaleService.Get("PROJECT_OPEN_REBUILD_CONFIRM"), token, details);
            if (!rebuild)
                return null;
            await prepareAsync(runner, projectPath, progress, token);
            return await loadDataAsync(projectPath, progress, token);
        }
        finally
        {
            timer.Stop();
            dialog.Finish();
        }
    }

    private static async Task<ProjectDataStore> loadDataAsync(string projectPath,
        IProgress<EditorOperationProgress> progress, CancellationToken token)
    {
        progress.Report(new("EDIT_OPERATION_VALIDATING"));
        ProjectDataStore data = await Task.Run(() => new ProjectDataStore(projectPath, loadCancellationToken: token,
            loadProgress: path => progress.Report(new("EDIT_OPERATION_VALIDATING",
                Path.GetRelativePath(projectPath, path)))), token);
        if (token.IsCancellationRequested)
        {
            data.Dispose();
            token.ThrowIfCancellationRequested();
        }
        return data;
    }

    private static async Task prepareAsync(ProjectRunnerService runner, string projectPath,
        IProgress<EditorOperationProgress> progress, CancellationToken token)
    {
        using ConsoleLogSession log = new();
        string? logError = log.Start(projectPath);
        if (logError is not null)
            throw new IOException(logError);
        string stage = "CONSTRUCT";
        void writeOutput(string line)
        {
            log.WriteLine(line);
            progress.Report(new(stage, line.Length > 240 ? line[..240] : line));
        }
        void onOutput(object? sender, string line) => writeOutput(line);
        runner.OutputReceived += onOutput;
        try
        {
            progress.Report(new(stage));
            ProjectRunResult build = await runner.BuildAsync(token);
            token.ThrowIfCancellationRequested();
            if (!build.Success)
                throw preparationError(projectPath, "BUILD_FAILED_TITLE", build.Detail);
            stage = "EXPORT";
            progress.Report(new(stage));
            ProjectExportResult export = await runner.ExportState.ExportAsync(writeOutput, token);
            token.ThrowIfCancellationRequested();
            if (!export.Success)
                throw preparationError(projectPath, "EXPORT_FAILED_TITLE", export.Detail);
        }
        finally
        {
            runner.OutputReceived -= onOutput;
        }
    }

    private static InvalidOperationException preparationError(string projectPath, string title, string detail) =>
        new(LocaleService.Get(title) + Environment.NewLine + detail + Environment.NewLine
            + string.Format(LocaleService.Get("PROJECT_OPEN_PREPARATION_LOG"), Path.Combine(projectPath, "Log", "Ludork.log")));
}
