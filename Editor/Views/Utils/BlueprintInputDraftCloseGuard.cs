using Avalonia.Controls;
using Ludork.Services;
using System;

namespace Ludork.Views.Utils;

internal static class BlueprintInputDraftCloseGuard
{
    public static void Attach(Window window, Func<bool> hasPendingInputs)
    {
        bool confirmed = false;
        bool confirming = false;
        window.Closing += async (_, args) =>
        {
            if (confirmed || args.CloseReason == WindowCloseReason.OwnerWindowClosing || !hasPendingInputs())
                return;
            args.Cancel = true;
            if (confirming)
                return;
            confirming = true;
            bool discard = await ConfirmationDialog.ShowAsync(window,
                LocaleService.Get("BLUEPRINT_TEXT_DRAFT_TITLE"), LocaleService.Get("BLUEPRINT_TEXT_DISCARD_DRAFT"));
            confirming = false;
            if (discard)
            {
                confirmed = true;
                window.Close();
            }
        };
    }
}
