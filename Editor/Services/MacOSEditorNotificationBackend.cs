using Avalonia.Threading;
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ludork.Services;

[SupportedOSPlatform("macos")]
internal sealed class MacOSEditorNotificationBackend : IEditorNotificationBackend
{
    private const string Library = "LudorkNotifications";
    private static readonly NotificationCallback callback = onNativeCompleted;
    private GCHandle context;
    private nint handle;

    public MacOSEditorNotificationBackend()
    {
        Dispatcher.UIThread.VerifyAccess();
        context = GCHandle.Alloc(this);
        try
        {
            handle = create(callback, GCHandle.ToIntPtr(context));
            if (handle == nint.Zero)
                throw new InvalidOperationException("System notifications require launching Ludork from its macOS .app bundle.");
        }
        catch
        {
            context.Free();
            throw;
        }
    }

    public event EventHandler<EditorNotificationCompletion>? Completed;

    public void Show(uint id, string title, string message)
    {
        Dispatcher.UIThread.VerifyAccess();
        ObjectDisposedException.ThrowIf(handle == nint.Zero, this);
        show(handle, id, title, message);
    }

    public void Close(uint id)
    {
        Dispatcher.UIThread.VerifyAccess();
        if (handle != nint.Zero)
            close(handle, id);
    }

    public void Dispose()
    {
        Dispatcher.UIThread.VerifyAccess();
        if (handle == nint.Zero)
            return;
        destroy(handle);
        handle = nint.Zero;
        context.Free();
    }

    private static void onNativeCompleted(nint context, uint id, int status, nint error)
    {
        try
        {
            if (GCHandle.FromIntPtr(context).Target is not MacOSEditorNotificationBackend backend)
                return;
            EditorNotificationCompletion completion = new(id, (EditorNotificationStatus)status, Marshal.PtrToStringUTF8(error));
            Dispatcher.UIThread.Post(() =>
            {
                if (backend.handle != nint.Zero)
                    backend.Completed?.Invoke(backend, completion);
            });
        }
        catch (Exception exception)
        {
            Trace.WriteLine("Could not receive system notification result: " + exception.Message);
        }
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void NotificationCallback(nint context, uint id, int status, nint error);

    [DllImport(Library, EntryPoint = "ludork_notifications_create", CallingConvention = CallingConvention.Cdecl)]
    private static extern nint create(NotificationCallback callback, nint context);

    [DllImport(Library, EntryPoint = "ludork_notifications_show", CallingConvention = CallingConvention.Cdecl)]
    private static extern void show(nint handle, uint id,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string title, [MarshalAs(UnmanagedType.LPUTF8Str)] string message);

    [DllImport(Library, EntryPoint = "ludork_notifications_close", CallingConvention = CallingConvention.Cdecl)]
    private static extern void close(nint handle, uint id);

    [DllImport(Library, EntryPoint = "ludork_notifications_destroy", CallingConvention = CallingConvention.Cdecl)]
    private static extern void destroy(nint handle);
}
