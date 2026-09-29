using Avalonia.Controls;
using Avalonia.Interactivity;
using Ludork.Services;

namespace Ludork.Views;

public partial class AboutDialog : Window
{
    public AboutDialog()
    {
        InitializeComponent();
        Title = LocaleService.Get("ABOUT_TITLE");
        AppNameText.Text = "Ludork";
        VersionText.Text = $"Version {EditorVersionService.FullVersion}";
        Documentation.Navigate(DocumentationService.About);
        CopyrightText.Text = LocaleService.Get("ABOUT_COPYRIGHT");
        LicensesButton.Content = LocaleService.Get("ABOUT_LICENSES");
        CloseButton.Content = LocaleService.Get("CLOSE");
    }

    private void onOpenLicenses(object? sender, RoutedEventArgs args)
    {
        _ = new DocumentationWindow(
            DocumentationService.Notices,
            LocaleService.Get("ABOUT_LICENSES")).ShowDialog(this);
    }

    private void onClose(object? sender, RoutedEventArgs args)
    {
        Close();
    }
}
