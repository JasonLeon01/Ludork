#include "MapPreviewData.hpp"

#include <Runtime/RuntimeDataReader.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ludork::preview_host::map_preview_data {
namespace {

float scalar(const RuntimeData& value) {
    return ludork::runtime::value_reader::requireFloat(value,
                                                       "Map preview value");
}

template <typename Grid, typename Reader>
void readGrid(const RuntimeData::Map& layer, const std::string& key, Grid& grid,
              Reader reader) {
    const RuntimeData* source = optional(layer, key);
    if (source == nullptr || source->isNil()) {
        return;
    }
    const RuntimeData::Array& rows = array(*source);
    for (std::size_t y = 0; y < std::min(rows.size(), grid.size()); ++y) {
        if (rows[y].isNil()) {
            continue;
        }
        const RuntimeData::Array& cells = array(rows[y]);
        for (std::size_t x = 0; x < std::min(cells.size(), grid[y].size());
             ++x) {
            if (!cells[x].isNil()) {
                grid[y][x] = reader(cells[x]);
            }
        }
    }
}

}  // namespace

const RuntimeData::Map& map(const RuntimeData& value) {
    return ludork::runtime::value_reader::requireMap(value,
                                                     "Map preview object");
}

const RuntimeData::Array& array(const RuntimeData& value) {
    return ludork::runtime::value_reader::requireArray(value,
                                                       "Map preview array");
}

const RuntimeData& required(const RuntimeData::Map& value,
                            const std::string& key) {
    return ludork::runtime::value_reader::requireValue(value, key,
                                                       "Map preview");
}

const RuntimeData* optional(const RuntimeData::Map& value,
                            const std::string& key) {
    return ludork::runtime::value_reader::findValue(value, key);
}

float number(const RuntimeData::Map& value, const std::string& key,
             float fallback) {
    const RuntimeData* item = optional(value, key);
    return item == nullptr ? fallback : scalar(*item);
}

int integer(const RuntimeData::Map& value, const std::string& key,
            int fallback) {
    const RuntimeData* item = optional(value, key);
    return item == nullptr ? fallback
                           : ludork::runtime::value_reader::requireInt(
                                 *item, "Map preview." + key);
}

bool boolean(const RuntimeData::Map& value, const std::string& key,
             bool fallback) {
    const RuntimeData* item = optional(value, key);
    return item == nullptr ? fallback
                           : ludork::runtime::value_reader::requireBool(
                                 *item, "Map preview." + key);
}

std::string text(const RuntimeData::Map& value, const std::string& key,
                 const std::string& fallback) {
    const RuntimeData* item = optional(value, key);
    return item == nullptr || item->isNil()
               ? fallback
               : ludork::runtime::value_reader::requireString(
                     *item, "Map preview." + key);
}

sf::Vector2f vector(const RuntimeData::Map& value, const std::string& key,
                    sf::Vector2f fallback) {
    const RuntimeData* item = optional(value, key);
    if (item == nullptr) {
        return fallback;
    }
    const RuntimeData::Array& values = array(*item);
    if (values.size() != 2) {
        throw std::invalid_argument(
            "Map preview vector must have two elements");
    }
    return {scalar(values[0]), scalar(values[1])};
}

sf::Color colour(const RuntimeData::Map& value, const std::string& key,
                 sf::Color fallback) {
    const RuntimeData* item = optional(value, key);
    if (item == nullptr) {
        return fallback;
    }
    const RuntimeData::Array& values = array(*item);
    if (values.size() != 4) {
        throw std::invalid_argument(
            "Map preview colour must have four elements");
    }
    std::uint8_t channels[4];
    for (std::size_t index = 0; index < 4; ++index) {
        channels[index] = static_cast<std::uint8_t>(
            std::clamp(scalar(values[index]), 0.0f, 255.0f));
    }
    return {channels[0], channels[1], channels[2], channels[3]};
}

Tileset tileset(const RuntimeData& value) {
    return Tileset::fromData(map(value));
}

AutoTile autoTile(const RuntimeData& value) {
    const RuntimeData::Map& fields = map(value);
    AutoTile result;
    result.name = text(fields, "name");
    result.fileName = text(fields, "fileName");
    result.passable = boolean(fields, "passable", true);
    result.material = text(fields, "material");
    return result;
}

MapFogSettings fog(const RuntimeData::Map& value) {
    MapFogSettings result;
    const RuntimeData* item = optional(value, "fog");
    if (item == nullptr || item->isNil()) {
        return result;
    }
    const RuntimeData::Map& fields = map(*item);
    result.enabled = boolean(fields, "enabled", false);
    result.colour = colour(fields, "colour");
    result.density = number(fields, "density", 0.5f);
    result.scale = number(fields, "scale", 256);
    result.velocity = vector(fields, "velocity", {15, 15});
    result.turbulence = number(fields, "turbulence", 0.5f);
    result.seed = integer(fields, "seed", 0);
    if (result.density < 0 || result.density > 1 || result.scale <= 0 ||
        result.turbulence < 0 || result.turbulence > 1) {
        throw std::invalid_argument(
            "Map fog density/turbulence must be in [0,1] and scale must be "
            "positive");
    }
    return result;
}

TileGrid tiles(const RuntimeData::Map& layer, int width, int height) {
    TileGrid result(height, std::vector<std::optional<int>>(width));
    readGrid(layer, "tiles", result, [](const RuntimeData& value) {
        return ludork::runtime::value_reader::requireInt(value,
                                                         "Map tile index");
    });
    return result;
}

AutoTileGrid autoTiles(const RuntimeData::Map& layer, int width, int height) {
    AutoTileGrid result(height, std::vector<AutoTileCell>(width));
    readGrid(layer, "autoTiles", result,
             [](const RuntimeData& value) -> AutoTileCell {
                 return ludork::runtime::value_reader::requireString(
                     value, "Map autotile key");
             });
    return result;
}

}  // namespace ludork::preview_host::map_preview_data
