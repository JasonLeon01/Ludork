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

public partial class IOSSigningInputs : UserControl
{
    private readonly AppleSigningCredentialStore<IOSStoredSigning> credentialStore =
        new("ios-signing.json", "editor/ios-signing");
    private readonly CancellationTokenSource lifetimeCancellation = new();
    private StoredAppleSigning<IOSStoredSigning>? loadedSigning;
    private bool credentialOperationPending;
    private int certificateSelectionGeneration;

    public bool CanConfirm { get; private set; }

    public event EventHandler? StateChanged;

    public IOSSigningOptions? Options { get; private set; }

    public IOSSigningInputs()
    {
        InitializeComponent();
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
        DetachedFromVisualTree += (_, _) => CancelPendingOperations();
        updateSigningVisibility();
    }

    private async void onBrowseCertificate(object? sender, RoutedEventArgs args)
    {
        string? path = await selectFileAsync(CertificatePathBox.Text,
            "PACK_SIGNING_SELECT_CERTIFICATE", "PACK_SIGNING_CERTIFICATE_FILES",
            ["*.p12", "*.pfx", "*.pkcs12"]);
        if (path is null || lifetimeCancellation.IsCancellationRequested)
            return;
        CertificatePathBox.Text = path;
        await loadSavedSigningAsync(path);
    }

    private async void onBrowseProvisioningProfile(object? sender, RoutedEventArgs args)
    {
        string? path = await selectFileAsync(ProvisioningProfileBox.Text,
            "PACK_IOS_SELECT_PROVISIONING_PROFILE", "PACK_IOS_PROVISIONING_PROFILE_FILES",
            ["*.mobileprovision"]);
        if (path is not null && !lifetimeCancellation.IsCancellationRequested)
            ProvisioningProfileBox.Text = path;
    }

    private async Task<string?> selectFileAsync(
        string? currentPath, string titleKey, string fileTypeKey, string[] patterns)
    {
        if (lifetimeCancellation.IsCancellationRequested || credentialOperationPending)
            return null;
        string? directory = Path.GetDirectoryName(currentPath ?? string.Empty);
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
            directory = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        IStorageProvider storageProvider = TopLevel.GetTopLevel(this)!.StorageProvider;
        credentialOperationPending = true;
        updateValidation();
        try
        {
            IStorageFolder? startLocation = await storageProvider.TryGetFolderFromPathAsync(
                new Uri(Path.GetFullPath(directory)));
            if (lifetimeCancellation.IsCancellationRequested)
                return null;
            IReadOnlyList<IStorageFile> files = await storageProvider.OpenFilePickerAsync(
                new FilePickerOpenOptions
                {
                    Title = LocaleService.Get(titleKey),
                    AllowMultiple = false,
                    SuggestedStartLocation = startLocation,
                    FileTypeFilter =
                    [
                        new FilePickerFileType(LocaleService.Get(fileTypeKey)) { Patterns = patterns },
                        new FilePickerFileType(LocaleService.Get("PACK_SIGNING_ALL_FILES")) { Patterns = ["*"] },
                    ],
                });
            return lifetimeCancellation.IsCancellationRequested || files.Count == 0
                ? null
                : Path.GetFullPath(files[0].Path.LocalPath).Normalize(NormalizationForm.FormC);
        }
        finally
        {
            if (!lifetimeCancellation.IsCancellationRequested)
            {
                credentialOperationPending = false;
                updateValidation();
            }
        }
    }

    private void updateSigningVisibility()
    {
        SigningPanel.IsVisible = EnvironmentSigningOption.IsChecked != true;
        updateValidation();
    }

