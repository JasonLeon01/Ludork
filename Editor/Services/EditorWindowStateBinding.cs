using Avalonia;
using Avalonia.Controls;
using Avalonia.Platform;
using Avalonia.Threading;
using System;

namespace Ludork.Services;

internal sealed class EditorWindowStateBinding
{
    private readonly Window window;
    private readonly string key;
    private readonly EditorSettings settings;
    private EditorWindowState normalState;
    private bool ready;
    private bool closed;
    private bool stateChanging;

    public EditorWindowStateBinding(Window window, string key, EditorSettings settings)
    {
        this.window = window;
        this.key = key;
        this.settings = settings;
        EditorWindowState? saved = settings.GetWindowState(key);
        normalState = saved ?? new EditorWindowState(
            double.IsFinite(window.Width) ? window.Width : Math.Max(640, window.MinWidth),
            double.IsFinite(window.Height) ? window.Height : Math.Max(480, window.MinHeight), null, null, false);
        restoreBounds(saved is not null, false);
        window.Opened += onOpened;
        window.Resized += onResized;
        window.PositionChanged += onPositionChanged;
        window.PropertyChanged += onPropertyChanged;
        window.Closed += onClosed;
    }

    private void restoreBounds(bool restorePosition, bool updateState = true)
    {
        Screen? screen = normalState.X is int x && normalState.Y is int y
            ? window.Screens.ScreenFromPoint(new PixelPoint(x, y)) : null;
        screen ??= window.Owner is Window owner ? window.Screens.ScreenFromWindow(owner) : null;
        screen ??= window.Screens.Primary;
        double scaling = screen?.Scaling ?? 1;
        double availableWidth = screen?.WorkingArea.Width / scaling ?? double.PositiveInfinity;
        double availableHeight = screen is not null ? Math.Max(1, screen.WorkingArea.Height / scaling - 40) : double.PositiveInfinity;
        double width = Math.Clamp(normalState.Width, window.MinWidth, Math.Max(window.MinWidth, Math.Min(window.MaxWidth, availableWidth)));
        double height = Math.Clamp(normalState.Height, window.MinHeight, Math.Max(window.MinHeight, Math.Min(window.MaxHeight, availableHeight)));
        window.Width = width;
        window.Height = height;
        int? positionX = normalState.X;
        int? positionY = normalState.Y;
        if (restorePosition && positionX is int px && positionY is int py && screen is not null)
        {
            PixelRect area = screen.WorkingArea;
            int pixelWidth = (int)Math.Ceiling(width * scaling);
            int pixelHeight = (int)Math.Ceiling((height + 40) * scaling);
            positionX = Math.Clamp(px, area.X, Math.Max(area.X, area.Right - pixelWidth));
            positionY = Math.Clamp(py, area.Y, Math.Max(area.Y, area.Bottom - pixelHeight));
            window.WindowStartupLocation = WindowStartupLocation.Manual;
            window.Position = new PixelPoint(positionX.Value, positionY.Value);
        }
        if (updateState)
            normalState = normalState with { Width = width, Height = height, X = positionX, Y = positionY };
    }

    private void onOpened(object? sender, EventArgs args)
    {
        normalState = normalState with
        {
            X = normalState.X ?? window.Position.X,
            Y = normalState.Y ?? window.Position.Y,
        };
        restoreBounds(true);
        normalState = normalState with { X = window.Position.X, Y = window.Position.Y };
        if (normalState.Maximized)
            window.WindowState = WindowState.Maximized;
        Dispatcher.UIThread.Post(() => ready = !closed, DispatcherPriority.ContextIdle);
    }

    private bool canCapture => ready && !closed && !stateChanging && window.IsVisible && window.IsActive;

    private void onResized(object? sender, WindowResizedEventArgs args)
    {
        if (!canCapture || window.WindowState != WindowState.Normal
            || args.Reason is not (WindowResizeReason.User or WindowResizeReason.Unspecified))
            return;
        if (!double.IsFinite(args.ClientSize.Width) || !double.IsFinite(args.ClientSize.Height)
            || args.ClientSize.Width <= 0 || args.ClientSize.Height <= 0)
            return;
        normalState = normalState with { Width = args.ClientSize.Width, Height = args.ClientSize.Height };
        record();
    }

    private void onPositionChanged(object? sender, PixelPointEventArgs args)
    {
        if (!canCapture || window.WindowState != WindowState.Normal)
            return;
        normalState = normalState with { X = window.Position.X, Y = window.Position.Y };
        record();
    }

    private void onPropertyChanged(object? sender, AvaloniaPropertyChangedEventArgs args)
    {
        if (args.Property != Window.WindowStateProperty || !ready || closed)
            return;
        stateChanging = true;
        if (window.WindowState is WindowState.Normal or WindowState.Maximized)
        {
            normalState = normalState with { Maximized = window.WindowState == WindowState.Maximized };
            record();
        }
        Dispatcher.UIThread.Post(() => stateChanging = false, DispatcherPriority.ContextIdle);
    }

    private void record()
    {
        settings.SetWindowState(key, normalState);
        EditorLayoutService.RequestSave();
    }

    private void onClosed(object? sender, EventArgs args)
    {
        closed = true;
        ready = false;
        window.Opened -= onOpened;
        window.Resized -= onResized;
        window.PositionChanged -= onPositionChanged;
        window.PropertyChanged -= onPropertyChanged;
        window.Closed -= onClosed;
        EditorLayoutService.Save();
    }
}
