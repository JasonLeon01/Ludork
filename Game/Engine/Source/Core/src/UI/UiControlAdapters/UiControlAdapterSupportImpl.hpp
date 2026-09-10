#pragma once

#include <SFML/Graphics/Texture.hpp>
#include <memory>
#include <mutex>

namespace ui_control_adapter_detail {

struct PlaceholderTextureCache {
    std::mutex mutex;
    std::shared_ptr<sf::Texture> texture;
};

}  // namespace ui_control_adapter_detail
