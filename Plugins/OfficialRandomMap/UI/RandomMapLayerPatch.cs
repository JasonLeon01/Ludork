using Ludork.Plugin.Abstractions;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Plugins.OfficialRandomMap.UI;

internal static class RandomMapLayerPatch
{
    public static PluginMapLayerWriteRequest Create(
        PluginMapSnapshot map,
        PluginMapLayerSnapshot layer,
        MapRegion region,
        IReadOnlyList<IReadOnlyList<int?>> generated)
    {
        IReadOnlyList<int?>[] tiles = new IReadOnlyList<int?>[map.Height];
        IReadOnlyList<string?>[] autoTiles = new IReadOnlyList<string?>[map.Height];
        for (int y = 0; y < map.Height; y++)
        {
            int?[] tileRow = layer.Tiles[y].ToArray();
            string?[] autoTileRow = layer.AutoTiles[y].ToArray();
            if (y >= region.Y && y < region.Bottom)
            {
                for (int x = region.X; x < region.Right; x++)
                {
                    tileRow[x] = generated[y - region.Y][x - region.X];
                    autoTileRow[x] = null;
                }
            }
            tiles[y] = tileRow;
            autoTiles[y] = autoTileRow;
        }
        return new PluginMapLayerWriteRequest(
            map.Key, layer.Name, map.Revision, layer.TilesetKey, tiles, autoTiles);
    }
}
