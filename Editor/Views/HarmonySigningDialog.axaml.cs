using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform;
using Avalonia.Platform.Storage;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views;

public partial class HarmonySigningDialog : Window
{
    private readonly HarmonySigningCredentialStore credentialStore = new();
    private readonly string? projectPath;
    private HarmonySigningOptions? loadedSigning;
    private bool credentialOperationPending;
    private bool closed;

    public HarmonySigningDialog() : this(null)
    {
    }

    public HarmonySigningDialog(string? projectPath)
    {
        this.projectPath = projectPath;
        InitializeComponent();
        Title = LocaleService.Get("PACK_HARMONY_SIGNING_TITLE");
        DescriptionText.Text = LocaleService.Get("PACK_HARMONY_SIGNING_DESCRIPTION");
        KeystoreLabel.Text = LocaleService.Get("PACK_HARMONY_KEYSTORE");
        CertificateLabel.Text = LocaleService.Get("PACK_HARMONY_CERTIFICATE");
        ProfileLabel.Text = LocaleService.Get("PACK_HARMONY_PROFILE");
        KeyAliasLabel.Text = LocaleService.Get("PACK_HARMONY_KEY_ALIAS");
        KeystorePasswordLabel.Text = LocaleService.Get("PACK_HARMONY_KEYSTORE_PASSWORD");
        SameKeyPasswordOption.Content = LocaleService.Get("PACK_HARMONY_KEY_PASSWORD_SAME");
        KeyPasswordLabel.Text = LocaleService.Get("PACK_HARMONY_KEY_PASSWORD");
        SaveSigningOption.Content = LocaleService.Get("PACK_HARMONY_SAVE_SIGNING");
        BrowseButton.Content = LocaleService.Get("BROWSE");
        CertificateBrowseButton.Content = LocaleService.Get("BROWSE");
        ProfileBrowseButton.Content = LocaleService.Get("BROWSE");
        ConfirmButton.Content = LocaleService.Get("CONFIRM");
        CancelButton.Content = LocaleService.Get("CANCEL");
        EditorInputs.ApplyReadOnly(KeystorePathBox);
        EditorInputs.ApplyReadOnly(CertificatePathBox);
        EditorInputs.ApplyReadOnly(ProfilePathBox);
        EditorInputs.ApplyEditable(KeyAliasBox);
        EditorInputs.ApplyEditable(KeystorePasswordBox);
        EditorInputs.ApplyEditable(KeyPasswordBox);
        KeystorePathBox.TextChanged += (_, _) => updateValidation();
        CertificatePathBox.TextChanged += (_, _) => updateValidation();
        ProfilePathBox.TextChanged += (_, _) => updateValidation();
        KeyAliasBox.TextChanged += (_, _) => updateValidation();
        KeystorePasswordBox.TextChanged += (_, _) => updateValidation();
        KeyPasswordBox.TextChanged += (_, _) => updateValidation();
        SameKeyPasswordOption.IsCheckedChanged += (_, _) => updateKeyPasswordVisibility();
        Opened += (_, _) =>
        {
            Screen? screen = Screens.ScreenFromWindow(this) ?? Screens.Primary;
            if (screen is not null)
                MaxHeight = Math.Max(400, screen.WorkingArea.Height / screen.Scaling - 80);
            BrowseButton.Focus();
        };
        Closed += (_, _) => closed = true;
        updateKeyPasswordVisibility();
    }

    private async void onBrowse(object? sender, RoutedEventArgs args)
    {
        string? path = await selectFileAsync(KeystorePathBox.Text,
            "PACK_HARMONY_SELECT_KEYSTORE", "PACK_HARMONY_KEYSTORE_FILES",
            ["*.p12", "*.pfx", "*.pkcs12"]);
        if (path is null || closed)
            return;
        KeystorePathBox.Text = path;
        await loadSavedSigningAsync(path);
    }

    private async void onBrowseCertificate(object? sender, RoutedEventArgs args)
    {
        string? path = await selectFileAsync(CertificatePathBox.Text,
            "PACK_HARMONY_SELECT_CERTIFICATE", "PACK_HARMONY_CERTIFICATE_FILES",
            ["*.cer", "*.crt", "*.pem"]);
        if (path is not null && !closed)
            CertificatePathBox.Text = path;
    }

