using Avalonia.Controls;
using Avalonia.Platform;
using Avalonia.Threading;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Ludork.Controls;

public sealed class GamePanel : NativeControlHost
{
    private const int WindowProcedureIndex = -4;
    private const uint GetRoot = 2;
    private const uint FocusGainedMessage = 0x0007;
    private const uint FocusLostMessage = 0x0008;
    private const uint CancelModeMessage = 0x001F;
    private const uint KeyDownMessage = 0x0100;
    private const uint KeyUpMessage = 0x0101;
    private const uint SystemKeyDownMessage = 0x0104;
    private const uint SystemKeyUpMessage = 0x0105;
    private const uint MouseMoveMessage = 0x0200;
    private const uint LeftButtonDownMessage = 0x0201;
    private const uint LeftButtonUpMessage = 0x0202;
    private const uint RightButtonDownMessage = 0x0204;
    private const uint RightButtonUpMessage = 0x0205;
    private const uint MiddleButtonDownMessage = 0x0207;
    private const uint MiddleButtonUpMessage = 0x0208;
    private const uint MouseWheelMessage = 0x020A;
    private const uint XButtonDownMessage = 0x020B;
    private const uint XButtonUpMessage = 0x020C;
    private const uint CaptureChangedMessage = 0x0215;
    private readonly List<RuntimeInputEvent> pendingEvents = [];
    private WindowProcedure? windowProcedure;
    private nint previousWindowProcedure;
    private bool flushQueued;
    private bool inputEnabled;
    private int pressedMouseButtons;

    public event EventHandler? NativeHandleReady;
    public event EventHandler<GameInputBatchEventArgs>? InputBatchReady;

    public nint NativeHandle { get; private set; }

    public void NotifyHostFocusLost()
    {
        bool wasEnabled = inputEnabled;
        releaseNativeInputOwnership(false);
        if (wasEnabled)
            enqueue(new("FocusLost"), true);
    }

    public void SetInputEnabled(bool enabled)
    {
        if (!enabled)
        {
            bool wasEnabled = inputEnabled;
            inputEnabled = false;
            if (wasEnabled)
                enqueue(new("FocusLost"), true);
            releaseNativeInputOwnership(true);
            return;
        }
        if (inputEnabled)
            return;
        inputEnabled = true;
        if (NativeHandle == nint.Zero || !canForwardOrdinaryInput(NativeHandle))
            return;
        if (GetFocus() == NativeHandle)
        {
            enqueue(new("FocusGained"));
            return;
        }
        SetFocus(NativeHandle);
    }

    protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
    {
        IPlatformHandle control = base.CreateNativeControlCore(parent);
        NativeHandle = control.Handle;
        if (OperatingSystem.IsWindows() && control.HandleDescriptor == "HWND")
        {
            windowProcedure = processWindowMessage;
            Marshal.SetLastPInvokeError(0);
            previousWindowProcedure = SetWindowLongPtrW(
                NativeHandle,
                WindowProcedureIndex,
                Marshal.GetFunctionPointerForDelegate(windowProcedure));
            int error = Marshal.GetLastPInvokeError();
            if (previousWindowProcedure == nint.Zero && error != 0)
            {
                NativeHandle = nint.Zero;
                windowProcedure = null;
            }
        }
        NativeHandleReady?.Invoke(this, EventArgs.Empty);
        return control;
    }

    protected override void DestroyNativeControlCore(IPlatformHandle control)
    {
        inputEnabled = false;
        releaseNativeInputOwnership(false);
        pendingEvents.Clear();
        flushQueued = false;
        if (OperatingSystem.IsWindows()
            && control.HandleDescriptor == "HWND"
            && previousWindowProcedure != nint.Zero)
        {
            SetWindowLongPtrW(control.Handle, WindowProcedureIndex, previousWindowProcedure);
        }
        previousWindowProcedure = nint.Zero;
        windowProcedure = null;
        NativeHandle = nint.Zero;
        base.DestroyNativeControlCore(control);
    }

