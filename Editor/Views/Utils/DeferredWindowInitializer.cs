using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Services;
using System;

namespace Ludork.Views.Utils;

internal sealed class DeferredWindowInitializer
{
    private readonly Window window;
    private readonly Action initialize;
    private bool closed;
    private bool scheduled;

    public DeferredWindowInitializer(Window window, Action initialize)
    {
        this.window = window;
        this.initialize = initialize;
        window.Opened += onOpened;
        window.Closed += onClosed;
    }

    public bool IsInitialized { get; private set; }

    public static Control CreateLoadingContent()
    {
        return new Border
        {
            Background = new SolidColorBrush(Color.Parse("#1e1e1e")),
            Child = new TextBlock
            {
                Text = LocaleService.Get("LOADING"),
                FontSize = 16,
                Foreground = new SolidColorBrush(Color.Parse("#bdbdbd")),
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment = VerticalAlignment.Center,
            },
        };
    }

    private void onOpened(object? sender, EventArgs args)
    {
        if (scheduled || IsInitialized || closed)
            return;
        scheduled = true;
        Dispatcher.UIThread.Post(run, DispatcherPriority.Background);
    }

    private void run()
    {
        scheduled = false;
        if (closed || IsInitialized)
            return;
        initialize();
        IsInitialized = true;
    }

    private void onClosed(object? sender, EventArgs args)
    {
        closed = true;
        window.Opened -= onOpened;
        window.Closed -= onClosed;
    }
}
