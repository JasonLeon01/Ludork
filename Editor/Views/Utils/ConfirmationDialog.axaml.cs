using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Ludork.Services;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public partial class ConfirmationDialog : Window
{
    public ConfirmationDialog() : this(string.Empty, string.Empty)
    {
    }

    public ConfirmationDialog(string title, string message, string? details = null)
    {
        InitializeComponent();
        Title = title;
        MessageText.Text = message;
        if (!string.IsNullOrWhiteSpace(details))
        {
            Width = 480;
            Height = 220;
            CanResize = true;
            MinWidth = 440;
            MinHeight = 180;
            MessageText.Text += Environment.NewLine + Environment.NewLine + details;
        }
        ConfirmButton.Content = LocaleService.Get("CONFIRM");
        CancelButton.Content = LocaleService.Get("CANCEL");
        AddHandler(KeyDownEvent, onKeyDown, RoutingStrategies.Tunnel);
    }

    public static async Task<bool> ShowAsync(
        Window owner, string title, string message, CancellationToken cancellationToken = default, string? details = null,
        string? confirmLabel = null)
    {
        cancellationToken.ThrowIfCancellationRequested();
        ConfirmationDialog dialog = new(title, message, details);
        if (confirmLabel is not null)
        {
            dialog.ConfirmButton.Content = confirmLabel;
            dialog.Width = 640;
            dialog.Height = 320;
            dialog.MinWidth = 440;
            dialog.MinHeight = 220;
            dialog.CanResize = true;
        }
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
