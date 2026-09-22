using Ludork.Composition;
using Avalonia.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System.IO;
using System.Threading.Tasks;

namespace Ludork.Views.Coordination;

internal sealed class ProjectPackCoordinator(Window owner, EditorProjectSession session, ProjectOperationCoordinator operations)
{
    private PackSelectionDialog? packSelectionDialog;
    private PackLogDialog? packLogDialog;

    public async Task ShowAsync()
    {
        if (!session.MainViewModel.CanEdit)
            return;
        if (packLogDialog is not null)
        {
            packLogDialog.Activate();
            return;
        }
        if (packSelectionDialog is not null)
        {
            packSelectionDialog.Activate();
            return;
        }
        if (!Directory.Exists(session.ProjectPath))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("PACK_TITLE"), LocaleService.Get("PACK_NO_PROJECT"));
            return;
        }
        string projectFilePath = Path.Combine(session.ProjectPath, "Main.proj");
        if (!File.Exists(projectFilePath))
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("PACK_TITLE"), LocaleService.Get("PACK_ENTRY_MISSING"));
            return;
        }
        if (!await EditorSaveWorkflow.TrySaveAsync(
                owner,
                session.ProjectSave,
                false,
                !session.ProjectConfig.IsStandalone))
        {
            return;
        }
        PackSelectionDialog selectionDialog = new(session.ProjectConfig);
        packSelectionDialog = selectionDialog;
        ProjectPackOptions? options = await selectionDialog.ShowDialog<ProjectPackOptions?>(owner);
        packSelectionDialog = null;
        if (options is null)
            return;
        string? scriptName = ProjectPackService.GetScriptName(options.Platform);
        if (scriptName is null || EditorRuntimePaths.FindFile("tools", scriptName) is null)
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("PACK_TITLE"), LocaleService.Get("PACK_SCRIPT_MISSING"));
            return;
        }

        Task? closed = null;
        await operations.PackAsync(async token =>
        {
            PackLogDialog logDialog = new();
            ProjectPackService packService = new(session.ProjectPath,
                (writeOutput, exportToken) => EditorExportWorkflow.ExportAsync(
                    logDialog, session.ProjectSave, session.ProjectRunner.ExportState, writeOutput, exportToken));
            packLogDialog = logDialog;
            packService.OutputReceived += (_, text) => logDialog.AppendLog(text);
            closed = logDialog.ShowDialog(owner);
            ProjectPackResult result = await packService.PackAsync(options, token);
            logDialog.Finish(result);
        });
        if (closed is not null)
            await closed;
        packLogDialog = null;
    }

}
