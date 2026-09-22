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
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed record MacOSSigningSelection(MacOSSigningOptions? Options)
{
    public static MacOSSigningSelection UseEnvironment { get; } =
        new((MacOSSigningOptions?)null);
}

public partial class MacOSSigningDialog : Window
{
    private const string AppleIdMode = "apple-id";
    private const string ApiKeyMode = "api-key";
    private readonly AppleSigningCredentialStore<MacOSStoredSigning> credentialStore =
        new("macos-signing.json", "editor/macos-signing");
    private StoredAppleSigning<MacOSStoredSigning>? loadedSigning;
    private bool credentialOperationPending;
    private int certificateSelectionGeneration;

    public MacOSSigningDialog()
    {
        InitializeComponent();
        Title = LocaleService.Get("PACK_MACOS_SIGNING_TITLE");
        DescriptionText.Text = LocaleService.Get("PACK_MACOS_SIGNING_DESCRIPTION");
        EnvironmentSigningOption.Content = LocaleService.Get("PACK_SIGNING_USE_ENVIRONMENT");
        SigningIdentityLabel.Text = LocaleService.Get("PACK_SIGNING_IDENTITY");
        CertificateLabel.Text = LocaleService.Get("PACK_SIGNING_CERTIFICATE");
        CertificatePasswordLabel.Text = LocaleService.Get("PACK_SIGNING_CERTIFICATE_PASSWORD");
        NotarizeOption.Content = LocaleService.Get("PACK_MACOS_NOTARIZE");
        AppleIdOption.Content = LocaleService.Get("PACK_MACOS_NOTARY_APPLE_ID_MODE");
        ApiKeyOption.Content = LocaleService.Get("PACK_MACOS_NOTARY_API_KEY_MODE");
        NotaryAppleIdLabel.Text = LocaleService.Get("PACK_MACOS_NOTARY_APPLE_ID");
        NotaryTeamIdLabel.Text = LocaleService.Get("PACK_MACOS_NOTARY_TEAM_ID");
        NotaryPasswordLabel.Text = LocaleService.Get("PACK_MACOS_NOTARY_PASSWORD");
        NotaryKeyLabel.Text = LocaleService.Get("PACK_MACOS_NOTARY_KEY");
        NotaryKeyIdLabel.Text = LocaleService.Get("PACK_MACOS_NOTARY_KEY_ID");
        NotaryKeyIssuerLabel.Text = LocaleService.Get("PACK_MACOS_NOTARY_KEY_ISSUER");
        SaveSigningOption.Content = LocaleService.Get("PACK_SIGNING_SAVE");
        BrowseCertificateButton.Content = LocaleService.Get("BROWSE");
        BrowseNotaryKeyButton.Content = LocaleService.Get("BROWSE");
        ConfirmButton.Content = LocaleService.Get("CONFIRM");
        CancelButton.Content = LocaleService.Get("CANCEL");
        EditorInputs.ApplyReadOnly(CertificatePathBox);
        EditorInputs.ApplyReadOnly(NotaryKeyPathBox);
        EditorInputs.ApplyEditable(SigningIdentityBox);
        EditorInputs.ApplyEditable(CertificatePasswordBox);
        EditorInputs.ApplyEditable(NotaryAppleIdBox);
        EditorInputs.ApplyEditable(NotaryTeamIdBox);
        EditorInputs.ApplyEditable(NotaryPasswordBox);
        EditorInputs.ApplyEditable(NotaryKeyIdBox);
        EditorInputs.ApplyEditable(NotaryKeyIssuerBox);
        EnvironmentSigningOption.IsCheckedChanged += (_, _) => updateSigningVisibility();
        NotarizeOption.IsCheckedChanged += (_, _) => updateNotarizationVisibility();
        AppleIdOption.IsCheckedChanged += (_, _) => updateNotarizationVisibility();
        ApiKeyOption.IsCheckedChanged += (_, _) => updateNotarizationVisibility();
        SigningIdentityBox.TextChanged += (_, _) => updateValidation();
        CertificatePathBox.TextChanged += (_, _) => updateValidation();
        CertificatePasswordBox.TextChanged += (_, _) => updateValidation();
        NotaryAppleIdBox.TextChanged += (_, _) => updateValidation();
        NotaryTeamIdBox.TextChanged += (_, _) => updateValidation();
        NotaryPasswordBox.TextChanged += (_, _) => updateValidation();
        NotaryKeyPathBox.TextChanged += (_, _) => updateValidation();
        NotaryKeyIdBox.TextChanged += (_, _) => updateValidation();
        NotaryKeyIssuerBox.TextChanged += (_, _) => updateValidation();
        Opened += (_, _) => ConfirmButton.Focus();
        updateSigningVisibility();
        updateNotarizationVisibility();
    }

