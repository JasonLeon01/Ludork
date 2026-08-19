using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class NewProjectWindow : Window
{
    private static readonly HashSet<string> reservedProjectNames = new(StringComparer.OrdinalIgnoreCase)
    {
        "CON",
        "PRN",
        "AUX",
        "NUL",
        "COM1",
        "COM2",
        "COM3",
        "COM4",
        "COM5",
        "COM6",
        "COM7",
        "COM8",
        "COM9",
        "LPT1",
        "LPT2",
        "LPT3",
        "LPT4",
        "LPT5",
        "LPT6",
        "LPT7",
        "LPT8",
        "LPT9",
    };
    private readonly EditorSettings editorSettings;
    private bool standalone = true;
    private bool creating;

    public NewProjectWindow() : this(EditorSettings.Load())
    {
    }

    public NewProjectWindow(EditorSettings settings)
    {
        editorSettings = settings;
        InitializeComponent();
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
        Title = LocaleService.Get("NEW_PROJECT");
        EditorInputs.ApplyReadOnly(ProjectLocationBox);
        EditorInputs.ApplyEditable(ProjectNameBox);
        applyLocale();

        ProjectLocationBox.Text = EditorPaths.ProjectsDirectory;
        ProjectNameBox.TextChanged += (_, _) => updateValidation();
        FfmpegToggle.IsCheckedChanged += (_, _) => onFfmpegChanged();
        Opened += (_, _) => ProjectNameBox.Focus();
        selectTemplate(true);
    }

    public static Task<string?> ShowAsync(Window owner, EditorSettings settings)
    {
        return new NewProjectWindow(settings).ShowDialog<string?>(owner);
    }

    private void applyLocale()
    {
        HeaderTitle.Text = LocaleService.Get("NEW_PROJECT");
        HeaderSubtitle.Text = LocaleService.Get("NEW_PROJECT_SUBTITLE");
        ChooseTemplateText.Text = LocaleService.Get("NEW_PROJECT_CHOOSE_TEMPLATE");
        StandaloneTitle.Text = LocaleService.Get("NEW_PROJECT_STANDALONE");
        StandaloneSummary.Text = LocaleService.Get("NEW_PROJECT_STANDALONE_SUMMARY");
        StandaloneBadge.Text = LocaleService.Get("NEW_PROJECT_RECOMMENDED");
        StandaloneNoCompilerBadge.Text = LocaleService.Get("NEW_PROJECT_NO_COMPILER");
        StandaloneReadyBadge.Text = LocaleService.Get("NEW_PROJECT_READY_TO_RUN");
        CppTitle.Text = LocaleService.Get("NEW_PROJECT_CPP");
        CppSummary.Text = LocaleService.Get("NEW_PROJECT_CPP_SUMMARY");
        CppBadge.Text = LocaleService.Get("NEW_PROJECT_ADVANCED");
        CppSourceBadge.Text = LocaleService.Get("NEW_PROJECT_FULL_SOURCE");
        CppDebugBadge.Text = LocaleService.Get("NEW_PROJECT_NATIVE_DEBUG");
        FeaturesTitle.Text = LocaleService.Get("NEW_PROJECT_FEATURES");
        RequirementsTitle.Text = LocaleService.Get("NEW_PROJECT_REQUIREMENTS");
        ProjectSettingsTitle.Text = LocaleService.Get("NEW_PROJECT_SETTINGS");
        LocationLabel.Text = LocaleService.Get("NEW_PROJECT_LOCATION");
        NameLabel.Text = LocaleService.Get("NEW_PROJECT_NAME");
        FfmpegTitle.Text = LocaleService.Get("NEW_PROJECT_FFMPEG");
        FfmpegSummary.Text = LocaleService.Get("NEW_PROJECT_FFMPEG_SUMMARY");
        FinalPathCaption.Text = LocaleService.Get("NEW_PROJECT_FINAL_PATH");
        BrowseButton.Content = LocaleService.Get("BROWSE");
        CancelButton.Content = LocaleService.Get("CANCEL");
        CreateButton.Content = LocaleService.Get("CREATE");
        ProgressText.Text = LocaleService.Get("NEW_PROJECT_CREATING");
    }

    private void selectTemplate(bool selectStandalone)
    {
        standalone = selectStandalone;
        StandaloneTemplateButton.IsChecked = standalone;
        CppTemplateButton.IsChecked = !standalone;
        StandaloneHeroIcon.IsVisible = standalone;
        CppHeroIcon.IsVisible = !standalone;

        string prefix = standalone ? "NEW_PROJECT_STANDALONE" : "NEW_PROJECT_CPP";
        DetailTitle.Text = LocaleService.Get(prefix);
        DetailDescription.Text = LocaleService.Get(prefix + "_DESCRIPTION");
        DetailPrimaryBadge.Text = LocaleService.Get(
            standalone ? "NEW_PROJECT_RECOMMENDED" : "NEW_PROJECT_ADVANCED"
        );
        DetailSecondaryBadge.Text = LocaleService.Get(
            standalone ? "NEW_PROJECT_READY_TO_RUN" : "NEW_PROJECT_NATIVE_DEBUG"
        );
        DetailFeature1.Text = "• " + LocaleService.Get(prefix + "_FEATURE_1");
        DetailFeature2.Text = "• " + LocaleService.Get(prefix + "_FEATURE_2");
        DetailFeature3.Text = "• " + LocaleService.Get(prefix + "_FEATURE_3");
        string requirementSuffix = OperatingSystem.IsMacOS() ? "_MACOS" : string.Empty;
        DetailRequirement1.Text = LocaleService.Get(prefix + "_REQUIREMENT_1" + requirementSuffix);
        DetailRequirement2.Text = LocaleService.Get(prefix + "_REQUIREMENT_2" + requirementSuffix);
        DetailRequirement3.Text = LocaleService.Get(prefix + "_REQUIREMENT_3" + requirementSuffix);
        updateCompatibility();
        updateValidation();
    }

    private void onFfmpegChanged()
    {
        updateCompatibility();
        updateValidation();
    }

    private void updateCompatibility()
    {
        bool visible = FfmpegToggle.IsChecked == true && OperatingSystem.IsMacOS();
        CompatibilityPanel.IsVisible = visible;
        CompatibilityText.Text = visible
            ? LocaleService.Get("NEW_PROJECT_FFMPEG_APPLE_HINT")
            : string.Empty;
    }

    private string getTemplateName()
    {
        string templateName = standalone ? "Standalone" : "Cpp";
        return FfmpegToggle.IsChecked == true ? templateName + "-ffmpeg" : templateName;
    }

    private void onStandaloneTemplateClick(object? sender, RoutedEventArgs args)
    {
        selectTemplate(true);
    }

    private void onCppTemplateClick(object? sender, RoutedEventArgs args)
    {
        selectTemplate(false);
    }

    private async void onBrowse(object? sender, RoutedEventArgs args)
    {
        string location = ProjectLocationBox.Text ?? string.Empty;
        IStorageFolder? startLocation = string.IsNullOrWhiteSpace(location)
            ? null
            : await StorageProvider.TryGetFolderFromPathAsync(new Uri(Path.GetFullPath(location)));
        IReadOnlyList<IStorageFolder> folders = await StorageProvider.OpenFolderPickerAsync(
            new FolderPickerOpenOptions
            {
                Title = LocaleService.Get("SELECT_PROJECT_DIR"),
                AllowMultiple = false,
                SuggestedStartLocation = startLocation,
            }
        );
        if (folders.Count == 0)
            return;
        ProjectLocationBox.Text = folders[0].Path.LocalPath;
        updateValidation();
    }

    private async void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (creating)
            return;
        if (args.Key == Key.Escape)
        {
            Close((string?)null);
            args.Handled = true;
            return;
        }
        if (args.Key == Key.Enter && CreateButton.IsEnabled)
        {
            args.Handled = true;
            await createProjectAsync();
            return;
        }
        if (!StandaloneTemplateButton.IsFocused && !CppTemplateButton.IsFocused)
            return;
        if (args.Key is Key.Left or Key.Up)
        {
            selectTemplate(true);
            StandaloneTemplateButton.Focus();
            args.Handled = true;
        }
        else if (args.Key is Key.Right or Key.Down)
        {
            selectTemplate(false);
            CppTemplateButton.Focus();
            args.Handled = true;
        }
    }

    private void onClosing(object? sender, WindowClosingEventArgs args)
    {
        if (creating)
            args.Cancel = true;
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        if (!creating)
            Close((string?)null);
    }

    private async void onCreate(object? sender, RoutedEventArgs args)
    {
        await createProjectAsync();
    }

    private void updateValidation()
    {
        FinalPathText.Text = getTargetPath();
        string? message = getValidationMessage();
        ValidationText.Text = message ?? string.Empty;
        CreateButton.IsEnabled = !creating && message is null;
    }

    private string? getValidationMessage()
    {
        string location = ProjectLocationBox.Text ?? string.Empty;
        if (string.IsNullOrWhiteSpace(location)
            || !Path.IsPathFullyQualified(location)
            || !Directory.Exists(location))
            return LocaleService.Get("SELECT_PROJECT_DIR");

        string name = ProjectNameBox.Text?.Trim() ?? string.Empty;
        if (name.Length == 0)
            return LocaleService.Get("ENTER_PROJECT_NAME");
        if (!isValidProjectName(name))
            return LocaleService.Get("NEW_PROJECT_NAME_INVALID");
        if (Directory.Exists(Path.Combine(location, name)) || File.Exists(Path.Combine(location, name)))
            return LocaleService.Get("PROJECT_EXISTS");

        string templateName = getTemplateName();
        if (EditorRuntimePaths.FindDirectory(Path.Combine("Templates", templateName)) is null)
            return LocaleService.Get("PROJECT_TEMPLATE_MISSING");
        return null;
    }

    private string getTargetPath()
    {
        string location = ProjectLocationBox.Text ?? string.Empty;
        string name = ProjectNameBox.Text?.Trim() ?? string.Empty;
        string targetPath = name.Length == 0 ? location : Path.Combine(location, name);
        return Path.IsPathFullyQualified(targetPath)
            ? Path.GetFullPath(targetPath)
            : targetPath;
    }

    private static bool isValidProjectName(string name)
    {
        if (!OperatingSystem.IsWindows())
        {
            return name is not "." and not ".."
                && name.IndexOfAny(Path.GetInvalidFileNameChars()) < 0;
        }
        string stem = name.Split('.')[0].TrimEnd(' ');
        return name is not "." and not ".."
            && name.IndexOfAny(Path.GetInvalidFileNameChars()) < 0
            && !name.EndsWith('.')
            && !name.EndsWith(' ')
            && !reservedProjectNames.Contains(stem);
    }

    private void setCreating(bool value)
    {
        creating = value;
        TemplateAndDetails.IsEnabled = !value;
        ProjectSettingsPanel.IsEnabled = !value;
        CancelButton.IsEnabled = !value;
        CopyProgress.IsVisible = value;
        ProgressText.IsVisible = value;
        if (value)
            CreateButton.IsEnabled = false;
        else
            updateValidation();
    }

    private async Task createProjectAsync()
    {
        string? validationMessage = getValidationMessage();
        if (validationMessage is not null)
        {
            updateValidation();
            return;
        }

        string templateName = getTemplateName();
        string templatePath = EditorRuntimePaths.FindDirectory(
            Path.Combine("Templates", templateName)
        )!;
        string targetPath = getTargetPath();
        setCreating(true);

        try
        {
            await Task.Run(() => copyDirectory(templatePath, targetPath));
            if (standalone && OperatingSystem.IsMacOS())
            {
                string executablePath = Path.Combine(targetPath, "Main");
                UnixFileMode mode = File.GetUnixFileMode(executablePath);
                File.SetUnixFileMode(
                    executablePath,
                    mode | UnixFileMode.UserExecute | UnixFileMode.GroupExecute | UnixFileMode.OtherExecute
                );
            }
            JsonObject config = new()
            {
                ["Cpp"] = !standalone,
            };
            if (FfmpegToggle.IsChecked == true)
                config["ffmpeg"] = true;
            string projectFilePath = Path.Combine(targetPath, "Main.proj");
            await File.WriteAllTextAsync(
                projectFilePath,
                config.ToJsonString(new() { WriteIndented = true })
            );
            creating = false;
            Close(projectFilePath);
        }
        catch (Exception exception) when (
            exception is IOException
                or UnauthorizedAccessException
                or ArgumentException
                or NotSupportedException
        )
        {
            removePartialProject(targetPath);
            setCreating(false);
            await AlertDialog.ShowAsync(
                this,
                LocaleService.Get("ERROR"),
                LocaleService.Get("COPY_FAILED") + Environment.NewLine + exception.Message
            );
        }
    }

    private static void copyDirectory(string sourcePath, string targetPath)
    {
        DirectoryInfo source = new(sourcePath);
        DirectoryInfo target = Directory.CreateDirectory(targetPath);
        foreach (FileInfo file in source.GetFiles())
            file.CopyTo(Path.Combine(target.FullName, file.Name), true);
        foreach (DirectoryInfo directory in source.GetDirectories())
            copyDirectory(directory.FullName, Path.Combine(target.FullName, directory.Name));
    }

    private static void removePartialProject(string targetPath)
    {
        if (!Directory.Exists(targetPath))
            return;
        try
        {
            Directory.Delete(targetPath, true);
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

}
