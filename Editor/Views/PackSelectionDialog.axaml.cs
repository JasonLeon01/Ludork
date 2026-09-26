using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Platform;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.IO;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class PackSelectionDialog : Window
{
    private const string AndroidStudioPath = "/Applications/Android Studio.app";
    private const string DevEcoStudioPath = "/Applications/DevEco-Studio.app";
    private const string DevEcoStudioEnvironment = "LUDORK_DEVECO_STUDIO";
    private bool? encryptionOptionsState;
    private bool encryptionOptionsExpanded;
    private readonly ProjectConfigService? projectConfig;
    private readonly string? projectPath;
    private readonly bool isStandalone;
    private CancellationTokenSource? validationCancellation;
    private int validationGeneration;
    private bool closed;

    public PackSelectionDialog() : this(null)
    {
    }

    public PackSelectionDialog(ProjectConfigService? projectConfig, string? projectPath = null)
    {
        this.projectConfig = projectConfig;
        this.projectPath = projectPath;
        isStandalone = projectConfig?.IsStandalone ?? true;
        InitializeComponent();
        VersionLabel.Text = LocaleService.Get("PACK_RELEASE_VERSION");
        DevOption.Content = LocaleService.Get("PACK_DEV");
        EditorInputs.ApplyEditable(VersionBox);
        VersionBox.Text = projectConfig?.PackagingVersion ?? "1.0.0";
        DevOption.IsVisible = !isStandalone;
        DevOption.IsChecked = !isStandalone && projectConfig?.PackagingDev == true;
        VersionBox.TextChanged += async (_, _) => await validateVersionAsync();
        DevOption.IsCheckedChanged += async (_, _) => await validateVersionAsync();
        Closed += (_, _) =>
        {
            closed = true;
            ++validationGeneration;
            validationCancellation?.Cancel();
        };
        Opened += async (_, _) => await validateVersionAsync(true);
        Title = LocaleService.Get("PACK_PROJECT");
        DescriptionText.Text = LocaleService.Get("PACK_MODE_DESC");
        Win32Option.Content = LocaleService.Get("PACK_PLATFORM_WIN32");
        MacOSOption.Content = LocaleService.Get("PACK_PLATFORM_MACOS");
        MacOSSigningOption.Content = LocaleService.Get("PACK_MACOS_SIGN_APP");
        IosOption.Content = LocaleService.Get("PACK_PLATFORM_IOS");
        IosSigningOption.Content = LocaleService.Get("PACK_IOS_SIGN_APP");
        IosStatusText.Text = LocaleService.Get("PACK_IOS_REQUIREMENTS");
        HarmonyOption.Content = LocaleService.Get("PACK_PLATFORM_HARMONYOS");
        HarmonySigningOption.Content = LocaleService.Get("PACK_HARMONY_SIGN_HAP");
        HarmonyMobileOption.Content = LocaleService.Get("PACK_HARMONY_DEVICE_MOBILE");
        HarmonyTwoInOneOption.Content = LocaleService.Get("PACK_HARMONY_DEVICE_TWO_IN_ONE");
        HarmonyGraphicsApiText.Text = LocaleService.Get("PACK_HARMONY_GRAPHICS_API");
        HarmonyOpenGLOption.Content = LocaleService.Get("PACK_HARMONY_GRAPHICS_OPENGL");
        HarmonyOpenGLESOption.Content = LocaleService.Get("PACK_HARMONY_GRAPHICS_OPENGL_ES");
        ExportToHarmonyDeviceOption.Content = LocaleService.Get("PACK_EXPORT_TO_HARMONY_DEVICE");
        AndroidOption.Content = LocaleService.Get("PACK_PLATFORM_ANDROID");
        AndroidSigningOption.Content = LocaleService.Get("PACK_ANDROID_SIGN_APK");
        ExportToIPhoneOption.Content = LocaleService.Get("PACK_EXPORT_TO_IPHONE");
        EncryptionOptionsTitle.Text = LocaleService.Get("PACK_ENCRYPT_GAME_DATA");
        LuacOption.Content = LocaleService.Get("PACK_USE_LUAC");
        EncryptShadersOption.Content = LocaleService.Get("PACK_ENCRYPT_SHADERS");
        EncryptDataOption.Content = LocaleService.Get("PACK_ENCRYPT_DATA");
        EncryptSavesOption.Content = LocaleService.Get("PACK_ENCRYPT_SAVES");
        UseLdPakOption.Content = LocaleService.Get("PACK_USE_LDPAK");
        ConfirmButton.Content = LocaleService.Get("CONFIRM");
        CancelButton.Content = LocaleService.Get("CANCEL");
        EncryptSavesOption.IsEnabled = !isStandalone;
        EncryptSavesHintText.Text = LocaleService.Get("PACK_ENCRYPT_SAVES_STANDALONE_HINT");
        EncryptSavesHintText.IsVisible = isStandalone;
        MacOSOption.IsCheckedChanged += (_, _) => updatePlatformOptionVisibility();
        IosOption.IsCheckedChanged += (_, _) => updatePlatformOptionVisibility();
        HarmonyOption.IsCheckedChanged += (_, _) => updateHarmonyDeviceVisibility();
        HarmonyMobileOption.IsCheckedChanged += (_, _) => updateHarmonyDeviceVisibility();
        HarmonyTwoInOneOption.IsCheckedChanged += (_, _) => updateHarmonyDeviceVisibility();
        ExportToHarmonyDeviceOption.IsCheckedChanged += (_, _) => updateHarmonyDeviceVisibility();
        AndroidOption.IsCheckedChanged += (_, _) => updateAndroidSigningVisibility();
        EncryptGameDataOption.Click += (_, _) => toggleEncryptionOptions();
        LuacOption.IsCheckedChanged += (_, _) => updateEncryptionOptionsState();
        EncryptShadersOption.IsCheckedChanged += (_, _) => updateEncryptionOptionsState();
        EncryptDataOption.IsCheckedChanged += (_, _) => updateEncryptionOptionsState();
        EncryptSavesOption.IsCheckedChanged += (_, _) => updateEncryptionOptionsState();
        updateEncryptionOptionsState();
        Win32Option.IsVisible = OperatingSystem.IsWindows();
        MacOSPanel.IsVisible = OperatingSystem.IsMacOS();
        bool sourceProjectOnMacOS = OperatingSystem.IsMacOS() && !isStandalone;
        IosPanel.IsVisible = sourceProjectOnMacOS;
        HarmonyPanel.IsVisible = sourceProjectOnMacOS
            && hasDevEcoStudio();
        AndroidPanel.IsVisible = sourceProjectOnMacOS
            && hasAndroidStudio();
        if (OperatingSystem.IsWindows())
            Win32Option.IsChecked = true;
        else if (OperatingSystem.IsMacOS())
            MacOSOption.IsChecked = true;
        else
            ConfirmButton.IsEnabled = false;
        Opened += (_, _) =>
        {
            Screen? screen = Screens.ScreenFromWindow(this) ?? Screens.Primary;
            if (screen is not null)
                MaxHeight = Math.Max(400, screen.WorkingArea.Height / screen.Scaling - 80);
            if (OperatingSystem.IsWindows())
                Win32Option.Focus();
            else if (OperatingSystem.IsMacOS())
                MacOSOption.Focus();
        };
    }

    private static bool hasAndroidStudio()
    {
        string userApplicationsPath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            "Applications",
            "Android Studio.app");
        return Directory.Exists(AndroidStudioPath)
            || Directory.Exists(userApplicationsPath);
    }

    private static bool hasDevEcoStudio()
    {
        string? configuredPath = Environment.GetEnvironmentVariable(
            DevEcoStudioEnvironment);
        if (!string.IsNullOrWhiteSpace(configuredPath))
        {
            string userProfile = Environment.GetFolderPath(
                Environment.SpecialFolder.UserProfile);
            string expandedPath = configuredPath == "~"
                ? userProfile
                : configuredPath.StartsWith("~/", StringComparison.Ordinal)
                    ? Path.Combine(userProfile, configuredPath[2..])
                    : configuredPath;
            if (Directory.Exists(expandedPath))
                return true;
        }

        string userApplicationsPath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            "Applications",
            "DevEco-Studio.app");
        return Directory.Exists(DevEcoStudioPath)
            || Directory.Exists(userApplicationsPath);
    }

    private async Task<bool> validateVersionAsync(bool immediate = false)
    {
        int generation = ++validationGeneration;
        validationCancellation?.Cancel();
        using CancellationTokenSource cancellation = new();
        validationCancellation = cancellation;
        VersionErrorText.IsVisible = false;
        try
        {
            if (!immediate)
                await Task.Delay(200, cancellation.Token);
            ProjectPackageMetadataResult result = await ProjectPackageMetadataService.ExecuteAsync(
                "release-version", null, VersionBox.Text ?? string.Empty,
                !isStandalone && DevOption.IsChecked == true, cancellation.Token);
            if (closed || generation != validationGeneration)
                return false;
            if (result.ExitCode != 0)
            {
                showVersionError(result.Error.Length != 0 ? result.Error : result.Output);
                return false;
            }
            using JsonDocument document = JsonDocument.Parse(result.Output);
            if (document.RootElement.ValueKind != JsonValueKind.Object
                || !document.RootElement.TryGetProperty("fullVersion", out JsonElement fullVersion)
                || fullVersion.ValueKind != JsonValueKind.String)
            {
                showVersionError(LocaleService.Get("PACK_VERSION_RESPONSE_INVALID"));
                return false;
            }
            return true;
        }
        catch (OperationCanceledException)
        {
            return false;
        }
        catch (JsonException exception)
        {
            if (!closed && generation == validationGeneration)
                showVersionError(exception.Message);
            return false;
        }
        finally
        {
            if (ReferenceEquals(validationCancellation, cancellation))
                validationCancellation = null;
        }
    }

    private void showVersionError(string message)
    {
        VersionErrorText.Text = message;
        VersionErrorText.IsVisible = true;
    }

    private async void onConfirm(object? sender, RoutedEventArgs args)
    {
        OptionsPanel.IsEnabled = false;
        ConfirmButton.IsEnabled = false;
        try
        {
            if (!await validateVersionAsync(true))
                return;
            ProjectPackPlatform? platform = Win32Option.IsChecked == true ? ProjectPackPlatform.Win32
                : MacOSOption.IsChecked == true ? ProjectPackPlatform.MacOS
                : IosOption.IsChecked == true ? ProjectPackPlatform.IOS
                : HarmonyOption.IsChecked == true ? ProjectPackPlatform.HarmonyOS
                : AndroidOption.IsChecked == true ? ProjectPackPlatform.Android
                : null;
            if (platform is null)
                return;
            AndroidSigningOptions? signing = null;
            HarmonySigningOptions? harmonySigning = null;
            if (platform == ProjectPackPlatform.HarmonyOS
                && (HarmonySigningOption.IsChecked == true || ExportToHarmonyDeviceOption.IsChecked == true))
            {
                HarmonySigningDialog signingDialog = new(projectPath);
                harmonySigning = await signingDialog.ShowDialog<HarmonySigningOptions?>(this);
                if (harmonySigning is null || closed)
                    return;
            }
            if (platform == ProjectPackPlatform.Android && AndroidSigningOption.IsChecked == true)
            {
                AndroidSigningDialog signingDialog = new();
                signing = await signingDialog.ShowDialog<AndroidSigningOptions?>(this);
                if (signing is null || closed)
                    return;
            }
            MacOSSigningOptions? macOSSigning = null;
            if (platform == ProjectPackPlatform.MacOS && MacOSSigningOption.IsChecked == true)
            {
                MacOSSigningDialog signingDialog = new();
                MacOSSigningSelection? selection = await signingDialog.ShowDialog<MacOSSigningSelection?>(this);
                if (selection is null || closed)
                    return;
                macOSSigning = selection.Options;
            }
            IOSSigningOptions? iosSigning = null;
            if (platform == ProjectPackPlatform.IOS && IosSigningOption.IsChecked == true)
            {
                IOSSigningDialog signingDialog = new();
                IOSSigningSelection? selection = await signingDialog.ShowDialog<IOSSigningSelection?>(this);
                if (selection is null || closed)
                    return;
                iosSigning = selection.Options;
            }
            HarmonyDeviceForm deviceForm = HarmonyTwoInOneOption.IsChecked == true
                ? HarmonyDeviceForm.TwoInOne
                : HarmonyDeviceForm.Mobile;
            ProjectPackOptions options = new(
                platform.Value,
                LuacOption.IsChecked == true,
                EncryptShadersOption.IsChecked == true,
                EncryptDataOption.IsChecked == true,
                EncryptSavesOption.IsChecked == true,
                UseLdPakOption.IsChecked == true)
            {
                Version = VersionBox.Text ?? string.Empty,
                Dev = !isStandalone && DevOption.IsChecked == true,
                ExportToIPhone = ExportToIPhoneOption.IsChecked == true,
                ExportToHarmonyDevice = ExportToHarmonyDeviceOption.IsChecked == true,
                HarmonyDeviceForm = deviceForm,
                HarmonyGraphicsApi = deviceForm == HarmonyDeviceForm.Mobile || HarmonyOpenGLESOption.IsChecked == true
                    ? HarmonyGraphicsApi.OpenGLES : HarmonyGraphicsApi.OpenGL,
                AndroidSigning = signing,
                HarmonySigning = harmonySigning,
                MacOSSigning = macOSSigning,
                IOSSigning = iosSigning,
            };
            projectConfig?.SetPackaging(options.Version, options.Dev);
            Close(options);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            showVersionError(exception.Message);
        }
        finally
        {
            if (!closed)
            {
                OptionsPanel.IsEnabled = true;
                ConfirmButton.IsEnabled = OperatingSystem.IsWindows() || OperatingSystem.IsMacOS();
            }
        }
    }

    private void onToggleEncryptionOptions(object? sender, RoutedEventArgs args)
    {
        encryptionOptionsExpanded = !encryptionOptionsExpanded;
        EncryptionOptionsPanel.IsVisible = encryptionOptionsExpanded;
        if (EncryptionOptionsChevron.RenderTransform is RotateTransform rotate)
            rotate.Angle = encryptionOptionsExpanded ? 90 : 0;
    }

    private void toggleEncryptionOptions()
    {
        bool isChecked = encryptionOptionsState != true;
        LuacOption.IsChecked = isChecked;
        EncryptShadersOption.IsChecked = isChecked;
        EncryptDataOption.IsChecked = isChecked;
        if (EncryptSavesOption.IsEnabled)
            EncryptSavesOption.IsChecked = isChecked;
        updateEncryptionOptionsState();
    }

    private void updateEncryptionOptionsState()
    {
        bool useLuac = LuacOption.IsChecked == true;
        bool encryptShaders = EncryptShadersOption.IsChecked == true;
        bool encryptData = EncryptDataOption.IsChecked == true;
        bool encryptSaves = EncryptSavesOption.IsChecked == true;
        bool includeEncryptSaves = EncryptSavesOption.IsEnabled;
        bool allChecked = useLuac
            && encryptShaders
            && encryptData
            && (!includeEncryptSaves || encryptSaves);
        bool anyChecked = useLuac
            || encryptShaders
            || encryptData
            || (includeEncryptSaves && encryptSaves);
        if (allChecked)
            encryptionOptionsState = true;
        else if (anyChecked)
            encryptionOptionsState = null;
        else
            encryptionOptionsState = false;
        EncryptGameDataOption.IsChecked = encryptionOptionsState;
    }

    private void updateAndroidSigningVisibility()
    {
        AndroidSigningOption.IsVisible = AndroidOption.IsVisible
            && AndroidOption.IsChecked == true;
    }

    private void updateHarmonyDeviceVisibility()
    {
        HarmonyDevicePanel.IsVisible = HarmonyOption.IsVisible
            && HarmonyOption.IsChecked == true;
        HarmonyGraphicsApiPanel.IsVisible = HarmonyDevicePanel.IsVisible
            && HarmonyTwoInOneOption.IsChecked == true;
        ExportToHarmonyDeviceOption.IsVisible = HarmonyDevicePanel.IsVisible;
        if (ExportToHarmonyDeviceOption.IsChecked == true)
            HarmonySigningOption.IsChecked = true;
        HarmonySigningOption.IsEnabled = ExportToHarmonyDeviceOption.IsChecked != true;
    }

    private void updateIosDetailsVisibility()
    {
        bool isIosSelected = IosOption.IsVisible
            && IosOption.IsChecked == true;
        IosStatusText.IsVisible = isIosSelected;
        ExportToIPhoneOption.IsVisible = isIosSelected;
    }

    private void updatePlatformOptionVisibility()
    {
        updateIosDetailsVisibility();
        updateSigningOptionVisibility();
    }

    private void updateSigningOptionVisibility()
    {
        MacOSSigningOption.IsVisible = MacOSOption.IsVisible
            && MacOSOption.IsChecked == true;
        IosSigningOption.IsVisible = IosOption.IsVisible
            && IosOption.IsChecked == true;
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close((ProjectPackOptions?)null);
    }
}
