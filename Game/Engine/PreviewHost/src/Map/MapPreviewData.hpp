#pragma once

#include <General/AutoTile.hpp>
#include <General/TileLayerData.hpp>
#include <Graphics/MapFogSettings.hpp>
#include <Runtime/RuntimeData.hpp>

#include <string>

namespace ludork::preview_host::map_preview_data {

const RuntimeData::Map& map(const RuntimeData& value);
const RuntimeData::Array& array(const RuntimeData& value);
const RuntimeData& required(const RuntimeData::Map& value,
                            const std::string& key);
const RuntimeData* optional(const RuntimeData::Map& value,
                            const std::string& key);
float number(const RuntimeData::Map& value, const std::string& key,
             float fallback);
int integer(const RuntimeData::Map& value, const std::string& key,
            int fallback);
bool boolean(const RuntimeData::Map& value, const std::string& key,
             bool fallback);
std::string text(const RuntimeData::Map& value, const std::string& key,
                 const std::string& fallback = {});
sf::Vector2f vector(const RuntimeData::Map& value, const std::string& key,
                    sf::Vector2f fallback = {});
sf::Color colour(const RuntimeData::Map& value, const std::string& key,
                 sf::Color fallback = sf::Color::White);
Tileset tileset(const RuntimeData& value);
AutoTile autoTile(const RuntimeData& value);
MapFogSettings fog(const RuntimeData::Map& value);
TileGrid tiles(const RuntimeData::Map& layer, int width, int height);
AutoTileGrid autoTiles(const RuntimeData::Map& layer, int width, int height);

}  // namespace ludork::preview_host::map_preview_data