    private async void onBrowseCertificate(object? sender, RoutedEventArgs args)
    {
        IStorageFolder? startLocation = await getStartLocationAsync(CertificatePathBox.Text);
        IReadOnlyList<IStorageFile> files = await StorageProvider.OpenFilePickerAsync(
            new FilePickerOpenOptions
            {
                Title = LocaleService.Get("PACK_SIGNING_SELECT_CERTIFICATE"),
                AllowMultiple = false,
                SuggestedStartLocation = startLocation,
                FileTypeFilter =
                [
                    new FilePickerFileType(LocaleService.Get("PACK_SIGNING_CERTIFICATE_FILES"))
                    {
                        Patterns = ["*.p12", "*.pfx", "*.pkcs12"],
                    },
                    new FilePickerFileType(LocaleService.Get("PACK_SIGNING_ALL_FILES"))
                    {
                        Patterns = ["*"],
                    },
                ],
            });
        if (files.Count == 0)
            return;
        string certificatePath = Path.GetFullPath(files[0].Path.LocalPath)
            .Normalize(NormalizationForm.FormC);
        CertificatePathBox.Text = certificatePath;
        await loadSavedSigningAsync(certificatePath);
    }

    private async void onBrowseNotaryKey(object? sender, RoutedEventArgs args)
    {
        IStorageFolder? startLocation = await getStartLocationAsync(NotaryKeyPathBox.Text);
        IReadOnlyList<IStorageFile> files = await StorageProvider.OpenFilePickerAsync(
            new FilePickerOpenOptions
            {
                Title = LocaleService.Get("PACK_MACOS_SELECT_NOTARY_KEY"),
                AllowMultiple = false,
                SuggestedStartLocation = startLocation,
                FileTypeFilter =
                [
                    new FilePickerFileType(LocaleService.Get("PACK_MACOS_NOTARY_KEY_FILES"))
                    {
                        Patterns = ["*.p8"],
                    },
                    new FilePickerFileType(LocaleService.Get("PACK_SIGNING_ALL_FILES"))
                    {
                        Patterns = ["*"],
                    },
                ],
            });
        if (files.Count == 0)
            return;
        NotaryKeyPathBox.Text = Path.GetFullPath(files[0].Path.LocalPath)
            .Normalize(NormalizationForm.FormC);
        updateValidation();
    }

    private async Task<IStorageFolder?> getStartLocationAsync(string? currentPath)
    {
        string? directory = Path.GetDirectoryName(currentPath ?? string.Empty);
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
            directory = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        return await StorageProvider.TryGetFolderFromPathAsync(
            new Uri(Path.GetFullPath(directory)));
    }

    private void updateSigningVisibility()
    {
        SigningPanel.IsVisible = EnvironmentSigningOption.IsChecked != true;
        updateNotarizationVisibility();
    }

    private void updateNotarizationVisibility()
    {
        bool notarize = EnvironmentSigningOption.IsChecked != true
            && NotarizeOption.IsChecked == true;
        NotarizationPanel.IsVisible = notarize;
        bool apiKey = ApiKeyOption.IsChecked == true;
        ApiKeyPanel.IsVisible = notarize && apiKey;
        AppleIdPanel.IsVisible = notarize && !apiKey;
        updateValidation();
    }

