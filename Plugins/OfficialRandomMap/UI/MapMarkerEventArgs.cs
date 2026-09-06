using System;

namespace Ludork.Plugins.OfficialRandomMap.UI;

internal sealed class MapMarkerEventArgs(int x, int y) : EventArgs
{
    public int X { get; } = x;

    public int Y { get; } = y;
}
