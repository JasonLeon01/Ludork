#include <General/TileLayerData.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {
std::size_t requireDimension(int value, const char* name) {
    if (value < 0) {
        throw std::invalid_argument(std::string(name) +
                                    " must not be negative");
    }
    return static_cast<std::size_t>(value);
}

TileGrid makeTileGrid(int width, int height) {
    return TileGrid(requireDimension(height, "TileLayerData height"),
                    std::vector<std::optional<int>>(
                        requireDimension(width, "TileLayerData width")));
}

AutoTileGrid makeAutoTileGrid(int width, int height) {
    return AutoTileGrid(requireDimension(height, "TileLayerData height"),
                        std::vector<AutoTileCell>(
                            requireDimension(width, "TileLayerData width")));
}

template <typename Grid>
std::size_t requireRectangularWidth(const Grid& grid, const char* name) {
    const std::size_t width = grid.empty() ? 0 : grid.front().size();
    if (std::any_of(grid.begin(), grid.end(), [width](const auto& row) {
            return row.size() != width;
        })) {
        throw std::invalid_argument(std::string(name) + " must be rectangular");
    }
    return width;
}
}  // namespace

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

TileLayerData::TileLayerData(std::string layerName, Tileset layerTileset,
                             int width, int height,
                             std::vector<AutoTile> autoTilePool,
                             std::vector<std::string> autoTileKeys,
                             std::string shaderPath)
    : layerName(std::move(layerName)),
      layerTileset(std::move(layerTileset)),
      tiles(makeTileGrid(width, height)),
      autoTiles(makeAutoTileGrid(width, height)),
      autoTilePool(std::move(autoTilePool)),
      autoTileKeys(std::move(autoTileKeys)),
      shaderPath(std::move(shaderPath)) {}

void TileLayerData::writeBlock(int x, int y, const TileGrid& tileBlock,
                               const AutoTileGrid& autoTileBlock) {
    validateBlock(x, y, tileBlock, autoTileBlock);
    const std::size_t destinationX = static_cast<std::size_t>(x);
    const std::size_t destinationY = static_cast<std::size_t>(y);
    for (std::size_t row = 0; row < tileBlock.size(); ++row) {
        std::copy(tileBlock[row].begin(), tileBlock[row].end(),
                  tiles[destinationY + row].begin() + destinationX);
        std::copy(autoTileBlock[row].begin(), autoTileBlock[row].end(),
                  autoTiles[destinationY + row].begin() + destinationX);
    }
}

void TileLayerData::validateBlock(int x, int y, const TileGrid& tileBlock,
                                  const AutoTileGrid& autoTileBlock) const {
    const std::size_t targetWidth =
        requireRectangularWidth(tiles, "TileLayerData tiles");
    const std::size_t autoTargetWidth =
        requireRectangularWidth(autoTiles, "TileLayerData autoTiles");
    if (autoTiles.size() != tiles.size() || autoTargetWidth != targetWidth) {
        throw std::invalid_argument(
            "TileLayerData tiles and autoTiles must have matching dimensions");
    }
    const std::size_t blockWidth =
        requireRectangularWidth(tileBlock, "TileLayerData tile block");
    const std::size_t autoBlockWidth =
        requireRectangularWidth(autoTileBlock, "TileLayerData autotile block");
    if (autoTileBlock.size() != tileBlock.size() ||
        autoBlockWidth != blockWidth) {
        throw std::invalid_argument(
            "TileLayerData tile and autotile blocks must have matching "
            "dimensions");
    }
    const std::size_t destinationX =
        requireDimension(x, "TileLayerData block x");
    const std::size_t destinationY =
        requireDimension(y, "TileLayerData block y");
    if (destinationY > tiles.size() ||
        tileBlock.size() > tiles.size() - destinationY ||
        destinationX > targetWidth || blockWidth > targetWidth - destinationX) {
        throw std::out_of_range("TileLayerData block is outside the grid");
    }
}
