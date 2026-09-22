using Avalonia.Controls;
using Avalonia.Interactivity;
using Ludork.Services;
using System;
using System.IO;
using System.Reflection;
using System.Text;
using System.Text.Json;

namespace Ludork.Views;

public partial class AboutDialog : Window
{
    public AboutDialog()
    {
        InitializeComponent();
        Title = LocaleService.Get("ABOUT_TITLE");
        AppNameText.Text = "Ludork";
        VersionText.Text = $"Version {getVersion()}";
        string aboutFileName = $"About_{LocaleService.CurrentLanguage}.md";
        string aboutPath = EditorRuntimePaths.FindFile("docs", aboutFileName)
            ?? throw new FileNotFoundException("About resource was not found.", aboutFileName);
        DescriptionText.Text = File.ReadAllText(aboutPath, Encoding.UTF8).Trim();
        CopyrightText.Text = LocaleService.Get("ABOUT_COPYRIGHT");
        LicensesButton.Content = LocaleService.Get("ABOUT_LICENSES");
        CloseButton.Content = LocaleService.Get("CLOSE");
    }

    private static string getVersion()
    {
        string? fullVersion = readPackagedVersion();
        if (fullVersion is not null)
            return fullVersion;
        Version? version = Assembly.GetEntryAssembly()?.GetName().Version;
        return version is null ? "1.0.0" : $"{version.Major}.{version.Minor}.{version.Build}";
    }

    private static string? readPackagedVersion()
    {
        string? buildInfoPath = EditorRuntimePaths.FindFile("BuildInfo.json");
        if (buildInfoPath is null)
            return null;
        using JsonDocument document = JsonDocument.Parse(File.ReadAllText(buildInfoPath, Encoding.UTF8));
        return document.RootElement.ValueKind == JsonValueKind.Object
            && document.RootElement.TryGetProperty("fullVersion", out JsonElement fullVersion)
            && fullVersion.ValueKind == JsonValueKind.String
            ? fullVersion.GetString()
            : null;
    }

    private void onOpenLicenses(object? sender, RoutedEventArgs args)
    {
        string noticesFileName = $"THIRD_PARTY_NOTICES_{LocaleService.CurrentLanguage}.md";
        string licensePath = EditorRuntimePaths.FindFile("docs", noticesFileName)
            ?? EditorRuntimePaths.FindFile("docs", "THIRD_PARTY_NOTICES.md")
            ?? throw new FileNotFoundException("Licence resource was not found.", noticesFileName);
        string imageRoot = Path.GetDirectoryName(licensePath)
            ?? throw new InvalidOperationException("Licence resource directory was not found.");
        _ = new MarkdownPreviewWindow(
            licensePath,
            LocaleService.Get("ABOUT_LICENSES"),
            imageRoot).ShowDialog(this);
    }

    private void onClose(object? sender, RoutedEventArgs args)
    {
        Close();
    }
}
