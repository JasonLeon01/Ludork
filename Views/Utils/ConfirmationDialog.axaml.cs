using Avalonia.Controls;
using Avalonia.Interactivity;
using Ludork.Services;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public partial class ConfirmationDialog : Window
{
    public ConfirmationDialog() : this(string.Empty, string.Empty)
    {
    }

    public ConfirmationDialog(string title, string message)
    {
        InitializeComponent();
        Title = title;
        MessageText.Text = message;
        ConfirmButton.Content = LocaleService.Get("CONFIRM");
        CancelButton.Content = LocaleService.Get("CANCEL");
    }

    public static Task<bool> ShowAsync(Window owner, string title, string message)
    {
        return new ConfirmationDialog(title, message).ShowDialog<bool>(owner);
    }

    private void onConfirm(object? sender, RoutedEventArgs args)
    {
        Close(true);
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close(false);
    }
}
