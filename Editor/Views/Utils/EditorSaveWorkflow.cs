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
        bool showSuccess = true,
        bool beforeNativeBuild = false)
    {
        await saveService.UiControlRegistry.Runtime.RefreshAsync();
        ProjectSaveAttempt attempt = saveService.TrySave(beforeNativeBuild: beforeNativeBuild);
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
            attempt = saveService.TrySave(true, beforeNativeBuild);
        }
        if (showSuccess || !attempt.Success)
            await EditorFeedback.ShowSaveResultAsync(owner, attempt.Result);
        return attempt.Success;
    }
}
