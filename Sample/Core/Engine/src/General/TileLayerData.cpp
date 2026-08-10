#include <General/TileLayerData.hpp>

#include <utility>

TileLayerData::TileLayerData(std::string layerName, Tileset layerTileset,
                             TileGrid tiles, AutoTileGrid autoTiles,
                             std::vector<AutoTile> autoTilePool,
                             std::vector<std::string> autoTileKeys,
                             std::string shaderPath)
    : layerName(std::move(layerName)),
      layerTileset(std::move(layerTileset)),
      tiles(std::move(tiles)),
      autoTiles(std::move(autoTiles)),
      autoTilePool(std::move(autoTilePool)),
      autoTileKeys(std::move(autoTileKeys)),
      shaderPath(std::move(shaderPath)) {}
