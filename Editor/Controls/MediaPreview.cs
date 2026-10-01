using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform;
using Avalonia.Threading;
using Ludork.Services;
using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace Ludork.Controls;

public sealed class MediaPreview : UserControl
{
    private readonly TextBlock status = new()
    {
        TextWrapping = TextWrapping.Wrap,
        HorizontalAlignment = HorizontalAlignment.Center,
        VerticalAlignment = VerticalAlignment.Center,
        Margin = new Thickness(12),
    };
    private readonly DispatcherTimer timeout = new() { Interval = TimeSpan.FromSeconds(30) };
    private NativeWebView? browser;
    private MediaPreviewSource? source;
    private string? path;
    private CancellationTokenSource? loading;

    public event EventHandler? CloseRequested;

    public MediaPreview()
    {
        timeout.Tick += (_, _) => showError(LocaleService.Get("MEDIA_PREVIEW_FAILED"));
    }

    public static bool CanPreview(string path) => MediaFileTypes.GetContentType(path) is not null;

    public void Load(string filePath)
    {
        Clear();
        path = filePath;
        if (VisualRoot is not null)
            loadMedia();
    }

    public void Clear()
    {
        path = null;
        releaseMedia();
        Content = null;
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        if (path is not null)
            loadMedia();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        releaseMedia();
        base.OnDetachedFromVisualTree(args);
    }

    private async void loadMedia()
    {
        releaseMedia();
        if (path is null)
            return;
        if (OperatingSystem.IsWindows() && !WebViewAdapterInfo.GetAdapterInfo(WebViewAdapterType.WebView2).IsInstalled)
        {
            showError(LocaleService.Get("MEDIA_PREVIEW_WEBVIEW2_REQUIRED"));
            return;
        }
        CancellationTokenSource request = new();
        loading = request;
        status.Text = LocaleService.Get("LOADING");
        Content = status;
        try
        {
            string tag = MediaFileTypes.GetContentType(path)?.StartsWith("audio/", StringComparison.Ordinal) == true
                ? "audio" : "video";
            using StreamReader reader = new(AssetLoader.Open(new Uri("avares://Ludork/Editor/Assets/MediaPreview.html")));
            string html = reader.ReadToEnd()
                .Replace("{{tag}}", tag)
                .Replace("{{name}}", WebUtility.HtmlEncode(Path.GetFileName(path)))
                .Replace("{{error}}", WebUtility.HtmlEncode(LocaleService.Get("MEDIA_PREVIEW_FAILED")))
                .Replace("{{loading}}", WebUtility.HtmlEncode(LocaleService.Get("LOADING")));
            MediaPreviewSource loaded = await MediaPreviewSource.CreateAsync(path, html, request.Token);
            if (request.IsCancellationRequested)
            {
                loaded.Dispose();
                return;
            }
            source = loaded;
            browser = new NativeWebView();
            browser.EnvironmentRequested += onEnvironmentRequested;
            browser.NavigationStarted += onNavigationStarted;
            browser.NavigationCompleted += onNavigationCompleted;
            browser.NewWindowRequested += onNewWindowRequested;
            browser.WebMessageReceived += onWebMessageReceived;
            browser.Source = source.Address;
            Content = browser;
            timeout.Start();
        }
        catch (OperationCanceledException) when (request.IsCancellationRequested)
        {
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or SocketException)
        {
            if (!request.IsCancellationRequested)
                showError(LocaleService.Get("MEDIA_PREVIEW_FAILED") + Environment.NewLine + exception.Message);
        }
    }

    private static void onEnvironmentRequested(object? sender, WebViewEnvironmentRequestedEventArgs args)
    {
        if (args is WindowsWebView2EnvironmentRequestedEventArgs windows)
            windows.UserDataFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "Ludork", "WebView2");
    }

    private void onNavigationStarted(object? sender, WebViewNavigationStartingEventArgs args)
    {
        if (args.Request is Uri uri && uri != source?.Address && uri.AbsoluteUri != "about:blank")
            args.Cancel = true;
    }

    private void onNavigationCompleted(object? sender, WebViewNavigationCompletedEventArgs args)
    {
        if (!ReferenceEquals(sender, browser) || args.Request != source?.Address)
            return;
        timeout.Stop();
        if (!args.IsSuccess)
            Dispatcher.UIThread.Post(() =>
            {
                if (ReferenceEquals(sender, browser))
                    showError(LocaleService.Get("MEDIA_PREVIEW_FAILED"));
            });
    }

    private void onWebMessageReceived(object? sender, WebMessageReceivedEventArgs args)
    {
        if (ReferenceEquals(sender, browser) && args.Body == "close")
            Dispatcher.UIThread.Post(() =>
            {
                if (ReferenceEquals(sender, browser))
                    CloseRequested?.Invoke(this, EventArgs.Empty);
            });
    }

    private static void onNewWindowRequested(object? sender, WebViewNewWindowRequestedEventArgs args) => args.Handled = true;

    private void showError(string message)
    {
        releaseMedia();
        status.Text = message;
        Content = status;
    }

    private void releaseMedia()
    {
        timeout.Stop();
        loading?.Cancel();
        loading?.Dispose();
        loading = null;
        if (browser is not null)
        {
            browser.EnvironmentRequested -= onEnvironmentRequested;
            browser.NavigationStarted -= onNavigationStarted;
            browser.NavigationCompleted -= onNavigationCompleted;
            browser.NewWindowRequested -= onNewWindowRequested;
            browser.WebMessageReceived -= onWebMessageReceived;
            Content = null;
            browser = null;
        }
        source?.Dispose();
        source = null;
    }
}
