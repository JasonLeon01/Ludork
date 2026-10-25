using System;

namespace Ludork.Plugin.Avalonia;

public sealed class MapRegionEventArgs(MapRegion region) : EventArgs
{
    public MapRegion Region { get; } = region;
}
