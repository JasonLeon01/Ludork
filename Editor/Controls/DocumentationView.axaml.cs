using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform;
using Avalonia.Threading;
using Ludork.Services;
using System;
using System.IO;

namespace Ludork.Controls;

public partial class DocumentationView : UserControl
{
    private readonly DispatcherTimer loadTimeout = new() { Interval = TimeSpan.FromSeconds(30) };
    private NativeWebView? browser;
    private Uri? currentUri;
    private bool attached;

    public DocumentationView()
    {
        InitializeComponent();
        LoadingText.Text = LocaleService.Get("LOADING");
        RetryButton.Content = LocaleService.Get("RETRY");
        InstallButton.Content = LocaleService.Get("DOCUMENTATION_INSTALL_WEBVIEW2");
        loadTimeout.Tick += (_, _) => showError(false);
    }

    public void Navigate(Uri uri)
    {
        currentUri = DocumentationService.WithEmbeddedMode(uri);
        if (attached)
            loadPage();
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        attached = true;
        if (currentUri is not null)
            loadPage();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        attached = false;
        releaseBrowser();
        base.OnDetachedFromVisualTree(args);
    }

    private void loadPage()
    {
        releaseBrowser();
        if (currentUri is null)
            return;
        if (OperatingSystem.IsWindows() && !WebViewAdapterInfo.GetAdapterInfo(WebViewAdapterType.WebView2).IsInstalled)
        {
            showError(true);
            return;
        }
        ErrorPanel.IsVisible = false;
        LoadingText.IsVisible = true;
        browser = new NativeWebView();
        browser.EnvironmentRequested += onEnvironmentRequested;
        browser.NavigationStarted += onNavigationStarted;
        browser.NavigationCompleted += onNavigationCompleted;
        browser.NewWindowRequested += onNewWindowRequested;
        browser.Source = currentUri;
        loadTimeout.Start();
        BrowserHost.Content = browser;
    }

    private void releaseBrowser()
    {
        loadTimeout.Stop();
        if (browser is null)
            return;
        browser.EnvironmentRequested -= onEnvironmentRequested;
        browser.NavigationStarted -= onNavigationStarted;
        browser.NavigationCompleted -= onNavigationCompleted;
        browser.NewWindowRequested -= onNewWindowRequested;
        BrowserHost.Content = null;
        browser = null;
    }

    private static void onEnvironmentRequested(object? sender, WebViewEnvironmentRequestedEventArgs args)
    {
        if (args is WindowsWebView2EnvironmentRequestedEventArgs windows)
            windows.UserDataFolder = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "Ludork", "WebView2");
    }

    private void onNavigationStarted(object? sender, WebViewNavigationStartingEventArgs args)
    {
        if (args.Request is not Uri uri || uri.AbsoluteUri == "about:blank")
            return;
        if (!DocumentationService.IsEmbeddedPage(uri))
        {
            args.Cancel = true;
            openExternal(uri);
            return;
        }
        Uri embeddedUri = DocumentationService.WithEmbeddedMode(uri);
        if (embeddedUri != uri)
        {
            args.Cancel = true;
            Dispatcher.UIThread.Post(() => browser?.Navigate(embeddedUri));
            return;
        }
        currentUri = uri;
        ErrorPanel.IsVisible = false;
        LoadingText.IsVisible = true;
        loadTimeout.Stop();
        loadTimeout.Start();
    }

    private void onNavigationCompleted(object? sender, WebViewNavigationCompletedEventArgs args)
    {
        if (!ReferenceEquals(sender, browser) || args.Request != currentUri)
            return;
        loadTimeout.Stop();
        LoadingText.IsVisible = false;
        if (!args.IsSuccess)
            Dispatcher.UIThread.Post(() =>
            {
                if (ReferenceEquals(sender, browser) && args.Request == currentUri)
                    showError(false);
            });
    }

    private void onNewWindowRequested(object? sender, WebViewNewWindowRequestedEventArgs args)
    {
        args.Handled = true;
        if (args.Request is not Uri uri)
            return;
        if (DocumentationService.IsEmbeddedPage(uri))
            browser?.Navigate(DocumentationService.WithEmbeddedMode(uri));
        else
            openExternal(uri);
    }

    private void showError(bool missingRuntime)
    {
        releaseBrowser();
        LoadingText.IsVisible = false;
        ErrorText.Text = LocaleService.Get(missingRuntime ? "DOCUMENTATION_WEBVIEW2_REQUIRED" : "DOCUMENTATION_LOAD_FAILED");
        InstallButton.IsVisible = missingRuntime;
        ErrorPanel.IsVisible = true;
    }

    private async void openExternal(Uri uri)
    {
        if (uri.Scheme is "https" or "http" or "mailto"
            && TopLevel.GetTopLevel(this) is TopLevel topLevel)
            await topLevel.Launcher.LaunchUriAsync(uri);
    }

    private void onRetry(object? sender, RoutedEventArgs args) => loadPage();

    private void onInstallRuntime(object? sender, RoutedEventArgs args)
        => openExternal(new Uri("https://developer.microsoft.com/microsoft-edge/webview2/"));
}
