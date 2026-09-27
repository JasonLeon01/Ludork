using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using System;
using System.Collections.Generic;

namespace Ludork.Views.Utils;

internal static class EditorContextMenuBehavior
{
    private static readonly Dictionary<int, long> mixedPointers = [];
    private static WeakReference<ContextMenu>? activeMenu;
    private static IPointer? mousePointer;
    private static ContextRequestedEventArgs? mixedRequest;
    private static long nextGesture;
    private static bool initialized;

    public static void Initialize()
    {
        if (initialized)
            return;
        initialized = true;
        MenuBase.IsOpenProperty.Changed.AddClassHandler<ContextMenu>(onMenuOpenChanged);
        InputElement.PointerPressedEvent.AddClassHandler<TopLevel>(onPointerPressed, RoutingStrategies.Tunnel, true);
        InputElement.PointerMovedEvent.AddClassHandler<TopLevel>(onPointerMoved, RoutingStrategies.Tunnel, true);
        InputElement.PointerReleasedEvent.AddClassHandler<TopLevel>(onPointerReleased, RoutingStrategies.Tunnel, true);
        InputElement.ContextRequestedEvent.AddClassHandler<TopLevel>(onContextRequested, RoutingStrategies.Tunnel, true);
    }

    public static bool IsMixedButtonRequest(ContextRequestedEventArgs args)
    {
        return ReferenceEquals(args, mixedRequest);
    }

    private static void onMenuOpenChanged(ContextMenu menu, AvaloniaPropertyChangedEventArgs change)
    {
        if (!change.GetNewValue<bool>())
        {
            if (activeMenu?.TryGetTarget(out ContextMenu? current) == true && ReferenceEquals(current, menu))
                activeMenu = null;
            return;
        }
        if (activeMenu?.TryGetTarget(out ContextMenu? previous) == true && !ReferenceEquals(previous, menu))
            previous.Close();
        mousePointer?.Capture(null);
        activeMenu = new WeakReference<ContextMenu>(menu);
    }

    private static void onPointerPressed(TopLevel root, PointerPressedEventArgs args)
    {
        if (args.Pointer.Type != PointerType.Mouse)
            return;
        mousePointer = args.Pointer;
        mixedPointers.Remove(args.Pointer.Id);
        handleMixedButtons(root, args);
    }

    private static void onPointerMoved(TopLevel root, PointerEventArgs args)
    {
        if (args.Pointer.Type != PointerType.Mouse)
            return;
        mousePointer = args.Pointer;
        if (mixedPointers.ContainsKey(args.Pointer.Id))
        {
            PointerPointProperties properties = args.GetCurrentPoint(root).Properties;
            if (!properties.IsLeftButtonPressed && !properties.IsRightButtonPressed)
            {
                mixedPointers.Remove(args.Pointer.Id);
                return;
            }
            args.Handled = true;
            return;
        }
        handleMixedButtons(root, args);
    }

    private static void handleMixedButtons(TopLevel root, PointerEventArgs args)
    {
        PointerPointProperties properties = args.GetCurrentPoint(root).Properties;
        if (!properties.IsLeftButtonPressed || !properties.IsRightButtonPressed
            || properties.PointerUpdateKind is not (PointerUpdateKind.LeftButtonPressed or PointerUpdateKind.RightButtonPressed))
        {
            return;
        }
        mixedPointers[args.Pointer.Id] = ++nextGesture;
        Interactive? target = root.InputHitTest(args.GetPosition(root)) as Interactive;
        args.Handled = true;
        args.Pointer.Capture(null);
        if (target is null || activeMenu?.TryGetTarget(out ContextMenu? menu) == true && menu.IsOpen)
            return;
        ContextRequestedEventArgs request = new(args);
        mixedRequest = request;
        try
        {
            target.RaiseEvent(request);
        }
        finally
        {
            mixedRequest = null;
        }
    }

    private static void onPointerReleased(TopLevel root, PointerReleasedEventArgs args)
    {
        if (!mixedPointers.TryGetValue(args.Pointer.Id, out long gesture))
            return;
        args.Handled = true;
        Dispatcher.UIThread.Post(() =>
        {
            if (mixedPointers.TryGetValue(args.Pointer.Id, out long current) && current == gesture)
                mixedPointers.Remove(args.Pointer.Id);
        });
    }

    private static void onContextRequested(TopLevel root, ContextRequestedEventArgs args)
    {
        if (!ReferenceEquals(args, mixedRequest) && mixedPointers.Count != 0 && args.TryGetPosition(root, out _))
            args.Handled = true;
    }
}
