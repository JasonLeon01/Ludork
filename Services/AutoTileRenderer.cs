using Avalonia;
using Avalonia.Media.Imaging;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class AutoTileRenderer : IDisposable
{
    private const int SourceTileSize = 32;
    private const int Top = 0x01;
    private const int Right = 0x02;
    private const int Bottom = 0x04;
    private const int Left = 0x08;
    private const int TopLeft = 0x10;
    private const int TopRight = 0x20;
    private const int BottomRight = 0x40;
    private const int BottomLeft = 0x80;

    private static readonly int[,] BasePattern =
    {
        { 1, 1, 1, 1 }, { 10, 12, 10, 12 }, { 4, 4, 10, 10 }, { 10, 10, 10, 10 },
        { 4, 6, 4, 6 }, { 7, 9, 7, 9 }, { 4, 4, 4, 4 }, { 7, 7, 7, 7 },
        { 6, 6, 12, 12 }, { 12, 12, 12, 12 }, { 5, 5, 11, 11 }, { 11, 11, 11, 11 },
        { 6, 6, 6, 6 }, { 9, 9, 9, 9 }, { 5, 5, 5, 5 }, { 8, 8, 8, 8 },
    };

    private readonly GameDataService gameData;
    private readonly Dictionary<string, Bitmap> sourceImages = new(StringComparer.Ordinal);

    public AutoTileRenderer(GameDataService gameData)
    {
        this.gameData = gameData;
    }

    public void drawTile(Avalonia.Media.DrawingContext context, string key, JsonArray grid, int x, int y, Rect destination, int frame)
    {
        Bitmap? source = getSource(key);
        if (source is null || destination.Width <= 0 || destination.Height <= 0)
            return;

        int mask = computeMask(grid, key, x, y);
        int normalized = normalizeMask(mask);
        int frameCount = Math.Max(1, source.PixelSize.Width / (3 * SourceTileSize));
        int frameOffset = Math.Abs(frame % frameCount) * 3 * SourceTileSize;
        double halfWidth = destination.Width / 2;
        double halfHeight = destination.Height / 2;
        for (int quadrant = 0; quadrant < 4; quadrant++)
        {
            int cell = getPatternCell(normalized, quadrant) - 1;
            int sourceX = frameOffset + cell % 3 * SourceTileSize + quadrant % 2 * SourceTileSize / 2;
            int sourceY = cell / 3 * SourceTileSize + quadrant / 2 * SourceTileSize / 2;
            Rect sourceRect = new Rect(sourceX, sourceY, SourceTileSize / 2, SourceTileSize / 2);
            if (sourceRect.Right > source.PixelSize.Width || sourceRect.Bottom > source.PixelSize.Height)
                continue;
            Rect target = new Rect(
                destination.X + quadrant % 2 * halfWidth,
                destination.Y + quadrant / 2 * halfHeight,
                halfWidth,
                halfHeight);
            context.DrawImage(source, sourceRect, target);
        }
    }

    internal int getFrameCount(string key)
    {
        Bitmap? source = getSource(key);
        return source is null ? 0 : Math.Max(1, source.PixelSize.Width / (3 * SourceTileSize));
    }

    public void Dispose()
    {
        foreach (Bitmap image in sourceImages.Values)
            image.Dispose();
        sourceImages.Clear();
    }

    private Bitmap? getSource(string key)
    {
        if (sourceImages.TryGetValue(key, out Bitmap? cached))
            return cached;
        if (!gameData.AutoTileData.TryGetValue(key, out JsonObject? data))
            return null;
        string? fileName = data["fileName"]?.GetValue<string>();
        if (!GameAssetPath.TryResolveExistingFile(
                gameData.ProjectPath,
                fileName,
                out string path))
        {
            return null;
        }
        Bitmap source = new Bitmap(path);
        sourceImages[key] = source;
        return source;
    }

    private static int getPatternCell(int mask, int quadrant)
    {
        int cell = BasePattern[mask & 0x0f, quadrant];
        int first = quadrant switch { 0 => Top, 1 => Top, 2 => Bottom, _ => Bottom };
        int second = quadrant switch { 0 => Left, 1 => Right, 2 => Left, _ => Right };
        int diagonal = quadrant switch { 0 => TopLeft, 1 => TopRight, 2 => BottomLeft, _ => BottomRight };
        return (mask & first) != 0 && (mask & second) != 0 && (mask & diagonal) == 0 ? 3 : cell;
    }

    private static int normalizeMask(int mask)
    {
        int result = mask & 0x0f;
        if ((mask & (Top | Left | TopLeft)) == (Top | Left | TopLeft)) result |= TopLeft;
        if ((mask & (Top | Right | TopRight)) == (Top | Right | TopRight)) result |= TopRight;
        if ((mask & (Bottom | Right | BottomRight)) == (Bottom | Right | BottomRight)) result |= BottomRight;
        if ((mask & (Bottom | Left | BottomLeft)) == (Bottom | Left | BottomLeft)) result |= BottomLeft;
        return result;
    }

    private static int computeMask(JsonArray grid, string key, int x, int y)
    {
        int mask = 0;
        if (sameKey(grid, key, x, y - 1)) mask |= Top;
        if (sameKey(grid, key, x + 1, y)) mask |= Right;
        if (sameKey(grid, key, x, y + 1)) mask |= Bottom;
        if (sameKey(grid, key, x - 1, y)) mask |= Left;
        if (sameKey(grid, key, x - 1, y - 1)) mask |= TopLeft;
        if (sameKey(grid, key, x + 1, y - 1)) mask |= TopRight;
        if (sameKey(grid, key, x + 1, y + 1)) mask |= BottomRight;
        if (sameKey(grid, key, x - 1, y + 1)) mask |= BottomLeft;
        return mask;
    }

    private static bool sameKey(JsonArray grid, string key, int x, int y)
    {
        return y >= 0 && y < grid.Count && grid[y] is JsonArray row
            && x >= 0 && x < row.Count && string.Equals(row[x]?.GetValue<string>(), key, StringComparison.Ordinal);
    }
}