    private void updateValidation()
    {
        bool environmentSigning = EnvironmentSigningOption.IsChecked == true;
        SaveSigningOption.IsVisible = !environmentSigning
            && !credentialOperationPending
            && AppleSigningInput.IsReadableFile(CertificatePathBox.Text ?? string.Empty);
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
        if (EnvironmentSigningOption.IsChecked == true)
            return null;
        string identity = SigningIdentityBox.Text ?? string.Empty;
        if (AppleSigningInput.HasLineBreak(identity))
            return LocaleService.Get("PACK_SIGNING_IDENTITY_LINE_BREAK");

        string certificatePath = CertificatePathBox.Text ?? string.Empty;
        string certificatePassword = CertificatePasswordBox.Text ?? string.Empty;
        if (certificatePath.Length != 0)
        {
            if (!AppleSigningInput.IsReadableFile(certificatePath))
                return LocaleService.Get("PACK_SIGNING_CERTIFICATE_INVALID");
            if (certificatePassword.Length == 0)
                return LocaleService.Get("PACK_SIGNING_CERTIFICATE_PASSWORD_REQUIRED");
            if (AppleSigningInput.HasLineBreak(certificatePassword))
                return LocaleService.Get("PACK_SIGNING_PASSWORD_LINE_BREAK");
        }

        if (NotarizeOption.IsChecked != true)
            return null;
        if (identity.Length == 0 && certificatePath.Length == 0)
            return LocaleService.Get("PACK_MACOS_SIGNING_IDENTITY_REQUIRED");
        if (ApiKeyOption.IsChecked == true)
        {
            if (!AppleSigningInput.IsReadableFile(NotaryKeyPathBox.Text ?? string.Empty))
                return LocaleService.Get("PACK_MACOS_NOTARY_KEY_REQUIRED");
            if (string.IsNullOrWhiteSpace(NotaryKeyIdBox.Text))
                return LocaleService.Get("PACK_MACOS_NOTARY_KEY_ID_REQUIRED");
            if (string.IsNullOrWhiteSpace(NotaryKeyIssuerBox.Text))
                return LocaleService.Get("PACK_MACOS_NOTARY_KEY_ISSUER_REQUIRED");
            return null;
        }
        if (string.IsNullOrWhiteSpace(NotaryAppleIdBox.Text))
            return LocaleService.Get("PACK_MACOS_NOTARY_APPLE_ID_REQUIRED");
        if (!AppleSigningInput.IsValidTeamId(NotaryTeamIdBox.Text ?? string.Empty))
            return LocaleService.Get("PACK_MACOS_NOTARY_TEAM_ID_REQUIRED");
        string notaryPassword = NotaryPasswordBox.Text ?? string.Empty;
        if (notaryPassword.Length == 0)
            return LocaleService.Get("PACK_MACOS_NOTARY_PASSWORD_REQUIRED");
        if (AppleSigningInput.HasLineBreak(notaryPassword))
            return LocaleService.Get("PACK_SIGNING_PASSWORD_LINE_BREAK");
        return null;
    }

    private async void onConfirm(object? sender, RoutedEventArgs args)
    {
        if (getValidationMessage() is not null)
        {
            updateValidation();
            return;
        }
        if (EnvironmentSigningOption.IsChecked == true)
        {
            Close(MacOSSigningSelection.UseEnvironment);
            return;
        }

        MacOSNotarizationOptions? notarization = null;
        if (NotarizeOption.IsChecked == true)
        {
            bool apiKey = ApiKeyOption.IsChecked == true;
            notarization = new MacOSNotarizationOptions(
                apiKey ? string.Empty : (NotaryAppleIdBox.Text ?? string.Empty).Trim(),
                apiKey ? string.Empty : (NotaryTeamIdBox.Text ?? string.Empty).Trim().ToUpperInvariant(),
                apiKey ? string.Empty : NotaryPasswordBox.Text ?? string.Empty,
                apiKey ? Path.GetFullPath(NotaryKeyPathBox.Text!) : string.Empty,
                apiKey ? (NotaryKeyIdBox.Text ?? string.Empty).Trim() : string.Empty,
                apiKey ? (NotaryKeyIssuerBox.Text ?? string.Empty).Trim() : string.Empty);
        }
        string certificatePath = CertificatePathBox.Text ?? string.Empty;
        MacOSSigningOptions options = new(
            (SigningIdentityBox.Text ?? string.Empty).Trim(),
            certificatePath.Length == 0 ? string.Empty : Path.GetFullPath(certificatePath),
            certificatePath.Length == 0 ? string.Empty : CertificatePasswordBox.Text ?? string.Empty,
            notarization);
        MacOSStoredSigning stored = new(
            options.SigningIdentity,
            notarization is null
                ? string.Empty
                : notarization.UsesApiKey ? ApiKeyMode : AppleIdMode,
            notarization?.AppleId ?? string.Empty,
            notarization?.TeamId ?? string.Empty,
            notarization?.KeyPath ?? string.Empty,
            notarization?.KeyId ?? string.Empty,
            notarization?.KeyIssuer ?? string.Empty);
        string notaryPassword = notarization?.AppSpecificPassword ?? string.Empty;
        bool updateStoredSigning = loadedSigning is not null
            && (stored != loadedSigning.Record
                || options.CertificatePassword != loadedSigning.CertificatePassword
                || notaryPassword != loadedSigning.NotaryPassword);
        if ((SaveSigningOption.IsChecked == true || updateStoredSigning)
            && certificatePath.Length != 0)
        {
            credentialOperationPending = true;
            CredentialStatusText.Text = string.Empty;
            updateValidation();
            try
            {
                await credentialStore.SaveAsync(
                    certificatePath,
                    stored,
                    options.CertificatePassword,
                    notaryPassword,
                    CancellationToken.None);
            }
            catch (Exception exception) when (isCredentialException(exception))
            {
                loadedSigning = null;
                SaveSigningOption.IsChecked = false;
                CredentialStatusText.Text = string.Format(
                    LocaleService.Get("PACK_SIGNING_SAVE_FAILED"),
                    exception.Message);
                credentialOperationPending = false;
                updateValidation();
                return;
            }
        }
        Close(new MacOSSigningSelection(options));
    }

