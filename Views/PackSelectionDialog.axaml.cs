using Avalonia.Controls;
using Avalonia.Interactivity;
using Ludork.Services;
using System;
using System.IO;

namespace Ludork.Views;

public partial class PackSelectionDialog : Window
{
    private const string AndroidStudioPath = "/Applications/Android Studio.app";
    private const string DevEcoStudioPath = "/Applications/DevEco-Studio.app";
    private const string DevEcoStudioEnvironment = "LUDORK_DEVECO_STUDIO";
    private bool? encryptionOptionsState;

    public PackSelectionDialog() : this(true)
    {
    }

    public PackSelectionDialog(bool isStandalone)
    {
        InitializeComponent();
        Title = LocaleService.Get("PACK_PROJECT");
        DescriptionText.Text = LocaleService.Get("PACK_MODE_DESC");
        Win32Option.Content = LocaleService.Get("PACK_PLATFORM_WIN32");
        MacOSOption.Content = LocaleService.Get("PACK_PLATFORM_MACOS");
        IosOption.Content = LocaleService.Get("PACK_PLATFORM_IOS");
        IosStatusText.Text = LocaleService.Get("PACK_IOS_REQUIREMENTS");
        HarmonyOption.Content = LocaleService.Get("PACK_PLATFORM_HARMONYOS");
        HarmonyMobileOption.Content = LocaleService.Get("PACK_HARMONY_DEVICE_MOBILE");
        HarmonyTwoInOneOption.Content = LocaleService.Get("PACK_HARMONY_DEVICE_TWO_IN_ONE");
        HarmonyGraphicsApiText.Text = LocaleService.Get("PACK_HARMONY_GRAPHICS_API");
        HarmonyOpenGLOption.Content = LocaleService.Get("PACK_HARMONY_GRAPHICS_OPENGL");
        HarmonyOpenGLESOption.Content = LocaleService.Get("PACK_HARMONY_GRAPHICS_OPENGL_ES");
        ExportToHarmonyDeviceOption.Content = LocaleService.Get("PACK_EXPORT_TO_HARMONY_DEVICE");
        AndroidOption.Content = LocaleService.Get("PACK_PLATFORM_ANDROID");
        AndroidSigningOption.Content = LocaleService.Get("PACK_ANDROID_SIGN_APK");
        ExportToIPhoneOption.Content = LocaleService.Get("PACK_EXPORT_TO_IPHONE");
        EncryptGameDataOption.Content = LocaleService.Get("PACK_ENCRYPT_GAME_DATA");
        LuacOption.Content = LocaleService.Get("PACK_USE_LUAC");
        EncryptShadersOption.Content = LocaleService.Get("PACK_ENCRYPT_SHADERS");
        EncryptDataOption.Content = LocaleService.Get("PACK_ENCRYPT_DATA");
        EncryptSavesOption.Content = LocaleService.Get("PACK_ENCRYPT_SAVES");
        UseLdPakOption.Content = LocaleService.Get("PACK_USE_LDPAK");
        ConfirmButton.Content = LocaleService.Get("CONFIRM");
        CancelButton.Content = LocaleService.Get("CANCEL");
        EncryptSavesOption.IsEnabled = !isStandalone;
        if (isStandalone)
        {
            ToolTip.SetTip(
                EncryptSavesOption,
                LocaleService.Get("PACK_ENCRYPT_SAVES_CPP_REQUIRED"));
        }
        MacOSOption.IsCheckedChanged += (_, _) => updateIosDetailsVisibility();
        IosOption.IsCheckedChanged += (_, _) => updateIosDetailsVisibility();
        HarmonyOption.IsCheckedChanged += (_, _) => updateHarmonyDeviceVisibility();
        HarmonyMobileOption.IsCheckedChanged += (_, _) => updateHarmonyDeviceVisibility();
        HarmonyTwoInOneOption.IsCheckedChanged += (_, _) => updateHarmonyDeviceVisibility();
        AndroidOption.IsCheckedChanged += (_, _) => updateAndroidSigningVisibility();
        EncryptGameDataOption.Click += (_, _) => toggleEncryptionOptions();
        LuacOption.IsCheckedChanged += (_, _) => updateEncryptionOptionsState();
        EncryptShadersOption.IsCheckedChanged += (_, _) => updateEncryptionOptionsState();
        EncryptDataOption.IsCheckedChanged += (_, _) => updateEncryptionOptionsState();
        EncryptSavesOption.IsCheckedChanged += (_, _) => updateEncryptionOptionsState();
        updateEncryptionOptionsState();
        Win32Option.IsVisible = OperatingSystem.IsWindows();
        MacOSOption.IsVisible = OperatingSystem.IsMacOS();
        bool sourceProjectOnMacOS = OperatingSystem.IsMacOS() && !isStandalone;
        IosPanel.IsVisible = sourceProjectOnMacOS;
        HarmonyPanel.IsVisible = sourceProjectOnMacOS
            && hasDevEcoStudio();
        AndroidPanel.IsVisible = sourceProjectOnMacOS
            && hasAndroidStudio();
        if (sourceProjectOnMacOS)
            MinHeight = 387;
        if (OperatingSystem.IsWindows())
            Win32Option.IsChecked = true;
        else if (OperatingSystem.IsMacOS())
            MacOSOption.IsChecked = true;
        else
            ConfirmButton.IsEnabled = false;
        Opened += (_, _) =>
        {
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

    private async void onConfirm(object? sender, RoutedEventArgs args)
    {
        if (Win32Option.IsChecked == true)
            Close(new ProjectPackOptions(
                ProjectPackPlatform.Win32,
                LuacOption.IsChecked == true,
                EncryptShadersOption.IsChecked == true,
                EncryptDataOption.IsChecked == true,
                EncryptSavesOption.IsChecked == true,
                UseLdPakOption.IsChecked == true));
        else if (MacOSOption.IsChecked == true)
            Close(new ProjectPackOptions(
                ProjectPackPlatform.MacOS,
                LuacOption.IsChecked == true,
                EncryptShadersOption.IsChecked == true,
                EncryptDataOption.IsChecked == true,
                EncryptSavesOption.IsChecked == true,
                UseLdPakOption.IsChecked == true));
        else if (IosOption.IsChecked == true)
            Close(new ProjectPackOptions(
                ProjectPackPlatform.IOS,
                LuacOption.IsChecked == true,
                EncryptShadersOption.IsChecked == true,
                EncryptDataOption.IsChecked == true,
                EncryptSavesOption.IsChecked == true,
                UseLdPakOption.IsChecked == true)
            {
                ExportToIPhone = ExportToIPhoneOption.IsChecked == true,
            });
        else if (HarmonyOption.IsChecked == true)
        {
            HarmonyDeviceForm deviceForm = HarmonyTwoInOneOption.IsChecked == true
                ? HarmonyDeviceForm.TwoInOne
                : HarmonyDeviceForm.Mobile;
            HarmonyGraphicsApi graphicsApi = deviceForm == HarmonyDeviceForm.Mobile
                ? HarmonyGraphicsApi.OpenGLES
                : HarmonyOpenGLESOption.IsChecked == true
                    ? HarmonyGraphicsApi.OpenGLES
                    : HarmonyGraphicsApi.OpenGL;
            Close(new ProjectPackOptions(
                ProjectPackPlatform.HarmonyOS,
                LuacOption.IsChecked == true,
                EncryptShadersOption.IsChecked == true,
                EncryptDataOption.IsChecked == true,
                EncryptSavesOption.IsChecked == true,
                UseLdPakOption.IsChecked == true)
            {
                HarmonyDeviceForm = deviceForm,
                HarmonyGraphicsApi = graphicsApi,
                ExportToHarmonyDevice = ExportToHarmonyDeviceOption.IsChecked == true,
            });
        }
        else if (AndroidOption.IsChecked == true)
        {
            AndroidSigningOptions? signing = null;
            if (AndroidSigningOption.IsChecked == true)
            {
                AndroidSigningDialog signingDialog = new();
                signing = await signingDialog.ShowDialog<AndroidSigningOptions?>(this);
                if (signing is null)
                    return;
            }
            Close(new ProjectPackOptions(
                ProjectPackPlatform.Android,
                LuacOption.IsChecked == true,
                EncryptShadersOption.IsChecked == true,
                EncryptDataOption.IsChecked == true,
                EncryptSavesOption.IsChecked == true,
                UseLdPakOption.IsChecked == true)
            {
                AndroidSigning = signing,
            });
        }
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
    }

    private void updateIosDetailsVisibility()
    {
        bool isIosSelected = IosOption.IsVisible
            && IosOption.IsChecked == true;
        IosStatusText.IsVisible = isIosSelected;
        ExportToIPhoneOption.IsVisible = isIosSelected;
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close((ProjectPackOptions?)null);
    }
}