    private nint processWindowMessage(nint handle, uint message, nint wParam, nint lParam)
    {
        if (message == CancelModeMessage)
        {
            releaseNativeInputOwnership(false);
        }
        else if (message == CaptureChangedMessage)
        {
            pressedMouseButtons = 0;
        }
        else if (message == FocusLostMessage && inputEnabled)
        {
            pressedMouseButtons = 0;
            enqueue(new("FocusLost"), true);
        }
        else if (message == FocusGainedMessage && canForwardOrdinaryInput(handle))
        {
            enqueue(new("FocusGained"));
        }
        else if ((message == KeyDownMessage || message == SystemKeyDownMessage)
            && canForwardOrdinaryInput(handle))
        {
            enqueueKey("KeyPressed", wParam, lParam);
        }
        else if ((message == KeyUpMessage || message == SystemKeyUpMessage)
            && canForwardOrdinaryInput(handle))
        {
            enqueueKey("KeyReleased", wParam, lParam);
        }
        else if (message == MouseMoveMessage && canForwardOrdinaryInput(handle))
        {
            NativePoint position = getClientPosition(lParam);
            enqueue(new("MouseMoved", X: position.X, Y: position.Y));
        }
        else if (isMouseButtonMessage(message) && canForwardOrdinaryInput(handle))
        {
            return processMouseButton(handle, message, wParam, lParam);
        }
        else if (message == MouseWheelMessage && canForwardOrdinaryInput(handle))
        {
            NativePoint position = getClientPosition(lParam);
            ScreenToClient(handle, ref position);
            double delta = getHighWord(wParam) / 120.0;
            enqueue(new("MouseWheelScrolled", X: position.X, Y: position.Y, Delta: delta));
        }

        return previousWindowProcedure == nint.Zero
            ? DefWindowProcW(handle, message, wParam, lParam)
            : CallWindowProcW(previousWindowProcedure, handle, message, wParam, lParam);
    }

    private void releaseNativeInputOwnership(bool restoreRootFocus)
    {
        pressedMouseButtons = 0;
        nint handle = NativeHandle;
        if (!OperatingSystem.IsWindows() || handle == nint.Zero)
            return;
        if (GetCapture() == handle)
            ReleaseCapture();
        if (!restoreRootFocus || GetFocus() != handle)
            return;
        nint root = GetAncestor(handle, GetRoot);
        if (root != nint.Zero && GetForegroundWindow() == root)
            SetFocus(root);
    }

    private nint processMouseButton(nint handle, uint message, nint wParam, nint lParam)
    {
        bool pressed = message is LeftButtonDownMessage
            or RightButtonDownMessage
            or MiddleButtonDownMessage
            or XButtonDownMessage;
        string button = message switch
        {
            RightButtonDownMessage or RightButtonUpMessage => "Right",
            MiddleButtonDownMessage or MiddleButtonUpMessage => "Middle",
            XButtonDownMessage or XButtonUpMessage => getHighWord(wParam) == 2 ? "Extra2" : "Extra1",
            _ => "Left",
        };
        int buttonMask = button switch
        {
            "Right" => 2,
            "Middle" => 4,
            "Extra1" => 8,
            "Extra2" => 16,
            _ => 1,
        };
        if (pressed)
        {
            SetFocus(handle);
            SetCapture(handle);
            pressedMouseButtons |= buttonMask;
        }
        else
        {
            pressedMouseButtons &= ~buttonMask;
            if (pressedMouseButtons == 0)
                ReleaseCapture();
        }
        NativePoint position = getClientPosition(lParam);
        enqueue(new(
            pressed ? "MouseButtonPressed" : "MouseButtonReleased",
            Button: button,
            X: position.X,
            Y: position.Y));
        if (message is XButtonDownMessage or XButtonUpMessage)
            return new nint(1);
        return previousWindowProcedure == nint.Zero
            ? DefWindowProcW(handle, message, wParam, lParam)
            : CallWindowProcW(previousWindowProcedure, handle, message, wParam, lParam);
    }

    private void enqueueKey(string eventType, nint wParam, nint lParam)
    {
        string? key = getKeyName((int)wParam, lParam);
        if (key is null)
            return;
        enqueue(new(
            eventType,
            Key: key,
            Alt: isKeyHeld(0x12),
            Control: isKeyHeld(0x11),
            Shift: isKeyHeld(0x10),
            System: isKeyHeld(0x5B) || isKeyHeld(0x5C)));
    }

    private void enqueue(RuntimeInputEvent inputEvent, bool force = false)
    {
        if (!inputEnabled && !force)
            return;
        if (inputEvent.Type == "MouseMoved"
            && pendingEvents.Count > 0
            && pendingEvents[^1].Type == "MouseMoved")
        {
            pendingEvents[^1] = inputEvent;
        }
        else
        {
            pendingEvents.Add(inputEvent);
        }
        if (flushQueued)
            return;
        flushQueued = true;
        Dispatcher.UIThread.Post(flushInput, DispatcherPriority.Input);
    }

    private void flushInput()
    {
        flushQueued = false;
        if (pendingEvents.Count == 0)
            return;
        RuntimeInputEvent[] events = pendingEvents.ToArray();
        pendingEvents.Clear();
        InputBatchReady?.Invoke(this, new(events));
    }

