using System;

namespace Ludork.Controls;

public sealed class WorldMapPlacementRemovedEventArgs(
    string worldKey,
    string childMapKey) : EventArgs
{
    public string WorldKey { get; } = worldKey;
    public string ChildMapKey { get; } = childMapKey;
}
