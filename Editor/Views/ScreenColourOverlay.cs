using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Ludork.Services;
using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed class ScreenColourOverlay : Window
{
    private ScreenColourOverlay()
    {
        WindowDecorations = Avalonia.Controls.WindowDecorations.None;
        WindowState = WindowState.FullScreen;
        Topmost = true;
        Background = new SolidColorBrush(Color.FromArgb(70, 0, 0, 0));
        Cursor = new Cursor(StandardCursorType.Cross);
        Content = new TextBlock
        {
            Text = LocaleService.Get("COLOUR_PICKER_PICK_SCREEN_HINT"),
            HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Center,
            VerticalAlignment = Avalonia.Layout.VerticalAlignment.Center,
            Foreground = Brushes.White,
        };
        PointerPressed += onPointerPressed;
        KeyDown += onKeyDown;
    }

    public static Task<Color?> ShowAsync(Window owner)
    {
        return new ScreenColourOverlay().ShowDialog<Color?>(owner);
    }

    private async void onPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (!args.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        IsVisible = false;
        await Task.Delay(75);
        Close(tryGetCursorColor(out Color color) ? color : null);
        args.Handled = true;
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.Escape)
            Close(null);
    }

    private static bool tryGetCursorColor(out Color color)
    {
        color = default;
        if (OperatingSystem.IsMacOS())
            return tryGetMacOSCursorColor(out color);
        if (!OperatingSystem.IsWindows() || !GetCursorPos(out NativePoint point))
            return false;
        IntPtr deviceContext = GetDC(IntPtr.Zero);
        if (deviceContext == IntPtr.Zero)
            return false;
        uint value = GetPixel(deviceContext, point.X, point.Y);
        ReleaseDC(IntPtr.Zero, deviceContext);
        if (value == 0xFFFFFFFF)
            return false;
        color = Color.FromRgb((byte)(value & 0xFF), (byte)((value >> 8) & 0xFF), (byte)((value >> 16) & 0xFF));
        return true;
    }

    private static bool tryGetMacOSCursorColor(out Color color)
    {
        color = default;
        if (!CGPreflightScreenCaptureAccess() && !CGRequestScreenCaptureAccess())
            return false;
        IntPtr mouseEvent = CGEventCreate(IntPtr.Zero);
        if (mouseEvent == IntPtr.Zero)
            return false;
        NativeCGPoint point = CGEventGetLocation(mouseEvent);
        CFRelease(mouseEvent);
        NativeCGRect sampleRect = new()
        {
            Origin = point,
            Size = new NativeCGSize { Width = 1, Height = 1 },
        };
        IntPtr image = CGWindowListCreateImage(sampleRect, 1, 0, 0);
        if (image == IntPtr.Zero)
            return false;
        IntPtr colorSpace = CGColorSpaceCreateDeviceRGB();
        byte[] pixel = new byte[4];
        GCHandle pixelHandle = GCHandle.Alloc(pixel, GCHandleType.Pinned);
        IntPtr context = CGBitmapContextCreate(
            pixelHandle.AddrOfPinnedObject(),
            1,
            1,
            8,
            4,
            colorSpace,
            0x4001
        );
        if (context != IntPtr.Zero)
        {
            CGContextDrawImage(context, new NativeCGRect
            {
                Origin = new NativeCGPoint(),
                Size = new NativeCGSize { Width = 1, Height = 1 },
            }, image);
            CFRelease(context);
        }
        pixelHandle.Free();
        CFRelease(colorSpace);
        CFRelease(image);
        if (context == IntPtr.Zero)
            return false;
        color = Color.FromRgb(pixel[0], pixel[1], pixel[2]);
        return true;
    }

    [DllImport("user32.dll")]
    private static extern bool GetCursorPos(out NativePoint point);

    [DllImport("user32.dll")]
    private static extern IntPtr GetDC(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern int ReleaseDC(IntPtr hWnd, IntPtr hDc);

    [DllImport("gdi32.dll")]
    private static extern uint GetPixel(IntPtr hDc, int x, int y);

    [return: MarshalAs(UnmanagedType.I1)]
    [DllImport("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")]
    private static extern bool CGPreflightScreenCaptureAccess();

    [return: MarshalAs(UnmanagedType.I1)]
    [DllImport("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")]
    private static extern bool CGRequestScreenCaptureAccess();

    [DllImport("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")]
    private static extern IntPtr CGEventCreate(IntPtr source);

    [DllImport("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")]
    private static extern NativeCGPoint CGEventGetLocation(IntPtr eventReference);

    [DllImport("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")]
    private static extern IntPtr CGWindowListCreateImage(
        NativeCGRect screenBounds,
        uint listOption,
        uint windowId,
        uint imageOption
    );

    [DllImport("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")]
    private static extern IntPtr CGColorSpaceCreateDeviceRGB();

    [DllImport("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")]
    private static extern IntPtr CGBitmapContextCreate(
        IntPtr data,
        nuint width,
        nuint height,
        nuint bitsPerComponent,
        nuint bytesPerRow,
        IntPtr colorSpace,
        uint bitmapInfo
    );

    [DllImport("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")]
    private static extern void CGContextDrawImage(IntPtr context, NativeCGRect rect, IntPtr image);

    [DllImport("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")]
    private static extern void CFRelease(IntPtr value);

    [StructLayout(LayoutKind.Sequential)]
    private struct NativePoint
    {
        public int X;
        public int Y;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeCGPoint
    {
        public double X;
        public double Y;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeCGSize
    {
        public double Width;
        public double Height;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeCGRect
    {
        public NativeCGPoint Origin;
        public NativeCGSize Size;
    }
}
