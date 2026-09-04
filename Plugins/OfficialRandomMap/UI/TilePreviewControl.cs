using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Ludork.Plugin.Abstractions;
using System;

namespace Ludork.Plugins.OfficialRandomMap.UI;

internal sealed class TilePreviewControl : Control, IDisposable
{
    private static readonly IBrush BackgroundBrush =
        new SolidColorBrush(Color.Parse("#262626"));
    private static readonly Pen BorderPen =
        new(new SolidColorBrush(Color.Parse("#5f6368")), 1);

    private readonly IMapEditorHost host;
    private Bitmap? bitmap;
    private PluginTilesetSnapshot? tileset;
    private int? tileNumber;

    public TilePreviewControl(IMapEditorHost host)
    {
        this.host = host;
        Width = 64;
        Height = 64;
    }

    public void SetTile(PluginTilesetSnapshot? nextTileset, int? nextTileNumber)
    {
        string? currentPath = tileset?.AssetPath;
        tileset = nextTileset;
        tileNumber = nextTileNumber;
        if (!string.Equals(
                currentPath,
                nextTileset?.AssetPath,
                StringComparison.Ordinal))
        {
            bitmap?.Dispose();
            bitmap = null;
            if (nextTileset is not null)
            {
                string filePath = host.ResolveAssetFile(nextTileset.AssetPath);
                try
                {
                    if (filePath.Length != 0)
                        bitmap = new Bitmap(filePath);
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
