using Avalonia.Controls;
using Avalonia.Interactivity;
using Ludork.Services;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public partial class AlertDialog : Window
{
    public AlertDialog() : this(string.Empty, string.Empty)
    {
    }

    public AlertDialog(string title, string message)
    {
        InitializeComponent();
        Title = title;
        MessageText.Text = message;
        ConfirmButton.Content = LocaleService.Get("CONFIRM");
    }

    public static Task ShowAsync(Window owner, string title, string message)
    {
        return new AlertDialog(title, message).ShowDialog(owner);
    }

    private void onConfirm(object? sender, RoutedEventArgs args)
    {
        Close();
    }
}
