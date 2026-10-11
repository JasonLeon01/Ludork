using System;

namespace Ludork.Plugins.OfficialRandomMap.UI;

internal readonly record struct MapRegion(int X, int Y, int Width, int Height)
{
    public int Right => X + Width;
    public int Bottom => Y + Height;

    public bool Contains(int x, int y)
    {
        return x >= X && y >= Y && x < Right && y < Bottom;
    }

    public static MapRegion FromPoints(int startX, int startY, int endX, int endY)
    {
        return new MapRegion(
            Math.Min(startX, endX),
            Math.Min(startY, endY),
            Math.Abs(endX - startX) + 1,
            Math.Abs(endY - startY) + 1);
    }
}
