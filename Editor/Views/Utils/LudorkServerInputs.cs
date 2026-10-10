using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Ludork.Services;

namespace Ludork.Views.Utils;

public sealed class LudorkServerInputs : StackPanel
{
    private readonly CheckBox enabled = new();
    private readonly TextBox url = EditorInputs.CreateEditableTextBox();
    private readonly TextBox key = EditorInputs.CreateEditableTextBox();
    private readonly StackPanel fields = new() { Spacing = 6, Margin = new Thickness(28, 0, 0, 0) };
    private readonly TextBlock error = new()
    {
        IsVisible = false,
        Foreground = EditorTheme.Brush("Error"),
        TextWrapping = TextWrapping.Wrap,
    };

    public LudorkServerInputs()
    {
        Spacing = 6;
        enabled.Content = LocaleService.Get("LUDORK_SERVER_ENABLE");
        url.PlaceholderText = LocaleService.Get("LUDORK_SERVER_URL_HINT");
        key.PasswordChar = '●';
        fields.Children.Add(new TextBlock { Text = LocaleService.Get("LUDORK_SERVER_URL") });
        fields.Children.Add(url);
        fields.Children.Add(new TextBlock { Text = LocaleService.Get("LUDORK_SERVER_KEY") });
        fields.Children.Add(key);
        Children.Add(enabled);
        Children.Add(fields);
        Children.Add(error);
        enabled.IsCheckedChanged += (_, _) =>
        {
            fields.IsVisible = enabled.IsChecked == true;
            error.IsVisible = false;
        };
        url.TextChanged += (_, _) => error.IsVisible = false;
        key.TextChanged += (_, _) => error.IsVisible = false;
        fields.IsVisible = false;
    }

    public void SetSettings(LudorkServerSettings settings)
    {
        url.Text = settings.Url;
        key.Text = settings.Key;
        enabled.IsChecked = settings.Enabled;
    }

    public bool TryGetSettings(out LudorkServerSettings settings)
    {
        settings = new(enabled.IsChecked == true, url.Text ?? string.Empty, key.Text ?? string.Empty);
        string? message = settings.GetValidationError();
        error.IsVisible = message is not null;
        error.Text = message is null ? string.Empty : LocaleService.Get(message);
        return message is null;
    }
}
