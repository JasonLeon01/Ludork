using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Ludork.Services;
using System.Threading;
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
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
    }

    public static async Task<bool> ShowAsync(
        Window owner, string title, string message, CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        ConfirmationDialog dialog = new(title, message);
        using CancellationTokenRegistration registration = cancellationToken.Register(
            () => Dispatcher.UIThread.Post(() => dialog.Close(false)));
        return await dialog.ShowDialog<bool>(owner);
    }

    private void onConfirm(object? sender, RoutedEventArgs args)
    {
        Close(true);
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        Close(false);
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key is Key.Enter or Key.Return or Key.Space)
        {
            Close(true);
            args.Handled = true;
        }
        else if (args.Key == Key.Escape)
        {
            Close(false);
            args.Handled = true;
        }
    }
}