    private void updateValidation()
    {
        if (lifetimeCancellation.IsCancellationRequested)
            return;
        Options = null;
        IsEnabled = !credentialOperationPending;
        bool environmentSigning = EnvironmentSigningOption.IsChecked == true;
        SaveSigningOption.IsVisible = !environmentSigning
            && !credentialOperationPending
            && SigningInput.IsReadableFile(CertificatePathBox.Text ?? string.Empty);
        string? validationMessage = credentialOperationPending ? null : getValidationMessage();
        ValidationText.Text = validationMessage ?? string.Empty;
        bool canConfirm = !credentialOperationPending && validationMessage is null;
        if (CanConfirm != canConfirm)
        {
            CanConfirm = canConfirm;
            StateChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    private string? getValidationMessage()
    {
        if (EnvironmentSigningOption.IsChecked == true)
            return null;
        string teamId = TeamIdBox.Text ?? string.Empty;
        if (teamId.Length != 0 && !AppleSigningInput.IsValidTeamId(teamId.Trim().ToUpperInvariant()))
            return LocaleService.Get("PACK_IOS_SIGNING_TEAM_ID_INVALID");
        if (SigningInput.HasInvalidCharacters(SigningIdentityBox.Text ?? string.Empty))
            return LocaleService.Get("PACK_SIGNING_IDENTITY_LINE_BREAK");

        string certificatePath = CertificatePathBox.Text ?? string.Empty;
        string provisioningProfile = ProvisioningProfileBox.Text ?? string.Empty;
        bool hasCertificate = certificatePath.Length != 0;
        bool hasProfile = provisioningProfile.Length != 0;
        if (hasCertificate != hasProfile)
            return LocaleService.Get("PACK_IOS_SIGNING_PAIR_REQUIRED");
        if (!hasCertificate)
            return null;
        if (!SigningInput.IsReadableFile(certificatePath))
            return LocaleService.Get("PACK_SIGNING_CERTIFICATE_INVALID");
        string certificatePassword = CertificatePasswordBox.Text ?? string.Empty;
        if (certificatePassword.Length == 0)
            return LocaleService.Get("PACK_SIGNING_CERTIFICATE_PASSWORD_REQUIRED");
        if (SigningInput.HasInvalidCharacters(certificatePassword))
            return LocaleService.Get("PACK_SIGNING_PASSWORD_LINE_BREAK");
        if (!SigningInput.IsReadableFile(provisioningProfile))
            return LocaleService.Get("PACK_IOS_PROVISIONING_PROFILE_INVALID");
        return null;
    }

    public async Task<bool> TryPrepareAsync(CancellationToken token)
    {
        if (lifetimeCancellation.IsCancellationRequested || token.IsCancellationRequested
            || credentialOperationPending)
            return false;
        Options = null;
        if (getValidationMessage() is not null)
        {
            updateValidation();
            return false;
        }
        if (EnvironmentSigningOption.IsChecked == true)
        {
            return true;
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
            using CancellationTokenSource operationCancellation =
                CancellationTokenSource.CreateLinkedTokenSource(token, lifetimeCancellation.Token);
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
                    operationCancellation.Token);
            }
            catch (OperationCanceledException) when (operationCancellation.IsCancellationRequested)
            {
                return false;
            }
            catch (Exception exception) when (isCredentialException(exception))
            {
                if (lifetimeCancellation.IsCancellationRequested)
                    return false;
                loadedSigning = null;
                SaveSigningOption.IsChecked = false;
                CredentialStatusText.Text = string.Format(
                    LocaleService.Get("PACK_SIGNING_SAVE_FAILED"),
                    exception.Message);
                return false;
            }
            finally
            {
                if (!lifetimeCancellation.IsCancellationRequested)
                {
                    credentialOperationPending = false;
                    updateValidation();
                }
            }
        }
        if (lifetimeCancellation.IsCancellationRequested || token.IsCancellationRequested)
            return false;
        Options = options;
        return true;
    }

    private async Task loadSavedSigningAsync(string certificatePath)
    {
        if (lifetimeCancellation.IsCancellationRequested)
            return;
        credentialOperationPending = true;
        int generation = ++certificateSelectionGeneration;
        loadedSigning = null;
        TeamIdBox.Text = string.Empty;
        CertificatePasswordBox.Text = string.Empty;
        ProvisioningProfileBox.Text = string.Empty;
        SigningIdentityBox.Text = string.Empty;
        SaveSigningOption.IsChecked = false;
        CredentialStatusText.Text = string.Empty;
        updateValidation();
        StoredAppleSigning<IOSStoredSigning>? signing;
        try
        {
            signing = await credentialStore.FindAsync(
                certificatePath,
                lifetimeCancellation.Token);
        }
        catch (OperationCanceledException) when (lifetimeCancellation.IsCancellationRequested)
        {
            return;
        }
        catch (Exception exception) when (isCredentialException(exception))
        {
            if (lifetimeCancellation.IsCancellationRequested
                || generation != certificateSelectionGeneration)
                return;
            CredentialStatusText.Text = string.Format(
                LocaleService.Get("PACK_SIGNING_LOAD_FAILED"),
                exception.Message);
            signing = null;
        }
        if (lifetimeCancellation.IsCancellationRequested
            || generation != certificateSelectionGeneration)
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

    public void CancelPendingOperations()
    {
        if (lifetimeCancellation.IsCancellationRequested)
            return;
        lifetimeCancellation.Cancel();
        certificateSelectionGeneration++;
        Options = null;
        if (CanConfirm)
        {
            CanConfirm = false;
            StateChanged?.Invoke(this, EventArgs.Empty);
        }
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