    private async Task loadSavedSigningAsync(string certificatePath)
    {
        int generation = ++certificateSelectionGeneration;
        loadedSigning = null;
        SigningIdentityBox.Text = string.Empty;
        CertificatePasswordBox.Text = string.Empty;
        NotarizeOption.IsChecked = false;
        AppleIdOption.IsChecked = true;
        NotaryAppleIdBox.Text = string.Empty;
        NotaryTeamIdBox.Text = string.Empty;
        NotaryPasswordBox.Text = string.Empty;
        NotaryKeyPathBox.Text = string.Empty;
        NotaryKeyIdBox.Text = string.Empty;
        NotaryKeyIssuerBox.Text = string.Empty;
        SaveSigningOption.IsChecked = false;
        CredentialStatusText.Text = string.Empty;
        credentialOperationPending = true;
        updateValidation();
        StoredAppleSigning<MacOSStoredSigning>? signing;
        try
        {
            signing = await credentialStore.FindAsync(
                certificatePath,
                CancellationToken.None);
        }
        catch (Exception exception) when (isCredentialException(exception))
        {
            if (generation != certificateSelectionGeneration)
                return;
            CredentialStatusText.Text = string.Format(
                LocaleService.Get("PACK_SIGNING_LOAD_FAILED"),
                exception.Message);
            signing = null;
        }
        if (generation != certificateSelectionGeneration)
            return;
        loadedSigning = signing;
        if (signing is not null)
        {
            SigningIdentityBox.Text = signing.Record.SigningIdentity;
            CertificatePasswordBox.Text = signing.CertificatePassword;
            if (signing.Record.NotaryMode.Length != 0)
            {
                bool apiKey = string.Equals(
                    signing.Record.NotaryMode,
                    ApiKeyMode,
                    StringComparison.Ordinal);
                NotarizeOption.IsChecked = true;
                ApiKeyOption.IsChecked = apiKey;
                AppleIdOption.IsChecked = !apiKey;
                NotaryAppleIdBox.Text = signing.Record.NotaryAppleId;
                NotaryTeamIdBox.Text = signing.Record.NotaryTeamId;
                NotaryKeyPathBox.Text = signing.Record.NotaryKeyPath;
                NotaryKeyIdBox.Text = signing.Record.NotaryKeyId;
                NotaryKeyIssuerBox.Text = signing.Record.NotaryKeyIssuer;
                NotaryPasswordBox.Text = signing.NotaryPassword;
            }
        }
        credentialOperationPending = false;
        updateNotarizationVisibility();
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close((MacOSSigningSelection?)null);
    }

    private static bool isCredentialException(Exception exception) =>
        exception is IOException
        or UnauthorizedAccessException
        or InvalidDataException
        or System.Text.Json.JsonException
        or InvalidOperationException
        or Win32Exception
        or ArgumentException
        or NotSupportedException;
}