    private async void onBrowseProfile(object? sender, RoutedEventArgs args)
    {
        string? path = await selectFileAsync(ProfilePathBox.Text,
            "PACK_HARMONY_SELECT_PROFILE", "PACK_HARMONY_PROFILE_FILES",
            ["*.p7b"]);
        if (path is not null && !closed)
            ProfilePathBox.Text = path;
    }

    private async Task<string?> selectFileAsync(
        string? currentPath, string titleKey, string fileTypeKey, string[] patterns)
    {
        string? directory = Path.GetDirectoryName(currentPath);
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
            directory = Path.GetDirectoryName(KeystorePathBox.Text);
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
            directory = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        IStorageFolder? startLocation = await StorageProvider.TryGetFolderFromPathAsync(
            new Uri(Path.GetFullPath(directory)));
        IReadOnlyList<IStorageFile> files = await StorageProvider.OpenFilePickerAsync(
            new FilePickerOpenOptions
            {
                Title = LocaleService.Get(titleKey),
                AllowMultiple = false,
                SuggestedStartLocation = startLocation,
                FileTypeFilter =
                [
                    new FilePickerFileType(LocaleService.Get(fileTypeKey)) { Patterns = patterns },
                    new FilePickerFileType(LocaleService.Get("PACK_HARMONY_ALL_FILES")) { Patterns = ["*"] },
                ],
            });
        return files.Count == 0
            ? null
            : Path.GetFullPath(files[0].Path.LocalPath).Normalize(NormalizationForm.FormC);
    }

    private void updateKeyPasswordVisibility()
    {
        KeyPasswordPanel.IsVisible = SameKeyPasswordOption.IsChecked != true;
        updateValidation();
    }

    private void updateValidation()
    {
        SigningPanel.IsEnabled = !credentialOperationPending;
        string? validationMessage = credentialOperationPending ? null : getValidationMessage();
        ValidationText.Text = validationMessage ?? string.Empty;
        ConfirmButton.IsEnabled = !credentialOperationPending && validationMessage is null;
    }

    private string? getValidationMessage()
    {
        string keystorePath = KeystorePathBox.Text ?? string.Empty;
        string certificatePath = CertificatePathBox.Text ?? string.Empty;
        string profilePath = ProfilePathBox.Text ?? string.Empty;
        if (!HarmonySigningInput.IsReadableFile(keystorePath))
            return LocaleService.Get("PACK_HARMONY_KEYSTORE_INVALID");
        if (!HarmonySigningInput.IsReadableFile(certificatePath))
            return LocaleService.Get("PACK_HARMONY_CERTIFICATE_INVALID");
        if (!HarmonySigningInput.IsReadableFile(profilePath))
            return LocaleService.Get("PACK_HARMONY_PROFILE_INVALID");
        if (!HarmonySigningInput.IsOutsideProject(keystorePath, projectPath)
            || !HarmonySigningInput.IsOutsideProject(certificatePath, projectPath)
            || !HarmonySigningInput.IsOutsideProject(profilePath, projectPath))
        {
            return LocaleService.Get("PACK_HARMONY_MATERIALS_OUTSIDE_PROJECT");
        }
        if (string.IsNullOrWhiteSpace(KeyAliasBox.Text))
            return LocaleService.Get("PACK_HARMONY_KEY_ALIAS_REQUIRED");
        if (HarmonySigningInput.HasLineBreak(KeyAliasBox.Text))
            return LocaleService.Get("PACK_HARMONY_KEY_ALIAS_INVALID");
        string keystorePassword = KeystorePasswordBox.Text ?? string.Empty;
        if (keystorePassword.Length == 0)
            return LocaleService.Get("PACK_HARMONY_KEYSTORE_PASSWORD_REQUIRED");
        if (HarmonySigningInput.HasLineBreak(keystorePassword))
            return LocaleService.Get("PACK_HARMONY_PASSWORD_LINE_BREAK");
        if (SameKeyPasswordOption.IsChecked != true)
        {
            string keyPassword = KeyPasswordBox.Text ?? string.Empty;
            if (keyPassword.Length == 0)
                return LocaleService.Get("PACK_HARMONY_KEY_PASSWORD_REQUIRED");
            if (HarmonySigningInput.HasLineBreak(keyPassword))
                return LocaleService.Get("PACK_HARMONY_PASSWORD_LINE_BREAK");
        }
        return null;
    }

