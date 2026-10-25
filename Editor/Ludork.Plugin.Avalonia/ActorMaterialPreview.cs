using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Ludork.Plugin.Abstractions;
using System;
using System.Runtime.InteropServices;

namespace Ludork.Plugin.Avalonia;

internal sealed class ActorMaterialPreview : IDisposable
{
    private readonly IPluginActorPreview preview;
    private readonly Action changed;

    public ActorMaterialPreview(IPluginActorPreview preview, Action changed)
    {
        this.preview = preview;
        this.changed = changed;
        preview.Changed += onChanged;
        refresh();
    }

    public WriteableBitmap? Bitmap { get; private set; }
    public Vector Offset { get; private set; }
    public bool IsActive { get => preview.IsActive; set => preview.IsActive = value; }

    private void onChanged(object? sender, EventArgs args)
    {
        refresh();
        changed();
    }

    private void refresh()
    {
        if (preview.Frame is not PluginActorPreviewFrame frame)
            return;
        WriteableBitmap next = new(new PixelSize(frame.Width, frame.Height), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Premul);
        using (ILockedFramebuffer buffer = next.Lock())
            for (int row = 0; row < frame.Height; row++)
                Marshal.Copy(frame.Pixels, row * frame.Width * 4, buffer.Address + row * buffer.RowBytes, frame.Width * 4);
        Bitmap?.Dispose();
        Bitmap = next;
        Offset = new Vector(frame.OffsetX, frame.OffsetY);
    }

    public void Dispose()
    {
        preview.Changed -= onChanged;
        preview.Dispose();
        Bitmap?.Dispose();
        Bitmap = null;
    }
}
