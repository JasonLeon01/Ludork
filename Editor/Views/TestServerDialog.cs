using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.IO;

namespace Ludork.Views;

public sealed class TestServerDialog : Window
{
    private readonly LudorkServerInputs inputs = new();
    private readonly TextBlock error = new()
    {
        IsVisible = false,
        Foreground = EditorTheme.Brush("Error"),
        TextWrapping = TextWrapping.Wrap,
    };
    private readonly Button confirm = new() { Content = LocaleService.Get("CONFIRM") };
    private readonly Button cancel = new() { Content = LocaleService.Get("CANCEL") };
    private bool saving;

    public TestServerDialog(string projectPath, LudorkServerSettings settings)
    {
        Title = LocaleService.Get("LUDORK_SERVER_TEST_SETTINGS");
        Width = 420;
        SizeToContent = SizeToContent.Height;
        CanResize = false;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = EditorTheme.Brush("Background");
        FontFamily = EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);
        inputs.SetSettings(settings);
        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { confirm, cancel },
        };
        Content = new StackPanel
        {
            Margin = new Thickness(24),
            Spacing = 16,
            Children = { inputs, error, buttons },
        };
        confirm.Click += async (_, _) =>
        {
            if (!inputs.TryGetSettings(out LudorkServerSettings value))
                return;
            saving = true;
            inputs.IsEnabled = false;
            confirm.IsEnabled = false;
            cancel.IsEnabled = false;
            try
            {
                await LudorkServerConfiguration.SaveTestSettingsAsync(projectPath, value);
                saving = false;
                Close();
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                error.Text = LocaleService.Get("LUDORK_SERVER_TEST_SAVE_FAILED");
                error.IsVisible = true;
            }
            finally
            {
                saving = false;
                inputs.IsEnabled = true;
                confirm.IsEnabled = true;
                cancel.IsEnabled = true;
            }
        };
        cancel.Click += (_, _) => Close();
        Closing += (_, args) => args.Cancel = saving;
    }
}
