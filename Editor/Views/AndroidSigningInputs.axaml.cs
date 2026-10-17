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

public partial class AndroidSigningInputs : UserControl
{
    private readonly AndroidSigningCredentialStore credentialStore = new();
    private readonly CancellationTokenSource lifetimeCancellation = new();
    private AndroidSigningOptions? loadedSigning;
    private bool credentialOperationPending;
    private int keystoreSelectionGeneration;
    private bool closed;

    public AndroidSigningInputs()
    {
        InitializeComponent();
        DescriptionText.Text = LocaleService.Get("PACK_ANDROID_SIGNING_DESCRIPTION");
        KeystoreLabel.Text = LocaleService.Get("PACK_ANDROID_KEYSTORE");
        KeyAliasLabel.Text = LocaleService.Get("PACK_ANDROID_KEY_ALIAS");
        KeystorePasswordLabel.Text = LocaleService.Get("PACK_ANDROID_KEYSTORE_PASSWORD");
        SameKeyPasswordOption.Content = LocaleService.Get("PACK_ANDROID_KEY_PASSWORD_SAME");
        KeyPasswordLabel.Text = LocaleService.Get("PACK_ANDROID_KEY_PASSWORD");
        SaveSigningOption.Content = LocaleService.Get("PACK_ANDROID_SAVE_SIGNING");
        BrowseButton.Content = LocaleService.Get("BROWSE");
        EditorInputs.ApplyReadOnly(KeystorePathBox);
        EditorInputs.ApplyEditable(KeyAliasBox);
        EditorInputs.ApplyEditable(KeystorePasswordBox);
        EditorInputs.ApplyEditable(KeyPasswordBox);
        KeystorePathBox.TextChanged += (_, _) => updateValidation();
        KeyAliasBox.TextChanged += (_, _) => updateValidation();
        KeystorePasswordBox.TextChanged += (_, _) => updateValidation();
        KeyPasswordBox.TextChanged += (_, _) => updateValidation();
        SameKeyPasswordOption.IsCheckedChanged += (_, _) => updateKeyPasswordVisibility();
        DetachedFromVisualTree += (_, _) => CancelPendingOperations();
        updateKeyPasswordVisibility();
    }

    public bool CanConfirm { get; private set; }

    public event EventHandler? StateChanged;

    public AndroidSigningOptions? Options { get; private set; }

    public void CancelPendingOperations()
    {
        if (closed)
            return;
        closed = true;
        ++keystoreSelectionGeneration;
        lifetimeCancellation.Cancel();
        Options = null;
        CanConfirm = false;
        StateChanged?.Invoke(this, EventArgs.Empty);
    }

    private async void onBrowse(object? sender, RoutedEventArgs args)
    {
        string? path = await selectFileAsync(KeystorePathBox.Text,
            "PACK_ANDROID_SELECT_KEYSTORE", "PACK_ANDROID_KEYSTORE_FILES",
            ["*.jks", "*.keystore", "*.p12", "*.pfx", "*.pkcs12"]);
        if (path is null || closed)
            return;
        KeystorePathBox.Text = path;
        await loadSavedSigningAsync(path);
    }

