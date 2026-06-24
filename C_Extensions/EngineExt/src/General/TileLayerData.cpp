#include <General/TileLayerData.hpp>

TileLayerData::TileLayerData(std::string layerName, Tileset layerTileset, TileGrid tiles, AutoTileGrid autoTiles,
                             std::vector<AutoTile> autoTilePool, std::vector<std::string> autoTileKeys,
                             std::string shaderPath)
    : layerName(layerName),
      layerTileset(layerTileset),
      tiles(tiles),
      autoTiles(autoTiles),
      autoTilePool(autoTilePool),
      autoTileKeys(autoTileKeys),
      shaderPath(shaderPath) {}
