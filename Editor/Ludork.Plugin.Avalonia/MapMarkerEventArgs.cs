using System;

namespace Ludork.Plugin.Avalonia;

public sealed class MapMarkerEventArgs(int x, int y) : EventArgs
{
    public int X { get; } = x;

    public int Y { get; } = y;
}
