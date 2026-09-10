using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Text;
using System.Threading;

namespace Ludork.Views;

public partial class AndroidSigningDialog : Window
{
    private readonly AndroidSigningCredentialStore credentialStore = new();
    private AndroidSigningOptions? loadedSigning;
    private bool credentialOperationPending;
    private int keystoreSelectionGeneration;

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
        SaveSigningOption.Content = LocaleService.Get("PACK_ANDROID_SAVE_SIGNING");
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
        string keystorePath = Path.GetFullPath(files[0].Path.LocalPath)
            .Normalize(NormalizationForm.FormC);
        KeystorePathBox.Text = keystorePath;
        await loadSavedSigningAsync(keystorePath);
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
        if (credentialOperationPending)
        {
            ValidationText.Text = string.Empty;
            ConfirmButton.IsEnabled = false;
            return;
        }
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

    private async void onConfirm(object? sender, RoutedEventArgs args)
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
        AndroidSigningOptions signing = new(
            Path.GetFullPath(KeystorePathBox.Text!),
            KeyAliasBox.Text!.Trim(),
            keystorePassword,
            keyPassword);
        bool updateStoredSigning = loadedSigning is not null
            && signing != loadedSigning;
        if (SaveSigningOption.IsChecked == true || updateStoredSigning)
        {
            credentialOperationPending = true;
            CredentialStatusText.Text = string.Empty;
            updateValidation();
            try
            {
                await credentialStore.SaveAsync(signing, CancellationToken.None);
            }
            catch (Exception exception) when (
                exception is IOException
                or UnauthorizedAccessException
                or InvalidDataException
                or System.Text.Json.JsonException
                or InvalidOperationException
                or Win32Exception
                or ArgumentException
                or NotSupportedException)
            {
                loadedSigning = null;
                SaveSigningOption.IsChecked = false;
                SaveSigningOption.IsVisible = true;
                CredentialStatusText.Text = string.Format(
                    LocaleService.Get("PACK_ANDROID_SIGNING_SAVE_FAILED"),
                    exception.Message);
                credentialOperationPending = false;
                updateValidation();
                return;
            }
        }
        Close(signing);
    }

    private async System.Threading.Tasks.Task loadSavedSigningAsync(string keystorePath)
    {
        int generation = ++keystoreSelectionGeneration;
        loadedSigning = null;
        KeyAliasBox.Text = string.Empty;
        KeystorePasswordBox.Text = string.Empty;
        KeyPasswordBox.Text = string.Empty;
        SameKeyPasswordOption.IsChecked = true;
        SaveSigningOption.IsChecked = false;
        SaveSigningOption.IsVisible = false;
        CredentialStatusText.Text = string.Empty;
        credentialOperationPending = true;
        updateValidation();
        AndroidSigningOptions? signing;
        try
        {
            signing = await credentialStore.FindAsync(
                keystorePath,
                CancellationToken.None);
        }
        catch (Exception exception) when (
            exception is IOException
            or UnauthorizedAccessException
            or InvalidDataException
            or System.Text.Json.JsonException
            or InvalidOperationException
            or Win32Exception
            or ArgumentException
            or NotSupportedException)
        {
            if (generation != keystoreSelectionGeneration)
                return;
            CredentialStatusText.Text = string.Format(
                LocaleService.Get("PACK_ANDROID_SIGNING_LOAD_FAILED"),
                exception.Message);
            signing = null;
        }
        if (generation != keystoreSelectionGeneration)
            return;
        loadedSigning = signing;
        if (signing is null)
        {
            SaveSigningOption.IsVisible = true;
        }
        else
        {
            KeyAliasBox.Text = signing.KeyAlias;
            KeystorePasswordBox.Text = signing.KeystorePassword;
            bool samePassword = string.Equals(
                signing.KeystorePassword,
                signing.KeyPassword,
                StringComparison.Ordinal);
            SameKeyPasswordOption.IsChecked = samePassword;
            KeyPasswordBox.Text = samePassword
                ? string.Empty
                : signing.KeyPassword;
        }
        credentialOperationPending = false;
        updateKeyPasswordVisibility();
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close((AndroidSigningOptions?)null);
    }
}
