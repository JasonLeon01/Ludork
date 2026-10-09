using Avalonia.Controls;
using Ludork.Services;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public static class EditorSaveWorkflow
{
    public static async Task<bool> TrySaveAsync(
        Window owner,
        ProjectSaveService saveService,
        bool beforeNativeBuild = false)
    {
        ProjectSaveAttempt? attempt = await EditorEditWorkflow.RunAsync<ProjectSaveAttempt?>(owner, saveService.GameData,
            LocaleService.Get("EDIT_OPERATION_SAVING"), async progress =>
            {
                await saveService.UiControlRegistry.Runtime.RefreshAsync();
                return await saveService.TrySaveAsync(beforeNativeBuild: beforeNativeBuild, progress: progress);
            }, null);
        if (attempt is null)
            return false;
        if (attempt.UiValidationResults.Any(result => !result.IsValid))
        {
            await EditorFeedback.ShowSaveResultAsync(owner, attempt.Result);
            return false;
        }
        if (attempt.ValidationBlocked)
        {
            bool continueSave = await BlueprintValidationDialog.ShowSaveConfirmationAsync(
                owner,
                attempt.ValidationResults);
            if (!continueSave)
                return false;
            attempt = await EditorEditWorkflow.RunAsync<ProjectSaveAttempt?>(owner, saveService.GameData,
                LocaleService.Get("EDIT_OPERATION_SAVING"), async progress => await saveService.TrySaveAsync(true, beforeNativeBuild, progress), null);
            if (attempt is null)
                return false;
        }
        await EditorFeedback.ShowSaveResultAsync(owner, attempt.Result);
        return attempt.Success;

    }
}
