using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Plugin.Abstractions;
using Ludork.Plugins.OfficialBlueprintAI.History;
using System;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialBlueprintAI.UI;

public sealed class BlueprintAssistantSettingsWindow : Window
{
    private readonly IBlueprintAssistantProvider provider;
    private readonly BlueprintAssistantProviderContext providerContext;
    private readonly ComboBox providerField = new();
    private readonly TextBox modelField = EditorInputs.CreateEditableTextBox();
    private readonly TextBox endpointField = EditorInputs.CreateEditableTextBox();
    private readonly TextBox organizationField = EditorInputs.CreateEditableTextBox();
    private readonly TextBox projectField = EditorInputs.CreateEditableTextBox();
    private readonly TextBox apiKeyField = EditorInputs.CreateEditableTextBox();
    private readonly CheckBox removeApiKey = new();
    private readonly TextBlock apiKeyStatus = new();
    private readonly TextBlock statusText = new()
    {
        TextWrapping = TextWrapping.Wrap,
        Foreground = new SolidColorBrush(Color.Parse("#bdbdbd")),
    };
    private readonly Button saveButton = new();
    private readonly Button testButton = new();
    private string selectedProvider = "OpenAI";
    private string maskedApiKey = string.Empty;

    public BlueprintAssistantSettingsWindow(
        IBlueprintAssistantProvider provider,
        BlueprintAssistantProviderContext providerContext)
    {
        this.provider = provider;
        this.providerContext = providerContext;
        Title = PluginUiText.Get("BLUEPRINT_AI_SETTINGS");
        Width = 620;
        Height = 610;
        MinWidth = 500;
        MinHeight = 520;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.Parse("#202124"));

        foreach (string providerName in new[] { "OpenAI", "DeepSeek", "Google", "Custom" })
            providerField.Items.Add(providerName);
        providerField.SelectedItem = selectedProvider;
        providerField.HorizontalAlignment = HorizontalAlignment.Stretch;
        providerField.SelectionChanged += (_, _) => onProviderChanged();
        apiKeyField.PasswordChar = '●';
        apiKeyField.PlaceholderText = PluginUiText.Get("BLUEPRINT_AI_API_KEY_UNCHANGED");
        apiKeyField.GotFocus += (_, _) => beginApiKeyEdit();
        apiKeyField.LostFocus += (_, _) => restoreMaskedApiKey();
        removeApiKey.Content = PluginUiText.Get("BLUEPRINT_AI_REMOVE_API_KEY");
        saveButton.Content = PluginUiText.Get("SAVE");
        testButton.Content = PluginUiText.Get("BLUEPRINT_AI_TEST_CONNECTION");
        Button closeButton = new()
        {
            Content = PluginUiText.Get("CLOSE"),
            MinWidth = 88,
        };

