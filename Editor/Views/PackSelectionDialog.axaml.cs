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
    private readonly MacOSSigningInputs macOSSigning = new();
    private readonly IOSSigningInputs iosSigning = new();
    private readonly HarmonySigningInputs harmonySigning;
    private readonly AndroidSigningInputs androidSigning = new();
    private readonly CancellationTokenSource lifetimeCancellation = new();
    private CancellationTokenSource? appNameCancellation;
    private int appNameGeneration;
    private string? appNameError;
    private bool versionValid;
    private bool appNameValid;
    private bool confirming;
    private bool opened;
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
        harmonySigning = new(projectPath);
        MacOSSigningHost.Content = macOSSigning;
        IOSSigningHost.Content = iosSigning;
        HarmonySigningHost.Content = harmonySigning;
        AndroidSigningHost.Content = androidSigning;
        macOSSigning.StateChanged += (_, _) => updateConfirmState();
        iosSigning.StateChanged += (_, _) => updateConfirmState();
        harmonySigning.StateChanged += (_, _) => updateConfirmState();
        androidSigning.StateChanged += (_, _) => updateConfirmState();
        VersionLabel.Text = LocaleService.Get("PACK_RELEASE_VERSION");
        DevOption.Content = LocaleService.Get("PACK_DEV");
        EditorInputs.ApplyEditable(VersionBox);
        VersionBox.Text = projectConfig?.PackagingVersion ?? "1.0.0";
        DevOption.IsVisible = !isStandalone;
        DevOption.IsChecked = !isStandalone && projectConfig?.PackagingDev == true;
        ServerOptions.IsVisible = !isStandalone;
        VersionBox.TextChanged += async (_, _) => await validateVersionAsync();
        DevOption.IsCheckedChanged += async (_, _) => await validateVersionAsync();
        Closed += (_, _) =>
        {
            closed = true;
            ++validationGeneration;
            ++appNameGeneration;
            validationCancellation?.Cancel();
            appNameCancellation?.Cancel();
            lifetimeCancellation.Cancel();
            macOSSigning.CancelPendingOperations();
            iosSigning.CancelPendingOperations();
            harmonySigning.CancelPendingOperations();
            androidSigning.CancelPendingOperations();
            lifetimeCancellation.Dispose();
        };
        Activated += async (_, _) =>
        {
            if (opened && !confirming)
                await validateAppNameAsync();
        };
        Title = LocaleService.Get("PACK_PROJECT");
        DescriptionText.Text = LocaleService.Get("PACK_MODE_DESC");
        MacOSSigningOption.Content = LocaleService.Get("PACK_MACOS_SIGN_APP");
        IosSigningOption.Content = LocaleService.Get("PACK_IOS_SIGN_APP");
        IosStatusText.Text = LocaleService.Get("PACK_IOS_REQUIREMENTS");
        HarmonySigningOption.Content = LocaleService.Get("PACK_HARMONY_SIGN_HAP");
        HarmonyMobileOption.Content = LocaleService.Get("PACK_HARMONY_DEVICE_MOBILE");
        HarmonyTwoInOneOption.Content = LocaleService.Get("PACK_HARMONY_DEVICE_TWO_IN_ONE");
        HarmonyGraphicsApiText.Text = LocaleService.Get("PACK_HARMONY_GRAPHICS_API");
        HarmonyOpenGLOption.Content = LocaleService.Get("PACK_HARMONY_GRAPHICS_OPENGL");
        HarmonyOpenGLESOption.Content = LocaleService.Get("PACK_HARMONY_GRAPHICS_OPENGL_ES");
        ExportToHarmonyDeviceOption.Content = LocaleService.Get("PACK_EXPORT_TO_HARMONY_DEVICE");
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
        PlatformList.SelectionChanged += (_, _) =>
        {
            updatePlatformOptions();
            OptionsScroll.Offset = default;
        };
        MacOSSigningOption.IsCheckedChanged += (_, _) => updatePlatformOptions();
        IosSigningOption.IsCheckedChanged += (_, _) => updatePlatformOptions();
        HarmonySigningOption.IsCheckedChanged += (_, _) => updatePlatformOptions();
        AndroidSigningOption.IsCheckedChanged += (_, _) => updatePlatformOptions();
        HarmonyMobileOption.IsCheckedChanged += (_, _) => updatePlatformOptions();
        HarmonyTwoInOneOption.IsCheckedChanged += (_, _) => updatePlatformOptions();
        ExportToHarmonyDeviceOption.IsCheckedChanged += (_, _) => updatePlatformOptions();
        EncryptGameDataOption.Click += (_, _) => toggleEncryptionOptions();
        LuacOption.IsCheckedChanged += (_, _) => updateEncryptionOptionsState();
        EncryptShadersOption.IsCheckedChanged += (_, _) => updateEncryptionOptionsState();
        EncryptDataOption.IsCheckedChanged += (_, _) => updateEncryptionOptionsState();
        EncryptSavesOption.IsCheckedChanged += (_, _) => updateEncryptionOptionsState();
        updateEncryptionOptionsState();
        if (OperatingSystem.IsWindows())
            addPlatform(ProjectPackPlatform.Win32, "PACK_PLATFORM_WIN32");
        if (OperatingSystem.IsMacOS())
        {
            addPlatform(ProjectPackPlatform.MacOS, "PACK_PLATFORM_MACOS");
            if (!isStandalone)
            {
                addPlatform(ProjectPackPlatform.IOS, "PACK_PLATFORM_IOS");
                if (hasDevEcoStudio())
                    addPlatform(ProjectPackPlatform.HarmonyOS, "PACK_PLATFORM_HARMONYOS");
                if (hasAndroidStudio())
                    addPlatform(ProjectPackPlatform.Android, "PACK_PLATFORM_ANDROID");
            }
        }
        PlatformList.SelectedIndex = PlatformList.ItemCount > 0 ? 0 : -1;
        updatePlatformOptions();
        Opened += async (_, _) =>
        {
            opened = true;
            Screen? screen = Screens.ScreenFromWindow(this) ?? Screens.Primary;
            if (screen is not null)
            {
                MaxWidth = Math.Max(1, screen.WorkingArea.Width / screen.Scaling - 40);
                MaxHeight = Math.Max(1, screen.WorkingArea.Height / screen.Scaling - 80);
                MinWidth = Math.Min(MinWidth, MaxWidth);
                MinHeight = Math.Min(MinHeight, MaxHeight);
                Width = Math.Min(Width, MaxWidth);
                Height = Math.Min(Height, MaxHeight);
            }
            if (PlatformList.SelectedItem is ListBoxItem selectedItem)
                selectedItem.Focus();
            else
                PlatformList.Focus();
            await Task.WhenAll(validateVersionAsync(true), validateAppNameAsync());
        };
    }

    private ProjectPackPlatform? selectedPlatform =>
        PlatformList.SelectedItem is ListBoxItem { Tag: ProjectPackPlatform platform } ? platform : null;

    private void addPlatform(ProjectPackPlatform platform, string localeKey)
    {
        PlatformList.Items.Add(new ListBoxItem
        {
            Content = LocaleService.Get(localeKey),
            Tag = platform,
        });
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
        if (closed)
            return false;
        int generation = ++validationGeneration;
        validationCancellation?.Cancel();
        using CancellationTokenSource cancellation = new();
        validationCancellation = cancellation;
        versionValid = false;
        VersionErrorText.IsVisible = false;
        updateConfirmState();
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
            versionValid = true;
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
            if (!closed && generation == validationGeneration)
                updateConfirmState();
        }
    }

    private async Task<bool> validateAppNameAsync()
    {
        if (closed)
            return false;
        int generation = ++appNameGeneration;
        appNameCancellation?.Cancel();
        using CancellationTokenSource cancellation = new();
        appNameCancellation = cancellation;
        appNameValid = false;
        updateConfirmState();
        try
        {
            ProjectPackageMetadataResult result = string.IsNullOrWhiteSpace(projectPath)
                ? new(-1, string.Empty, LocaleService.Get("PACK_NO_PROJECT"))
                : await ProjectPackageMetadataService.ReadAppNameAsync(projectPath, cancellation.Token);
            if (closed || generation != appNameGeneration)
                return false;
            appNameValid = result.ExitCode == 0;
            appNameError = appNameValid ? null
                : result.ExitCode == ProjectToolConstants.AppNameUnchangedExitCode
                    ? LocaleService.Get("PACK_APP_NAME_UNCHANGED")
                    : result.Error.Length != 0 ? result.Error : result.Output;
            return appNameValid;
        }
        catch (OperationCanceledException)
        {
            return false;
        }
        finally
        {
            if (ReferenceEquals(appNameCancellation, cancellation))
                appNameCancellation = null;
            if (!closed && generation == appNameGeneration)
                updateConfirmState();
        }
    }

    private void updateConfirmState()
    {
        bool signingValid = selectedPlatform switch
        {
            ProjectPackPlatform.MacOS => MacOSSigningOption.IsChecked != true || macOSSigning.CanConfirm,
            ProjectPackPlatform.IOS => IosSigningOption.IsChecked != true || iosSigning.CanConfirm,
            ProjectPackPlatform.HarmonyOS => HarmonySigningOption.IsChecked != true || harmonySigning.CanConfirm,
            ProjectPackPlatform.Android => AndroidSigningOption.IsChecked != true || androidSigning.CanConfirm,
            _ => true,
        };
        ConfirmButton.IsEnabled = !closed && !confirming && appNameValid && versionValid
            && selectedPlatform is not null && signingValid;
        ToolTip.SetTip(ConfirmButtonHint, appNameValid ? null : appNameError);
    }

    private void showVersionError(string message)
    {
        VersionErrorText.Text = message;
        VersionErrorText.IsVisible = true;
    }

    private async void onConfirm(object? sender, RoutedEventArgs args)
    {
        if (!ConfirmButton.IsEnabled || selectedPlatform is not ProjectPackPlatform platform)
            return;
        confirming = true;
        OptionsPanel.IsEnabled = false;
        updateConfirmState();
        try
        {
            if (!await validateAppNameAsync() || !await validateVersionAsync(true) || closed)
                return;
            LudorkServerSettings serverSettings = LudorkServerSettings.Disabled;
            if (!isStandalone && !ServerOptions.TryGetSettings(out serverSettings))
                return;
            CancellationToken token = lifetimeCancellation.Token;
            AndroidSigningOptions? androidOptions = null;
            HarmonySigningOptions? harmonyOptions = null;
            MacOSSigningOptions? macOSOptions = null;
            IOSSigningOptions? iosOptions = null;
            if (platform == ProjectPackPlatform.HarmonyOS && HarmonySigningOption.IsChecked == true)
            {
                if (!await harmonySigning.TryPrepareAsync(token) || closed)
                    return;
                harmonyOptions = harmonySigning.Options;
            }
            if (platform == ProjectPackPlatform.Android && AndroidSigningOption.IsChecked == true)
            {
                if (!await androidSigning.TryPrepareAsync(token) || closed)
                    return;
                androidOptions = androidSigning.Options;
            }
            if (platform == ProjectPackPlatform.MacOS && MacOSSigningOption.IsChecked == true)
            {
                if (!await macOSSigning.TryPrepareAsync(token) || closed)
                    return;
                macOSOptions = macOSSigning.Options;
            }
            if (platform == ProjectPackPlatform.IOS && IosSigningOption.IsChecked == true)
            {
                if (!await iosSigning.TryPrepareAsync(token) || closed)
                    return;
                iosOptions = iosSigning.Options;
            }
            HarmonyDeviceForm deviceForm = platform == ProjectPackPlatform.HarmonyOS
                && HarmonyTwoInOneOption.IsChecked == true ? HarmonyDeviceForm.TwoInOne : HarmonyDeviceForm.Mobile;
            ProjectPackOptions options = new(
                platform,
                LuacOption.IsChecked == true,
                EncryptShadersOption.IsChecked == true,
                EncryptDataOption.IsChecked == true,
                EncryptSavesOption.IsChecked == true,
                UseLdPakOption.IsChecked == true)
            {
                Version = VersionBox.Text ?? string.Empty,
                Dev = !isStandalone && DevOption.IsChecked == true,
                ExportToIPhone = platform == ProjectPackPlatform.IOS && ExportToIPhoneOption.IsChecked == true,
                ExportToHarmonyDevice = platform == ProjectPackPlatform.HarmonyOS
                    && ExportToHarmonyDeviceOption.IsChecked == true,
                HarmonyDeviceForm = deviceForm,
                HarmonyGraphicsApi = deviceForm == HarmonyDeviceForm.Mobile || HarmonyOpenGLESOption.IsChecked == true
                    ? HarmonyGraphicsApi.OpenGLES : HarmonyGraphicsApi.OpenGL,
                AndroidSigning = androidOptions,
                HarmonySigning = harmonyOptions,
                MacOSSigning = macOSOptions,
                IOSSigning = iosOptions,
                Server = serverSettings,
            };
            projectConfig?.SetPackaging(options.Version, options.Dev);
            Close(options);
        }
        catch (OperationCanceledException) when (closed)
        {
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            showVersionError(exception.Message);
        }
        finally
        {
            if (!closed)
            {
                confirming = false;
                OptionsPanel.IsEnabled = true;
                updateConfirmState();
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

    private void updatePlatformOptions()
    {
        ProjectPackPlatform? platform = selectedPlatform;
        PlatformTitle.Text = (PlatformList.SelectedItem as ListBoxItem)?.Content as string;
        MacOSPanel.IsVisible = platform == ProjectPackPlatform.MacOS;
        IosPanel.IsVisible = platform == ProjectPackPlatform.IOS;
        HarmonyPanel.IsVisible = platform == ProjectPackPlatform.HarmonyOS;
        AndroidPanel.IsVisible = platform == ProjectPackPlatform.Android;
        HarmonyGraphicsApiPanel.IsVisible = HarmonyTwoInOneOption.IsChecked == true;
        if (ExportToHarmonyDeviceOption.IsChecked == true)
            HarmonySigningOption.IsChecked = true;
        HarmonySigningOption.IsEnabled = ExportToHarmonyDeviceOption.IsChecked != true;
        MacOSSigningHost.IsVisible = MacOSPanel.IsVisible && MacOSSigningOption.IsChecked == true;
        IOSSigningHost.IsVisible = IosPanel.IsVisible && IosSigningOption.IsChecked == true;
        HarmonySigningHost.IsVisible = HarmonyPanel.IsVisible && HarmonySigningOption.IsChecked == true;
        AndroidSigningHost.IsVisible = AndroidPanel.IsVisible && AndroidSigningOption.IsChecked == true;
        SigningSeparator.IsVisible = MacOSSigningHost.IsVisible || IOSSigningHost.IsVisible
            || HarmonySigningHost.IsVisible || AndroidSigningHost.IsVisible;
        updateConfirmState();
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close((ProjectPackOptions?)null);
    }
}
