#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>

#include <SFML/Audio.hpp>
#include <SFML/System/Time.hpp>

#include <optional>

BIND_CLASS(table_init = true)
class SoundFilter {
public:
    BIND_PROPERTY()
    std::optional<bool> loop;

    BIND_PROPERTY()
    std::optional<sf::Time> offset;

    BIND_PROPERTY()
    std::optional<float> pitch;

    BIND_PROPERTY()
    std::optional<float> pan;

    BIND_PROPERTY()
    std::optional<float> volume;

    BIND_PROPERTY()
    std::optional<bool> spatial;

    BIND_PROPERTY()
    std::optional<sf::Vector3f> position;

    BIND_PROPERTY()
    std::optional<sf::Vector3f> direction;

    BIND_PROPERTY()
    std::optional<sf::SoundSource::Cone> cone;

    BIND_PROPERTY()
    std::optional<sf::Vector3f> velocity;

    BIND_PROPERTY()
    std::optional<float> dopplerFactor;

    BIND_PROPERTY()
    std::optional<float> directionalAttenuationFactor;

    BIND_PROPERTY()
    std::optional<bool> relativeToListener;

    BIND_PROPERTY()
    std::optional<float> minDistance;

    BIND_PROPERTY()
    std::optional<float> maxDistance;

    BIND_PROPERTY()
    std::optional<float> minGain;

    BIND_PROPERTY()
    std::optional<float> maxGain;

    BIND_PROPERTY()
    std::optional<float> attenuation;
};

BIND_CLASS(table_init = true)
class MusicFilter : public SoundFilter {
public:
    BIND_PROPERTY()
    std::optional<sf::Music::TimeSpan> loopPoint;
};
