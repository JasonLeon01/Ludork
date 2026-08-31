#pragma once

#include <Gameplay/ActorApiTypes.hpp>

#include <SFML/Audio/Sound.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>

#include <memory>
#include <optional>
#include <vector>

namespace ludork::engine::actor_impl {

struct ActorRuntime {
    bool moving = false;
    bool inRoute = false;
    std::optional<std::vector<sf::Vector2i>> route =
        std::vector<sf::Vector2i>{};
    bool moveEnabled = true;
    std::optional<sf::Vector2f> departure;
    std::optional<sf::Vector2f> destination;
    std::optional<sf::Vector2i> moveOriginMapPosition;
    float realSpeed = 0.0f;
    std::shared_ptr<sf::Sound> autoSoundObject;
    float autoSoundCooldown = 0.0f;
    std::optional<sf::Vector3f> autoSoundLastPosition;
    std::shared_ptr<AutoSoundParams> autoSoundParams =
        std::make_shared<AutoSoundParams>();
};

}  // namespace ludork::engine::actor_impl
