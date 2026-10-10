using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.IO;
using System.Text.Json;

namespace Ludork.Views;

public partial class MainWindow
{
    private TestServerDialog? testServerDialog;

    private async void onTestServerSettings(object? sender, EventArgs args)
    {
        if (viewModel?.CanEdit != true || !viewModel.IsSourceProject)
            return;
        if (testServerDialog is not null)
        {
            testServerDialog.Activate();
            return;
        }
        LudorkServerSettings settings;
        try
        {
            settings = LudorkServerConfiguration.ReadTestSettings(ProjectPath);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or JsonException)
        {
            await AlertDialog.ShowAsync(this, LocaleService.Get("LUDORK_SERVER_TEST_SETTINGS"),
                LocaleService.Get("LUDORK_SERVER_TEST_LOAD_FAILED"));
            return;
        }
        testServerDialog = new TestServerDialog(ProjectPath, settings);
        await testServerDialog.ShowDialog(this);
        testServerDialog = null;
    }
}
