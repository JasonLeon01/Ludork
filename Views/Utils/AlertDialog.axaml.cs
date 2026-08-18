using Avalonia.Controls;
using Avalonia.Input;
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
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
    }

    public static Task ShowAsync(Window owner, string title, string message)
    {
        return new AlertDialog(title, message).ShowDialog(owner);
    }

    private void onConfirm(object? sender, RoutedEventArgs args)
    {
        Close();
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key is not (Key.Enter or Key.Return or Key.Space or Key.Escape))
            return;
        Close();
        args.Handled = true;
    }
}
