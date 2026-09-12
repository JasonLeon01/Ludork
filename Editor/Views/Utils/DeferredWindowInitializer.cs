using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Threading;
using Ludork.Services;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

internal sealed class DeferredWindowInitializer
{
    private readonly Window window;
    private readonly Func<CancellationToken, Task> initialize;
    private readonly CancellationTokenSource lifetime = new();
    private bool closed;
    private bool scheduled;

    public DeferredWindowInitializer(Window window, Func<CancellationToken, Task> initialize)
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
            Child = new TextBlock
            {
                Text = LocaleService.Get("LOADING"),
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

    private async void run()
    {
        if (closed || IsInitialized)
        {
            if (closed)
                lifetime.Dispose();
            return;
        }
        try
        {
            await initialize(lifetime.Token);
            lifetime.Token.ThrowIfCancellationRequested();
            IsInitialized = true;
        }
        catch (OperationCanceledException) when (closed)
        {
        }
        catch (Exception error)
        {
            if (closed)
                return;
            Button retry = new() { Content = LocaleService.Get("RETRY") };
            retry.Click += (_, _) =>
            {
                window.Content = CreateLoadingContent();
                onOpened(this, EventArgs.Empty);
            };
            window.Content = new StackPanel
            {
                Spacing = 8,
                Margin = new Thickness(16),
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment = VerticalAlignment.Center,
                Children =
                {
                    new TextBlock { Text = LocaleService.Get("ERROR") },
                    new TextBlock { Text = error.Message, TextWrapping = Avalonia.Media.TextWrapping.Wrap },
                    retry,
                },
            };
        }
        finally
        {
            scheduled = false;
            if (closed)
                lifetime.Dispose();
        }
    }

    private void onClosed(object? sender, EventArgs args)
    {
        closed = true;
        lifetime.Cancel();
        if (!scheduled)
            lifetime.Dispose();
        window.Opened -= onOpened;
        window.Closed -= onClosed;
    }
}
