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

public sealed record IOSSigningSelection(IOSSigningOptions? Options)
{
    public static IOSSigningSelection UseEnvironment { get; } =
        new((IOSSigningOptions?)null);
}

public partial class IOSSigningDialog : Window
{
    private readonly AppleSigningCredentialStore<IOSStoredSigning> credentialStore =
        new("ios-signing.json", "editor/ios-signing");
    private StoredAppleSigning<IOSStoredSigning>? loadedSigning;
    private bool credentialOperationPending;
    private int certificateSelectionGeneration;

    public IOSSigningDialog()
    {
        InitializeComponent();
        Title = LocaleService.Get("PACK_IOS_SIGNING_TITLE");
        DescriptionText.Text = LocaleService.Get("PACK_IOS_SIGNING_DESCRIPTION");
        EnvironmentSigningOption.Content = LocaleService.Get("PACK_SIGNING_USE_ENVIRONMENT");
        TeamIdLabel.Text = LocaleService.Get("PACK_IOS_SIGNING_TEAM_ID");
        CertificateLabel.Text = LocaleService.Get("PACK_SIGNING_CERTIFICATE");
        CertificatePasswordLabel.Text = LocaleService.Get("PACK_SIGNING_CERTIFICATE_PASSWORD");
        ProvisioningProfileLabel.Text = LocaleService.Get("PACK_IOS_PROVISIONING_PROFILE");
        SigningIdentityLabel.Text = LocaleService.Get("PACK_SIGNING_IDENTITY");
        SaveSigningOption.Content = LocaleService.Get("PACK_SIGNING_SAVE");
        BrowseCertificateButton.Content = LocaleService.Get("BROWSE");
        BrowseProvisioningProfileButton.Content = LocaleService.Get("BROWSE");
        ConfirmButton.Content = LocaleService.Get("CONFIRM");
        CancelButton.Content = LocaleService.Get("CANCEL");
        EditorInputs.ApplyReadOnly(CertificatePathBox);
        EditorInputs.ApplyReadOnly(ProvisioningProfileBox);
        EditorInputs.ApplyEditable(TeamIdBox);
        EditorInputs.ApplyEditable(CertificatePasswordBox);
        EditorInputs.ApplyEditable(SigningIdentityBox);
        EnvironmentSigningOption.IsCheckedChanged += (_, _) => updateSigningVisibility();
        TeamIdBox.TextChanged += (_, _) => updateValidation();
        CertificatePathBox.TextChanged += (_, _) => updateValidation();
        CertificatePasswordBox.TextChanged += (_, _) => updateValidation();
        ProvisioningProfileBox.TextChanged += (_, _) => updateValidation();
        SigningIdentityBox.TextChanged += (_, _) => updateValidation();
        Opened += (_, _) => ConfirmButton.Focus();
        updateSigningVisibility();
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

    private async void onBrowseProvisioningProfile(object? sender, RoutedEventArgs args)
    {
        IStorageFolder? startLocation = await getStartLocationAsync(ProvisioningProfileBox.Text);
        IReadOnlyList<IStorageFile> files = await StorageProvider.OpenFilePickerAsync(
            new FilePickerOpenOptions
            {
                Title = LocaleService.Get("PACK_IOS_SELECT_PROVISIONING_PROFILE"),
                AllowMultiple = false,
                SuggestedStartLocation = startLocation,
                FileTypeFilter =
                [
                    new FilePickerFileType(LocaleService.Get("PACK_IOS_PROVISIONING_PROFILE_FILES"))
                    {
                        Patterns = ["*.mobileprovision"],
                    },
                    new FilePickerFileType(LocaleService.Get("PACK_SIGNING_ALL_FILES"))
                    {
                        Patterns = ["*"],
                    },
                ],
            });
        if (files.Count == 0)
            return;
        ProvisioningProfileBox.Text = Path.GetFullPath(files[0].Path.LocalPath)
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
        string teamId = TeamIdBox.Text ?? string.Empty;
        if (teamId.Length != 0 && !AppleSigningInput.IsValidTeamId(teamId.Trim().ToUpperInvariant()))
            return LocaleService.Get("PACK_IOS_SIGNING_TEAM_ID_INVALID");
        if (AppleSigningInput.HasLineBreak(SigningIdentityBox.Text ?? string.Empty))
            return LocaleService.Get("PACK_SIGNING_IDENTITY_LINE_BREAK");

        string certificatePath = CertificatePathBox.Text ?? string.Empty;
        string provisioningProfile = ProvisioningProfileBox.Text ?? string.Empty;
        bool hasCertificate = certificatePath.Length != 0;
        bool hasProfile = provisioningProfile.Length != 0;
        if (hasCertificate != hasProfile)
            return LocaleService.Get("PACK_IOS_SIGNING_PAIR_REQUIRED");
        if (!hasCertificate)
            return null;
        if (!AppleSigningInput.IsReadableFile(certificatePath))
            return LocaleService.Get("PACK_SIGNING_CERTIFICATE_INVALID");
        string certificatePassword = CertificatePasswordBox.Text ?? string.Empty;
        if (certificatePassword.Length == 0)
            return LocaleService.Get("PACK_SIGNING_CERTIFICATE_PASSWORD_REQUIRED");
        if (AppleSigningInput.HasLineBreak(certificatePassword))
            return LocaleService.Get("PACK_SIGNING_PASSWORD_LINE_BREAK");
        if (!AppleSigningInput.IsReadableFile(provisioningProfile))
            return LocaleService.Get("PACK_IOS_PROVISIONING_PROFILE_INVALID");
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
            Close(IOSSigningSelection.UseEnvironment);
            return;
        }

        string certificatePath = CertificatePathBox.Text ?? string.Empty;
        IOSSigningOptions options = new(
            (TeamIdBox.Text ?? string.Empty).Trim().ToUpperInvariant(),
            certificatePath.Length == 0 ? string.Empty : Path.GetFullPath(certificatePath),
            certificatePath.Length == 0 ? string.Empty : CertificatePasswordBox.Text ?? string.Empty,
            certificatePath.Length == 0
                ? string.Empty
                : Path.GetFullPath(ProvisioningProfileBox.Text!),
            (SigningIdentityBox.Text ?? string.Empty).Trim());
        IOSStoredSigning stored = new(
            options.TeamId,
            options.ProvisioningProfilePath,
            options.SigningIdentity);
        bool updateStoredSigning = loadedSigning is not null
            && (stored != loadedSigning.Record
                || options.CertificatePassword != loadedSigning.CertificatePassword);
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
                    string.Empty,
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
        Close(new IOSSigningSelection(options));
    }

    private async Task loadSavedSigningAsync(string certificatePath)
    {
        int generation = ++certificateSelectionGeneration;
        loadedSigning = null;
        TeamIdBox.Text = string.Empty;
        CertificatePasswordBox.Text = string.Empty;
        ProvisioningProfileBox.Text = string.Empty;
        SigningIdentityBox.Text = string.Empty;
        SaveSigningOption.IsChecked = false;
        CredentialStatusText.Text = string.Empty;
        credentialOperationPending = true;
        updateValidation();
        StoredAppleSigning<IOSStoredSigning>? signing;
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
            TeamIdBox.Text = signing.Record.TeamId;
            CertificatePasswordBox.Text = signing.CertificatePassword;
            ProvisioningProfileBox.Text = signing.Record.ProvisioningProfilePath;
            SigningIdentityBox.Text = signing.Record.SigningIdentity;
        }
        credentialOperationPending = false;
        updateValidation();
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close((IOSSigningSelection?)null);
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
