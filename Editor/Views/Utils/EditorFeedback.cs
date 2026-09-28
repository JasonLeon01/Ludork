using Avalonia.Controls;
using Ludork.Controls;
using Ludork.Services;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public static class EditorFeedback
{
    public static void ShowHistory(Toast toast, string action, HistoryResult result)
    {
        if (!string.IsNullOrWhiteSpace(result.Message))
            toast.ShowMessage(result.Message);
        else
            ShowHistory(toast, action, result.Changes);
    }

    public static Task ShowSaveResultAsync(Window owner, SaveResult result)
    {
        if (result.Success)
            return Task.CompletedTask;
        string message = LocaleService.Get("SAVE_FAILED");
        if (!string.IsNullOrWhiteSpace(result.Details))
            message += System.Environment.NewLine + result.Details;
        return AlertDialog.ShowAsync(owner, LocaleService.Get("HINT"), message);
    }

    public static void ShowHistory(Toast toast, string action, IReadOnlyList<string> differences)
    {
        if (differences.Count != 0)
            toast.ShowMessage(action + ":\n" + string.Join("\n", differences));
    }
}
