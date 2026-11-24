using Avalonia.Controls;
using Ludork.Services;
using System;
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
        ProjectSaveAttempt? attempt = await trySaveAsync(false);
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
            attempt = await trySaveAsync(true);
            if (attempt is null)
                return false;
        }
        await EditorFeedback.ShowSaveResultAsync(owner, attempt.Result);
        return attempt.Success;

        async Task<ProjectSaveAttempt?> trySaveAsync(bool allowInvalidBlueprints)
        {
            Func<Task<bool>>? check = saveService.ExternalChangeCheck;
            bool sessionCheck = check is not null && check == saveService.SessionExternalChangeCheck;
            if (check is not null && !await check())
                return null;
            return await EditorEditWorkflow.RunAsync<ProjectSaveAttempt?>(owner, saveService.GameData,
                LocaleService.Get("EDIT_OPERATION_SAVING"), async progress =>
                {
                    if (!allowInvalidBlueprints)
                        await saveService.UiControlRegistry.Runtime.RefreshAsync();
                    return await saveService.TrySaveAsync(allowInvalidBlueprints, beforeNativeBuild, progress,
                        externalChangesChecked: sessionCheck);
                }, null);
        }
    }
}
