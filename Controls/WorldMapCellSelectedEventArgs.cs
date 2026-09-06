using System;

namespace Ludork.Controls;

public sealed class WorldMapCellSelectedEventArgs(int x, int y) : EventArgs
{
    public int X { get; } = x;
    public int Y { get; } = y;
}
