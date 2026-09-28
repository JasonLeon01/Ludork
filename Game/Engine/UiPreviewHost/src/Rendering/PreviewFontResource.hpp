#pragma once

#include <Runtime/AssetInputStream.hpp>
#include <SFML/Graphics/Font.hpp>

#include <memory>

namespace ludork::preview_host {

struct PreviewFontResource {
    std::unique_ptr<ludork::runtime::AssetInputStream> stream;
    sf::Font font;
};

}  // namespace ludork::preview_host
