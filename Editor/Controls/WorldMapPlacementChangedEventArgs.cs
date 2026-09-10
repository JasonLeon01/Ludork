using System;

namespace Ludork.Controls;

public sealed class WorldMapPlacementChangedEventArgs(
    string worldKey,
    string childMapKey,
    int x,
    int y) : EventArgs
{
    public string WorldKey { get; } = worldKey;
    public string ChildMapKey { get; } = childMapKey;
    public int X { get; } = x;
    public int Y { get; } = y;
}
