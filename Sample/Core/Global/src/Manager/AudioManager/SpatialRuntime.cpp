#include "SpatialRuntime.hpp"

namespace ludork::global::audio_manager_impl {

namespace {
constexpr float SpatialMinDistance = 64.0f;
}

sf::Vector3f positionOf(
    const std::shared_ptr<sf::Transformable>& transformable) {
    if (transformable == nullptr) {
        return {};
    }
    const sf::Vector2f position = transformable->getPosition();
    return {position.x, position.y, 0.0f};
}

void applyAudioFilter(sf::SoundSource& source, const SoundFilter& filter) {
    if (filter.pan.has_value()) {
        source.setPan(*filter.pan);
    }
    if (filter.spatial.has_value()) {
        source.setSpatializationEnabled(*filter.spatial);
    }
    if (filter.position.has_value()) {
        source.setPosition(*filter.position);
    }
    if (filter.direction.has_value()) {
        source.setDirection(*filter.direction);
    }
    if (filter.cone.has_value()) {
        source.setCone(*filter.cone);
    }
    if (filter.velocity.has_value()) {
        source.setVelocity(*filter.velocity);
    }
    if (filter.dopplerFactor.has_value()) {
        source.setDopplerFactor(*filter.dopplerFactor);
    }
    if (filter.directionalAttenuationFactor.has_value()) {
        source.setDirectionalAttenuationFactor(
            *filter.directionalAttenuationFactor);
    }
    if (filter.relativeToListener.has_value()) {
        source.setRelativeToListener(*filter.relativeToListener);
    }
    if (filter.minDistance.has_value()) {
        source.setMinDistance(*filter.minDistance);
    } else if (filter.spatial.value_or(false)) {
        source.setMinDistance(SpatialMinDistance);
    }
    if (filter.maxDistance.has_value()) {
        source.setMaxDistance(*filter.maxDistance);
    }
    if (filter.minGain.has_value()) {
        source.setMinGain(*filter.minGain);
    }
    if (filter.maxGain.has_value()) {
        source.setMaxGain(*filter.maxGain);
    }
    if (filter.attenuation.has_value()) {
        source.setAttenuation(*filter.attenuation);
    }
}

void applySoundSettings(sf::Sound& sound, const SoundFilter& filter) {
    if (filter.loop.has_value()) {
        sound.setLooping(*filter.loop);
    }
    if (filter.offset.has_value()) {
        sound.setPlayingOffset(*filter.offset);
    }
    applyAudioFilter(sound, filter);
}

void applyMusicSettings(sf::Music& music, const MusicFilter& filter) {
    if (filter.loop.has_value()) {
        music.setLooping(*filter.loop);
    }
    if (filter.offset.has_value()) {
        music.setPlayingOffset(*filter.offset);
    }
    if (filter.loopPoint.has_value()) {
        music.setLoopPoints(*filter.loopPoint);
    }
    applyAudioFilter(music, filter);
    music.setSpatializationEnabled(false);
}

bool isSpatial(const SoundFilter* filter) {
    return filter != nullptr && filter->spatial.value_or(false);
}

}  // namespace ludork::global::audio_manager_impl
