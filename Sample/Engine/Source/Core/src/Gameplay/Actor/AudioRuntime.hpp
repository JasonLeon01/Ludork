#pragma once

#include <Filters/SoundFilter.hpp>
#include <Gameplay/ActorApiTypes.hpp>

#include <SFML/System/Vector2.hpp>

namespace ludork::engine::actor_impl {

float listenerDistance(const sf::Vector2f& position);
SoundFilter buildSoundFilter(const AutoSoundParams& params,
                             const sf::Vector2f& position);

}  // namespace ludork::engine::actor_impl