        Grid form = new()
        {
            RowDefinitions = new RowDefinitions(
                "Auto,Auto,Auto,Auto,Auto,Auto,Auto,Auto"),
            ColumnDefinitions = new ColumnDefinitions("150,*"),
            RowSpacing = 10,
            ColumnSpacing = 12,
        };
        addRow(form, 0, "BLUEPRINT_AI_PROVIDER", providerField);
        addRow(form, 1, "BLUEPRINT_AI_MODEL", modelField);
        addRow(form, 2, "BLUEPRINT_AI_ENDPOINT", endpointField);
        addRow(form, 3, "BLUEPRINT_AI_ORGANIZATION", organizationField);
        addRow(form, 4, "BLUEPRINT_AI_PROJECT", projectField);
        addRow(form, 5, "BLUEPRINT_AI_API_KEY", apiKeyField);
        Grid.SetRow(apiKeyStatus, 6);
        Grid.SetColumn(apiKeyStatus, 1);
        form.Children.Add(apiKeyStatus);
        Grid.SetRow(removeApiKey, 7);
        Grid.SetColumn(removeApiKey, 1);
        form.Children.Add(removeApiKey);

        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
        };
        buttons.Children.Add(testButton);
        buttons.Children.Add(saveButton);
        buttons.Children.Add(closeButton);

        Grid content = new()
        {
            Margin = new Thickness(20),
            RowDefinitions = new RowDefinitions("*,Auto,Auto"),
            RowSpacing = 12,
        };
        content.Children.Add(new ScrollViewer { Content = form });
        Grid.SetRow(statusText, 1);
        content.Children.Add(statusText);
        Grid.SetRow(buttons, 2);
        content.Children.Add(buttons);
        Content = content;

        Opened += async (_, _) => await loadAsync();
        saveButton.Click += async (_, _) => await saveAsync(false);
        testButton.Click += async (_, _) => await saveAsync(true);
        closeButton.Click += (_, _) => Close();
    }

    private static void addRow(
        Grid form,
        int row,
        string labelKey,
        Control field)
    {
        TextBlock label = new()
        {
            Text = PluginUiText.Get(labelKey),
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid.SetRow(label, row);
        form.Children.Add(label);
        Grid.SetRow(field, row);
        Grid.SetColumn(field, 1);
        form.Children.Add(field);
    }

    private async Task loadAsync()
    {
        setBusy(true);
        try
        {
            BlueprintAssistantSettings settings = await provider.LoadSettingsAsync(
                providerContext,
                CancellationToken.None);
            applySettings(settings);
            statusText.Text = string.Empty;
        }
        catch (Exception exception)
        {
            statusText.Text = BlueprintAssistantText.SanitizeError(exception.Message);
        }
        finally
        {
            setBusy(false);
        }
    }

    private async Task saveAsync(bool testConnection)
    {
        setBusy(true);
        try
        {
            bool shouldRemoveApiKey = removeApiKey.IsChecked == true;
            string apiKeyText = apiKeyField.Text ?? string.Empty;
            string? newApiKey =
                shouldRemoveApiKey ||
                string.IsNullOrEmpty(apiKeyText) ||
                string.Equals(apiKeyText, maskedApiKey, StringComparison.Ordinal)
                    ? null
                    : apiKeyText;
            BlueprintAssistantSettingsUpdate update = new(
                providerField.SelectedItem as string ?? selectedProvider,
                modelField.Text?.Trim() ?? string.Empty,
                endpointField.Text?.Trim() ?? string.Empty,
                organizationField.Text?.Trim() ?? string.Empty,
                projectField.Text?.Trim() ?? string.Empty,
                newApiKey,
                shouldRemoveApiKey);
            PluginResult saveResult = await provider.SaveSettingsAsync(
                providerContext,
                update,
                CancellationToken.None);
            if (!saveResult.Success)
            {
                statusText.Text = BlueprintAssistantText.SanitizeError(saveResult.Error);
                return;
            }
            BlueprintAssistantSettings settings = await provider.LoadSettingsAsync(
                providerContext,
                CancellationToken.None);
            applySettings(settings);
            if (!testConnection)
            {
                statusText.Text = PluginUiText.Get("BLUEPRINT_AI_SETTINGS_SAVED");
                return;
            }
            PluginResult testResult = await provider.TestConnectionAsync(
                providerContext,
                CancellationToken.None);
            statusText.Text = testResult.Success
                ? PluginUiText.Get("BLUEPRINT_AI_CONNECTION_SUCCEEDED")
                : BlueprintAssistantText.SanitizeError(testResult.Error);
        }
        catch (Exception exception)
        {
            statusText.Text = BlueprintAssistantText.SanitizeError(exception.Message);
        }
        finally
        {
            setBusy(false);
        }
    }

    private void applySettings(BlueprintAssistantSettings settings)
    {
        selectedProvider = normalizeProvider(settings.Provider);
        providerField.SelectedItem = selectedProvider;
        modelField.Text = settings.Model;
        endpointField.Text = settings.Endpoint;
        organizationField.Text = settings.Organization;
        projectField.Text = settings.Project;
        applyApiKeySettings(settings);
    }

    private void applyApiKeySettings(BlueprintAssistantSettings settings)
    {
        maskedApiKey = settings.MaskedApiKey;
        apiKeyField.PasswordChar = maskedApiKey.Length == 0 ? '●' : '\0';
        apiKeyField.Text = maskedApiKey;
        removeApiKey.IsChecked = false;
        apiKeyStatus.Text = settings.HasApiKey
            ? PluginUiText.Get("BLUEPRINT_AI_API_KEY_STORED")
            : PluginUiText.Get("BLUEPRINT_AI_API_KEY_MISSING");
    }

    private void beginApiKeyEdit()
    {
        if (maskedApiKey.Length == 0 ||
            !string.Equals(apiKeyField.Text, maskedApiKey, StringComparison.Ordinal))
        {
            return;
        }

        apiKeyField.Text = string.Empty;
        apiKeyField.PasswordChar = '●';
    }

    private void restoreMaskedApiKey()
    {
        if (maskedApiKey.Length == 0 ||
            !string.IsNullOrEmpty(apiKeyField.Text))
        {
            return;
        }

        apiKeyField.PasswordChar = '\0';
        apiKeyField.Text = maskedApiKey;
    }

    private void setBusy(bool busy)
    {
        saveButton.IsEnabled = !busy;
        testButton.IsEnabled = !busy;
    }

    private void onProviderChanged()
    {
        string nextProvider = providerField.SelectedItem as string ?? selectedProvider;
        string endpoint = endpointField.Text?.Trim() ?? string.Empty;
        string oldPreset = getProviderEndpoint(selectedProvider);
        if (endpoint.Length == 0
            || string.Equals(endpoint, oldPreset, StringComparison.OrdinalIgnoreCase))
        {
            endpointField.Text = getProviderEndpoint(nextProvider);
        }
        selectedProvider = nextProvider;
    }

    private static string normalizeProvider(string providerName)
    {
        return new[] { "OpenAI", "DeepSeek", "Google", "Custom" }
            .FirstOrDefault(value => string.Equals(
                value,
                providerName,
                StringComparison.OrdinalIgnoreCase))
            ?? "Custom";
    }

    private static string getProviderEndpoint(string providerName)
    {
        return providerName switch
        {
            "OpenAI" => "https://api.openai.com/v1",
            "DeepSeek" => "https://api.deepseek.com",
            "Google" => "https://generativelanguage.googleapis.com/v1beta/openai",
            _ => string.Empty,
        };
    }
}