    private bool canForwardOrdinaryInput(nint handle)
    {
        return inputEnabled
            && GetForegroundWindow() == GetAncestor(handle, GetRoot);
    }

    private static bool isMouseButtonMessage(uint message)
    {
        return message is LeftButtonDownMessage
            or LeftButtonUpMessage
            or RightButtonDownMessage
            or RightButtonUpMessage
            or MiddleButtonDownMessage
            or MiddleButtonUpMessage
            or XButtonDownMessage
            or XButtonUpMessage;
    }

    private static NativePoint getClientPosition(nint lParam)
    {
        long value = lParam.ToInt64();
        return new((short)(value & 0xFFFF), (short)((value >> 16) & 0xFFFF));
    }

    private static short getHighWord(nint value)
    {
        return (short)((value.ToInt64() >> 16) & 0xFFFF);
    }

    private static bool isKeyHeld(int virtualKey)
    {
        return (GetKeyState(virtualKey) & 0x8000) != 0;
    }

    private static string? getKeyName(int virtualKey, nint lParam)
    {
        if (virtualKey is >= 0x41 and <= 0x5A)
            return ((char)virtualKey).ToString();
        if (virtualKey is >= 0x30 and <= 0x39)
            return "Num" + (virtualKey - 0x30);
        if (virtualKey is >= 0x60 and <= 0x69)
            return "Numpad" + (virtualKey - 0x60);
        if (virtualKey is >= 0x70 and <= 0x7E)
            return "F" + (virtualKey - 0x6F);
        bool extended = (lParam.ToInt64() & 0x01000000) != 0;
        return virtualKey switch
        {
            0x08 => "Backspace",
            0x09 => "Tab",
            0x0D => "Enter",
            0x10 => ((lParam.ToInt64() >> 16) & 0xFF) == 0x36 ? "RShift" : "LShift",
            0x11 => extended ? "RControl" : "LControl",
            0x12 => extended ? "RAlt" : "LAlt",
            0x13 => "Pause",
            0x1B => "Escape",
            0x20 => "Space",
            0x21 => "PageUp",
            0x22 => "PageDown",
            0x23 => "End",
            0x24 => "Home",
            0x25 => "Left",
            0x26 => "Up",
            0x27 => "Right",
            0x28 => "Down",
            0x2D => "Insert",
            0x2E => "Delete",
            0x5B => "LSystem",
            0x5C => "RSystem",
            0x5D => "Menu",
            0x6A => "Multiply",
            0x6B => "Add",
            0x6D => "Subtract",
            0x6E => "Period",
            0x6F => "Divide",
            0xBA => "Semicolon",
            0xBB => "Equal",
            0xBC => "Comma",
            0xBD => "Hyphen",
            0xBE => "Period",
            0xBF => "Slash",
            0xC0 => "Grave",
            0xDB => "LBracket",
            0xDC => "Backslash",
            0xDD => "RBracket",
            0xDE => "Apostrophe",
            _ => null,
        };
    }

    private delegate nint WindowProcedure(nint handle, uint message, nint wParam, nint lParam);

    [StructLayout(LayoutKind.Sequential)]
    private struct NativePoint(int x, int y)
    {
        public int X = x;
        public int Y = y;
    }

    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtrW", SetLastError = true)]
    private static extern nint SetWindowLongPtrW(nint handle, int index, nint value);

    [DllImport("user32.dll")]
    private static extern nint CallWindowProcW(nint previous, nint handle, uint message, nint wParam, nint lParam);

    [DllImport("user32.dll")]
    private static extern nint DefWindowProcW(nint handle, uint message, nint wParam, nint lParam);

    [DllImport("user32.dll")]
    private static extern nint SetFocus(nint handle);

    [DllImport("user32.dll")]
    private static extern nint GetFocus();

    [DllImport("user32.dll")]
    private static extern nint SetCapture(nint handle);

    [DllImport("user32.dll")]
    private static extern nint GetCapture();

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool ReleaseCapture();

    [DllImport("user32.dll")]
    private static extern nint GetForegroundWindow();

    [DllImport("user32.dll")]
    private static extern nint GetAncestor(nint handle, uint flags);

    [DllImport("user32.dll")]
    private static extern short GetKeyState(int virtualKey);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool ScreenToClient(nint handle, ref NativePoint point);
}

public sealed class GameInputBatchEventArgs(IReadOnlyList<RuntimeInputEvent> events) : EventArgs
{
    public IReadOnlyList<RuntimeInputEvent> Events { get; } = events;
}
