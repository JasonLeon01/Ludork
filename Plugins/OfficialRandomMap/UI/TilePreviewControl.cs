using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Ludork.Plugin.Abstractions;
using System;
using System.IO;

namespace Ludork.Plugins.OfficialRandomMap.UI;

internal sealed class TilePreviewControl : Control, IDisposable
{
    private static readonly IBrush BackgroundBrush =
        new SolidColorBrush(Color.Parse("#262626"));
    private static readonly Pen BorderPen =
        new(new SolidColorBrush(Color.Parse("#5f6368")), 1);

    private Bitmap? bitmap;
    private PluginTilesetSnapshot? tileset;
    private int? tileNumber;

    public TilePreviewControl()
    {
        Width = 64;
        Height = 64;
    }

    public void SetTile(PluginTilesetSnapshot? nextTileset, int? nextTileNumber)
    {
        string? currentPath = tileset?.ImagePath;
        tileset = nextTileset;
        tileNumber = nextTileNumber;
        if (!string.Equals(
                currentPath,
                nextTileset?.ImagePath,
                StringComparison.OrdinalIgnoreCase))
        {
            bitmap?.Dispose();
            bitmap = null;
            if (nextTileset is not null
                && File.Exists(nextTileset.ImagePath))
            {
                try
                {
                    bitmap = new Bitmap(nextTileset.ImagePath);
                }
                catch (Exception)
                {
                    bitmap = null;
                }
            }
        }
        InvalidateVisual();
    }

    public override void Render(DrawingContext context)
    {
        context.FillRectangle(BackgroundBrush, Bounds);
        if (bitmap is not null
            && tileset is not null
            && tileNumber is int selected
            && selected >= 0)
        {
            int columns = Math.Max(
                1,
                bitmap.PixelSize.Width / Math.Max(1, tileset.TileWidth));
            int sourceX = selected % columns * tileset.TileWidth;
            int sourceY = selected / columns * tileset.TileHeight;
            Rect source = new(
                sourceX,
                sourceY,
                tileset.TileWidth,
                tileset.TileHeight);
            if (source.Right <= bitmap.PixelSize.Width
                && source.Bottom <= bitmap.PixelSize.Height)
            {
                context.DrawImage(bitmap, source, Bounds.Deflate(4));
            }
        }
        context.DrawRectangle(null, BorderPen, Bounds.Deflate(0.5));
    }

    public void Dispose()
    {
        bitmap?.Dispose();
        bitmap = null;
    }
}
