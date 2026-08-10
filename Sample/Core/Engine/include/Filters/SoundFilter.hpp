#pragma once

#include <BindAnnotations.hpp>

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
    bool needEffect = false;

    BIND_PROPERTY(metadata_type = "function")
    std::optional<sf::SoundSource::EffectProcessor> soundEffect;

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

BIND_FUNCTION()
sf::SoundSource::EffectProcessor echoEffect(float delay, float decay,
                                            float sampleRate);

BIND_FUNCTION(defaults = {0.7})
sf::SoundSource::EffectProcessor distortionEffect(float drive,
                                                  float threshold = 0.7f);

BIND_FUNCTION(defaults = {0.7, 0.3, 44100.0})
sf::SoundSource::EffectProcessor underwaterEffect(float depth = 0.7f,
                                                  float bubbleIntensity = 0.3f,
                                                  float sampleRate = 44100.0f);
