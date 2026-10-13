#pragma once

#include <SFML/Graphics/Texture.hpp>

#include <memory>
#include <string>

namespace ludork::global::panorama_controller_impl {

struct PanoramaControllerImpl {
    std::string graphic;
    bool active = false;
    std::shared_ptr<sf::Texture> texture;
};

PanoramaControllerImpl& panoramaControllerImpl();

}  // namespace ludork::global::panorama_controller_impl
