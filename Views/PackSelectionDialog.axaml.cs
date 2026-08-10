using Avalonia.Controls;
using Avalonia.Interactivity;
using Ludork.Services;
using System;
using System.IO;

namespace Ludork.Views;

public partial class PackSelectionDialog : Window
{
    private const string DevEcoStudioPath = "/Applications/DevEco-Studio.app";
    private const string DevEcoStudioEnvironment = "LUDORK_DEVECO_STUDIO";

    public PackSelectionDialog() : this(true)
    {
    }

    public PackSelectionDialog(bool isStandalone)
    {
        InitializeComponent();
        Title = LocaleService.Get("PACK_MODE_TITLE");
        DescriptionText.Text = LocaleService.Get("PACK_MODE_DESC");
        Win32Option.Content = LocaleService.Get("PACK_PLATFORM_WIN32");
        MacOSOption.Content = LocaleService.Get("PACK_PLATFORM_MACOS");
        IosOption.Content = LocaleService.Get("PACK_PLATFORM_IOS");
        IosStatusText.Text = LocaleService.Get("PACK_IOS_REQUIREMENTS");
        HarmonyOption.Content = LocaleService.Get("PACK_PLATFORM_HARMONYOS");
        HarmonyMobileOption.Content = LocaleService.Get("PACK_HARMONY_DEVICE_MOBILE");
        HarmonyTwoInOneOption.Content = LocaleService.Get("PACK_HARMONY_DEVICE_TWO_IN_ONE");
        HarmonyTwoInOneStatusText.Text = LocaleService.Get("PACK_HARMONY_DEVICE_TEMPORARILY_UNAVAILABLE");
        ExportToHarmonyDeviceOption.Content = LocaleService.Get("PACK_EXPORT_TO_HARMONY_DEVICE");
        ExportToIPhoneOption.Content = LocaleService.Get("PACK_EXPORT_TO_IPHONE");
        LuacOption.Content = LocaleService.Get("PACK_USE_LUAC");
        EncryptShadersOption.Content = LocaleService.Get("PACK_ENCRYPT_SHADERS");
        EncryptDataOption.Content = LocaleService.Get("PACK_ENCRYPT_DATA");
        ConfirmButton.Content = LocaleService.Get("CONFIRM");
        CancelButton.Content = LocaleService.Get("CANCEL");
        MacOSOption.IsCheckedChanged += (_, _) => updateExportToIPhoneVisibility();
        IosOption.IsCheckedChanged += (_, _) => updateExportToIPhoneVisibility();
        HarmonyOption.IsCheckedChanged += (_, _) => updateHarmonyDeviceVisibility();
        HarmonyMobileOption.IsCheckedChanged += (_, _) => updateHarmonyDeviceVisibility();
        Win32Option.IsVisible = OperatingSystem.IsWindows();
        MacOSOption.IsVisible = OperatingSystem.IsMacOS();
        bool sourceProjectOnMacOS = OperatingSystem.IsMacOS() && !isStandalone;
        IosPanel.IsVisible = sourceProjectOnMacOS;
        HarmonyPanel.IsVisible = sourceProjectOnMacOS
            && hasDevEcoStudio();
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

    private void onConfirm(object? sender, RoutedEventArgs args)
    {
        if (Win32Option.IsChecked == true)
            Close(new ProjectPackOptions(
                ProjectPackPlatform.Win32,
                LuacOption.IsChecked == true,
                EncryptShadersOption.IsChecked == true,
                EncryptDataOption.IsChecked == true));
        else if (MacOSOption.IsChecked == true)
            Close(new ProjectPackOptions(
                ProjectPackPlatform.MacOS,
                LuacOption.IsChecked == true,
                EncryptShadersOption.IsChecked == true,
                EncryptDataOption.IsChecked == true));
        else if (IosOption.IsChecked == true)
            Close(new ProjectPackOptions(
                ProjectPackPlatform.IOS,
                LuacOption.IsChecked == true,
                EncryptShadersOption.IsChecked == true,
                EncryptDataOption.IsChecked == true)
            {
                ExportToIPhone = ExportToIPhoneOption.IsChecked == true,
            });
        else if (HarmonyOption.IsChecked == true
            && HarmonyMobileOption.IsChecked == true)
        {
            Close(new ProjectPackOptions(
                ProjectPackPlatform.HarmonyOS,
                LuacOption.IsChecked == true,
                EncryptShadersOption.IsChecked == true,
                EncryptDataOption.IsChecked == true)
            {
                HarmonyDeviceForm = HarmonyDeviceForm.Mobile,
                ExportToHarmonyDevice = ExportToHarmonyDeviceOption.IsChecked == true,
            });
        }
    }

    private void updateHarmonyDeviceVisibility()
    {
        HarmonyDevicePanel.IsVisible = HarmonyOption.IsVisible
            && HarmonyOption.IsChecked == true;
        ExportToHarmonyDeviceOption.IsVisible = HarmonyDevicePanel.IsVisible
            && HarmonyMobileOption.IsChecked == true;
    }

    private void updateExportToIPhoneVisibility()
    {
        ExportToIPhoneOption.IsVisible = OperatingSystem.IsMacOS()
            && IosOption.IsChecked == true;
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close((ProjectPackOptions?)null);
    }
}
