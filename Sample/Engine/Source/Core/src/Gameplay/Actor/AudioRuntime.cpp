#include "AudioRuntime.hpp"

#include <SFML/Audio/Listener.hpp>

#include <cmath>

namespace ludork::engine::actor_impl {

float listenerDistance(const sf::Vector2f& position) {
    const sf::Vector3f listenerPosition = sf::Listener::getPosition();
    const float x = position.x - listenerPosition.x;
    const float y = position.y - listenerPosition.y;
    return std::sqrt(x * x + y * y);
}

SoundFilter buildSoundFilter(const AutoSoundParams& params,
                             const sf::Vector2f& position) {
    SoundFilter filter;
    filter.volume = params.volume;
    filter.spatial = true;
    filter.position = sf::Vector3f(position.x, position.y, 0.0f);
    filter.relativeToListener = false;
    filter.minDistance = params.minDistance;
    filter.attenuation = params.attenuation;
    if (params.loop) {
        filter.loop = true;
    }
    if (params.maxDistance > 0.0f) {
        filter.maxDistance = params.maxDistance;
    }
    return filter;
}

}  // namespace ludork::engine::actor_impl
