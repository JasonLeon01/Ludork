using Avalonia.Controls;
using Ludork.Controls;
using Ludork.Services;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public static class EditorFeedback
{
    public static Task ShowSaveResultAsync(Window owner, SaveResult result)
    {
        string prefix = LocaleService.Get(result.Success ? "SAVE_SUCCESS" : "SAVE_FAILED");
        string detailTemplate = LocaleService.Get("SAVE_PATH");
        string details = detailTemplate.Contains("{}", System.StringComparison.Ordinal)
            ? detailTemplate.Replace("{}", result.Details, System.StringComparison.Ordinal)
            : detailTemplate + result.Details;
        string message = prefix + details;
        return AlertDialog.ShowAsync(owner, LocaleService.Get("HINT"), message);
    }

    public static void ShowHistory(Toast toast, string action, IReadOnlyList<string> differences)
    {
        if (differences.Count != 0)
            toast.ShowMessage(action + ":\n" + string.Join("\n", differences));
    }
}
