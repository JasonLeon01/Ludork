using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using System;
using System.Linq;

namespace Ludork.Services;

internal static class EditorSplitterChangeBinding
{
    public static void Attach(GridSplitter splitter, Func<GridLength[]> read, Action<GridLength[]> save)
    {
        GridLength[]? start = null;
        void complete(GridLength[]? previous)
        {
            if (previous is null || !splitter.IsEffectivelyVisible || !splitter.IsEffectivelyEnabled)
                return;
            GridLength[] current = read();
            if (!current.SequenceEqual(previous))
                save(current);
        }
        splitter.DragStarted += (_, _) => start = read();
        splitter.DragCompleted += (_, _) =>
        {
            complete(start);
            start = null;
        };
        splitter.AddHandler(InputElement.KeyDownEvent, (_, args) =>
        {
            if (args.Key is not (Key.Left or Key.Right or Key.Up or Key.Down))
                return;
            GridLength[] previous = read();
            Dispatcher.UIThread.Post(() => complete(previous), DispatcherPriority.Input);
        }, RoutingStrategies.Tunnel, true);
    }
}
