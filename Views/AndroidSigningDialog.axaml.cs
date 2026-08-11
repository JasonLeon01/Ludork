using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;

namespace Ludork.Views;

public partial class AndroidSigningDialog : Window
{
    public AndroidSigningDialog()
    {
        InitializeComponent();
        Title = LocaleService.Get("PACK_ANDROID_SIGNING_TITLE");
        DescriptionText.Text = LocaleService.Get("PACK_ANDROID_SIGNING_DESCRIPTION");
        KeystoreLabel.Text = LocaleService.Get("PACK_ANDROID_KEYSTORE");
        KeyAliasLabel.Text = LocaleService.Get("PACK_ANDROID_KEY_ALIAS");
        KeystorePasswordLabel.Text = LocaleService.Get("PACK_ANDROID_KEYSTORE_PASSWORD");
        SameKeyPasswordOption.Content = LocaleService.Get("PACK_ANDROID_KEY_PASSWORD_SAME");
        KeyPasswordLabel.Text = LocaleService.Get("PACK_ANDROID_KEY_PASSWORD");
        BrowseButton.Content = LocaleService.Get("BROWSE");
        ConfirmButton.Content = LocaleService.Get("CONFIRM");
        CancelButton.Content = LocaleService.Get("CANCEL");
        EditorInputs.ApplyReadOnly(KeystorePathBox);
        EditorInputs.ApplyEditable(KeyAliasBox);
        EditorInputs.ApplyEditable(KeystorePasswordBox);
        EditorInputs.ApplyEditable(KeyPasswordBox);
        KeystorePathBox.TextChanged += (_, _) => updateValidation();
        KeyAliasBox.TextChanged += (_, _) => updateValidation();
        KeystorePasswordBox.TextChanged += (_, _) => updateValidation();
        KeyPasswordBox.TextChanged += (_, _) => updateValidation();
        SameKeyPasswordOption.IsCheckedChanged += (_, _) => updateKeyPasswordVisibility();
        Opened += (_, _) => BrowseButton.Focus();
        updateKeyPasswordVisibility();
    }

    private async void onBrowse(object? sender, RoutedEventArgs args)
    {
        IStorageFolder? startLocation = await getStartLocationAsync();
        IReadOnlyList<IStorageFile> files = await StorageProvider.OpenFilePickerAsync(
            new FilePickerOpenOptions
            {
                Title = LocaleService.Get("PACK_ANDROID_SELECT_KEYSTORE"),
                AllowMultiple = false,
                SuggestedStartLocation = startLocation,
                FileTypeFilter =
                [
                    new FilePickerFileType(LocaleService.Get("PACK_ANDROID_KEYSTORE_FILES"))
                    {
                        Patterns = ["*.jks", "*.keystore", "*.p12", "*.pfx", "*.pkcs12"],
                    },
                    new FilePickerFileType(LocaleService.Get("PACK_ANDROID_ALL_FILES"))
                    {
                        Patterns = ["*"],
                    },
                ],
            });
        if (files.Count == 0)
            return;
        KeystorePathBox.Text = files[0].Path.LocalPath;
    }

    private async System.Threading.Tasks.Task<IStorageFolder?> getStartLocationAsync()
    {
        string currentPath = KeystorePathBox.Text ?? string.Empty;
        string? directory = Path.GetDirectoryName(currentPath);
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
            directory = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        return await StorageProvider.TryGetFolderFromPathAsync(
            new Uri(Path.GetFullPath(directory)));
    }

    private void updateKeyPasswordVisibility()
    {
        KeyPasswordPanel.IsVisible = SameKeyPasswordOption.IsChecked != true;
        updateValidation();
    }

    private void updateValidation()
    {
        string? validationMessage = getValidationMessage();
        ValidationText.Text = validationMessage ?? string.Empty;
        ConfirmButton.IsEnabled = validationMessage is null;
    }

    private string? getValidationMessage()
    {
        string keystorePath = KeystorePathBox.Text ?? string.Empty;
        if (!isReadableAbsoluteFile(keystorePath))
            return LocaleService.Get("PACK_ANDROID_KEYSTORE_INVALID");
        if (string.IsNullOrWhiteSpace(KeyAliasBox.Text))
            return LocaleService.Get("PACK_ANDROID_KEY_ALIAS_REQUIRED");
        if (hasLineBreak(KeyAliasBox.Text))
            return LocaleService.Get("PACK_ANDROID_KEY_ALIAS_INVALID");

        string keystorePassword = KeystorePasswordBox.Text ?? string.Empty;
        if (keystorePassword.Length == 0)
            return LocaleService.Get("PACK_ANDROID_KEYSTORE_PASSWORD_REQUIRED");
        if (hasLineBreak(keystorePassword))
            return LocaleService.Get("PACK_ANDROID_PASSWORD_LINE_BREAK");

        if (SameKeyPasswordOption.IsChecked != true)
        {
            string keyPassword = KeyPasswordBox.Text ?? string.Empty;
            if (keyPassword.Length == 0)
                return LocaleService.Get("PACK_ANDROID_KEY_PASSWORD_REQUIRED");
            if (hasLineBreak(keyPassword))
                return LocaleService.Get("PACK_ANDROID_PASSWORD_LINE_BREAK");
        }
        return null;
    }

    private static bool isReadableAbsoluteFile(string path)
    {
        if (string.IsNullOrWhiteSpace(path)
            || !Path.IsPathFullyQualified(path)
            || !File.Exists(path))
        {
            return false;
        }
        try
        {
            using FileStream stream = File.Open(
                path,
                FileMode.Open,
                FileAccess.Read,
                FileShare.ReadWrite | FileShare.Delete);
            return stream.CanRead;
        }
        catch (UnauthorizedAccessException)
        {
            return false;
        }
        catch (IOException)
        {
            return false;
        }
    }

    private static bool hasLineBreak(string value)
    {
        return value.IndexOfAny(['\r', '\n']) >= 0;
    }

    private void onConfirm(object? sender, RoutedEventArgs args)
    {
        if (getValidationMessage() is not null)
        {
            updateValidation();
            return;
        }

        string keystorePassword = KeystorePasswordBox.Text ?? string.Empty;
        string keyPassword = SameKeyPasswordOption.IsChecked == true
            ? keystorePassword
            : KeyPasswordBox.Text ?? string.Empty;
        Close(new AndroidSigningOptions(
            Path.GetFullPath(KeystorePathBox.Text!),
            KeyAliasBox.Text!.Trim(),
            keystorePassword,
            keyPassword));
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close((AndroidSigningOptions?)null);
    }
}
