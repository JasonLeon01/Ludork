#pragma once

#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace ludork::global::fog_controller_impl {

struct WorldFogLayer {
    std::string graphic;
    float power = 0.0f;
    sf::Vector2f scroll;
    float distort = 0.0f;
    sf::IntRect cellRect;
    std::shared_ptr<sf::Texture> texture;
};

struct WorldFogState {
    std::optional<WorldFogLayer> base;
    std::unordered_map<std::string, WorldFogLayer> regions;
    float time = 0.0f;
};

struct WorldFogRuntime {
    std::optional<WorldFogState> state;
};

WorldFogRuntime& worldFogRuntime();

}  // namespace ludork::global::fog_controller_impl
