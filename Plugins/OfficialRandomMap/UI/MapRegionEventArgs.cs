using System;

namespace Ludork.Plugins.OfficialRandomMap.UI;

internal sealed class MapRegionEventArgs(MapRegion region) : EventArgs
{
    public MapRegion Region { get; } = region;
}