    private async Task<string?> selectFileAsync(
        string? currentPath, string titleKey, string fileTypeKey, string[] patterns)
    {
        if (closed || credentialOperationPending)
            return null;
        string? directory = Path.GetDirectoryName(currentPath);
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
            directory = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        IStorageProvider storageProvider = TopLevel.GetTopLevel(this)!.StorageProvider;
        credentialOperationPending = true;
        updateValidation();
        try
        {
            IStorageFolder? startLocation = await storageProvider.TryGetFolderFromPathAsync(
                new Uri(Path.GetFullPath(directory)));
            if (closed)
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
                        new FilePickerFileType(LocaleService.Get("PACK_ANDROID_ALL_FILES")) { Patterns = ["*"] },
                    ],
                });
            return closed || files.Count == 0
                ? null
                : Path.GetFullPath(files[0].Path.LocalPath).Normalize(NormalizationForm.FormC);
        }
        finally
        {
            if (!closed)
            {
                credentialOperationPending = false;
                updateValidation();
            }
        }
    }

    private void updateKeyPasswordVisibility()
    {
        if (closed)
            return;
        KeyPasswordPanel.IsVisible = SameKeyPasswordOption.IsChecked != true;
        updateValidation();
    }

    private void updateValidation()
    {
        if (closed)
            return;
        Options = null;
        SigningPanel.IsEnabled = !credentialOperationPending;
        string? validationMessage = credentialOperationPending ? null : getValidationMessage();
        ValidationText.Text = validationMessage ?? string.Empty;
        CanConfirm = !credentialOperationPending && validationMessage is null;
        StateChanged?.Invoke(this, EventArgs.Empty);
    }

    private string? getValidationMessage()
    {
        string keystorePath = KeystorePathBox.Text ?? string.Empty;
        if (!SigningInput.IsReadableFile(keystorePath))
            return LocaleService.Get("PACK_ANDROID_KEYSTORE_INVALID");
        if (string.IsNullOrWhiteSpace(KeyAliasBox.Text))
            return LocaleService.Get("PACK_ANDROID_KEY_ALIAS_REQUIRED");
        if (SigningInput.HasInvalidCharacters(KeyAliasBox.Text))
            return LocaleService.Get("PACK_ANDROID_KEY_ALIAS_INVALID");

        string keystorePassword = KeystorePasswordBox.Text ?? string.Empty;
        if (keystorePassword.Length == 0)
            return LocaleService.Get("PACK_ANDROID_KEYSTORE_PASSWORD_REQUIRED");
        if (SigningInput.HasInvalidCharacters(keystorePassword))
            return LocaleService.Get("PACK_ANDROID_PASSWORD_LINE_BREAK");

        if (SameKeyPasswordOption.IsChecked != true)
        {
            string keyPassword = KeyPasswordBox.Text ?? string.Empty;
            if (keyPassword.Length == 0)
                return LocaleService.Get("PACK_ANDROID_KEY_PASSWORD_REQUIRED");
            if (SigningInput.HasInvalidCharacters(keyPassword))
                return LocaleService.Get("PACK_ANDROID_PASSWORD_LINE_BREAK");
        }
        return null;
    }

    public async Task<bool> TryPrepareAsync(CancellationToken token)
    {
        if (closed || token.IsCancellationRequested)
            return false;
        Options = null;
        if (credentialOperationPending || getValidationMessage() is not null)
        {
            updateValidation();
            return false;
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
        if (SaveSigningOption.IsChecked == true || (loadedSigning is not null && signing != loadedSigning))
        {
            using CancellationTokenSource cancellation = CancellationTokenSource.CreateLinkedTokenSource(
                token, lifetimeCancellation.Token);
            credentialOperationPending = true;
            CredentialStatusText.Text = string.Empty;
            updateValidation();
            try
            {
                await credentialStore.SaveAsync(signing, cancellation.Token);
                if (cancellation.IsCancellationRequested)
                    return false;
                loadedSigning = signing;
            }
            catch (OperationCanceledException) when (cancellation.IsCancellationRequested)
            {
                return false;
            }
            catch (Exception exception) when (isCredentialException(exception))
            {
                if (cancellation.IsCancellationRequested)
                    return false;
                loadedSigning = null;
                SaveSigningOption.IsChecked = false;
                SaveSigningOption.IsVisible = true;
                CredentialStatusText.Text = string.Format(
                    LocaleService.Get("PACK_ANDROID_SIGNING_SAVE_FAILED"), exception.Message);
                return false;
            }
            finally
            {
                if (!closed)
                {
                    credentialOperationPending = false;
                    updateValidation();
                }
            }
        }
        if (closed || token.IsCancellationRequested)
            return false;
        Options = signing;
        return true;
    }

    private async Task loadSavedSigningAsync(string keystorePath)
    {
        if (closed)
            return;
        int generation = ++keystoreSelectionGeneration;
        CancellationToken cancellationToken = lifetimeCancellation.Token;
        credentialOperationPending = true;
        loadedSigning = null;
        KeyAliasBox.Text = string.Empty;
        KeystorePasswordBox.Text = string.Empty;
        KeyPasswordBox.Text = string.Empty;
        SameKeyPasswordOption.IsChecked = true;
        SaveSigningOption.IsChecked = false;
        SaveSigningOption.IsVisible = false;
        CredentialStatusText.Text = string.Empty;
        updateValidation();
        AndroidSigningOptions? signing;
        try
        {
            signing = await credentialStore.FindAsync(keystorePath, cancellationToken);
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            return;
        }
        catch (Exception exception) when (isCredentialException(exception))
        {
            if (closed || generation != keystoreSelectionGeneration)
                return;
            CredentialStatusText.Text = string.Format(
                LocaleService.Get("PACK_ANDROID_SIGNING_LOAD_FAILED"), exception.Message);
            signing = null;
        }
        if (closed || generation != keystoreSelectionGeneration)
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
