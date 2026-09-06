#pragma once

#include <Runtime/AssetInputStream.hpp>
#include <SFML/Graphics/Font.hpp>
#include <memory>

namespace ludork::engine::text_config {

struct FontResource {
    std::unique_ptr<ludork::runtime::AssetInputStream> stream;
    sf::Font font;
};

struct StyleFlags {
    bool bold = false;
    bool italic = false;
    bool underlined = false;
    bool strikeThrough = false;
};

}  // namespace ludork::engine::text_config
