#pragma once

#include <Filters/SoundFilter.hpp>

#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundSource.hpp>
#include <SFML/Graphics/Transformable.hpp>

#include <memory>

namespace ludork::global::audio_manager_impl {

sf::Vector3f positionOf(
    const std::shared_ptr<sf::Transformable>& transformable);
void applyAudioFilter(sf::SoundSource& source, const SoundFilter& filter);
void applySoundSettings(sf::Sound& sound, const SoundFilter& filter);
void applyMusicSettings(sf::Music& music, const MusicFilter& filter);
bool isSpatial(const SoundFilter* filter);

}  // namespace ludork::global::audio_manager_impl
