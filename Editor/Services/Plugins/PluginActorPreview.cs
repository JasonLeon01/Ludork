using Avalonia;
using Avalonia.Media.Imaging;
using Ludork.Plugin.Abstractions;
using System;
using System.Runtime.InteropServices;

namespace Ludork.Services.Plugins;

internal sealed class PluginActorPreview : IPluginActorPreview
{
    private readonly ActorPreviewLease lease;

    public PluginActorPreview(ActorPreviewLease lease)
    {
        this.lease = lease;
        lease.FrameChanged += onFrameChanged;
        capture();
    }

    public PluginActorPreviewFrame? Frame { get; private set; }
    public event EventHandler? Changed;
    public bool IsActive { get => lease.IsActive; set => lease.IsActive = value; }

    private void onFrameChanged(object? sender, EventArgs args)
    {
        capture();
        Changed?.Invoke(this, EventArgs.Empty);
    }

    private void capture()
    {
        if (lease.Frame is not Bitmap bitmap)
        {
            Frame = null;
            return;
        }
        PixelRect rect = lease.SourceRect;
        byte[] pixels = new byte[checked(rect.Width * rect.Height * 4)];
        GCHandle pinned = GCHandle.Alloc(pixels, GCHandleType.Pinned);
        try { bitmap.CopyPixels(rect, pinned.AddrOfPinnedObject(), pixels.Length, rect.Width * 4); }
        finally { pinned.Free(); }
        Frame = new PluginActorPreviewFrame(rect.Width, rect.Height, pixels, lease.FrameOffset.X, lease.FrameOffset.Y, lease.MaterialError);
    }

    public void Dispose()
    {
        lease.FrameChanged -= onFrameChanged;
        lease.Dispose();
        Frame = null;
    }
}