    private async void onConfirm(object? sender, RoutedEventArgs args)
    {
        if (credentialOperationPending || getValidationMessage() is not null)
        {
            updateValidation();
            return;
        }
        string keystorePassword = KeystorePasswordBox.Text ?? string.Empty;
        HarmonySigningOptions signing = new(
            Path.GetFullPath(KeystorePathBox.Text!),
            Path.GetFullPath(CertificatePathBox.Text!),
            Path.GetFullPath(ProfilePathBox.Text!),
            KeyAliasBox.Text!.Trim(),
            keystorePassword,
            SameKeyPasswordOption.IsChecked == true ? keystorePassword : KeyPasswordBox.Text ?? string.Empty);
        if (SaveSigningOption.IsChecked == true || (loadedSigning is not null && signing != loadedSigning))
        {
            credentialOperationPending = true;
            CredentialStatusText.Text = string.Empty;
            updateValidation();
            try
            {
                await credentialStore.SaveAsync(signing, CancellationToken.None);
            }
            catch (Exception exception) when (
                exception is IOException or UnauthorizedAccessException or InvalidDataException
                or System.Text.Json.JsonException or InvalidOperationException or Win32Exception
                or ArgumentException or NotSupportedException)
            {
                loadedSigning = null;
                SaveSigningOption.IsChecked = false;
                SaveSigningOption.IsVisible = true;
                CredentialStatusText.Text = LocaleService.Get("PACK_HARMONY_SIGNING_SAVE_FAILED");
                credentialOperationPending = false;
                updateValidation();
                return;
            }
        }
        if (!closed)
            Close(signing);
    }

    private async Task loadSavedSigningAsync(string keystorePath)
    {
        loadedSigning = null;
        CertificatePathBox.Text = string.Empty;
        ProfilePathBox.Text = string.Empty;
        KeyAliasBox.Text = string.Empty;
        KeystorePasswordBox.Text = string.Empty;
        KeyPasswordBox.Text = string.Empty;
        SameKeyPasswordOption.IsChecked = true;
        SaveSigningOption.IsChecked = false;
        SaveSigningOption.IsVisible = false;
        CredentialStatusText.Text = string.Empty;
        credentialOperationPending = true;
        updateValidation();
        HarmonySigningOptions? signing;
        try
        {
            signing = await credentialStore.FindAsync(keystorePath, CancellationToken.None);
        }
        catch (Exception exception) when (
            exception is IOException or UnauthorizedAccessException or InvalidDataException
            or System.Text.Json.JsonException or InvalidOperationException or Win32Exception
            or ArgumentException or NotSupportedException)
        {
            CredentialStatusText.Text = LocaleService.Get("PACK_HARMONY_SIGNING_LOAD_FAILED");
            signing = null;
        }
        if (closed)
            return;
        loadedSigning = signing;
        SaveSigningOption.IsVisible = signing is null;
        if (signing is not null)
        {
            CertificatePathBox.Text = signing.CertificatePath;
            ProfilePathBox.Text = signing.ProfilePath;
            KeyAliasBox.Text = signing.KeyAlias;
            KeystorePasswordBox.Text = signing.KeystorePassword;
            bool samePassword = string.Equals(signing.KeystorePassword, signing.KeyPassword, StringComparison.Ordinal);
            SameKeyPasswordOption.IsChecked = samePassword;
            KeyPasswordBox.Text = samePassword ? string.Empty : signing.KeyPassword;
        }
        credentialOperationPending = false;
        updateKeyPasswordVisibility();
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close((HarmonySigningOptions?)null);
    }
}
