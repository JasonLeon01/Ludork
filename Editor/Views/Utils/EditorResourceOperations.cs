using Avalonia.Controls;
using Ludork.Services;
using System;
using System.IO;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public static class EditorResourceOperations
{
    public static async Task<bool> DeleteAsync(Window owner, Func<bool> operation)
    {
        try
        {
            return operation();
        }
        catch (Exception exception) when (exception is InvalidOperationException or IOException or UnauthorizedAccessException)
        {
            await AlertDialog.ShowAsync(owner, LocaleService.Get("DELETE_FAILED"), exception.Message);
            return false;
        }
    }
}
