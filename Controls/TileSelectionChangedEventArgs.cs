using Ludork.ViewModels;
using System;

namespace Ludork.Controls;

public sealed class TileSelectionChangedEventArgs(TileSelection? tiles, string? autoTileKey) : EventArgs
{
    public TileSelection? Tiles { get; } = tiles;
    public string? AutoTileKey { get; } = autoTileKey;
}
