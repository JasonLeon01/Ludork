#pragma once

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <memory>
#include <optional>
#include <string>

namespace ludork::global::panorama_controller_impl {

struct PanoramaControllerImpl {
    std::string graphic;
    bool active = false;
    std::shared_ptr<sf::Texture> texture;
    std::optional<sf::Sprite> sprite;
};

PanoramaControllerImpl& panoramaControllerImpl();

}  // namespace ludork::global::panorama_controller_impl
