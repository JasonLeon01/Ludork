#pragma once

#include <BindAnnotations.hpp>
#include <GlobalRuntimeApi.hpp>

#include <SFML/Audio.hpp>
#include <SFML/Graphics/Transformable.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

class SoundFilter;
class MusicFilter;

BIND_CLASS()
class LUDORK_GLOBAL_API AudioManager {
public:
    BIND_METHOD()
    static std::shared_ptr<sf::SoundBuffer> loadSound(
        const std::string& filePath);

    BIND_METHOD(defaults = {nil, nil})
    static std::shared_ptr<sf::Sound> playSound(
        const std::string& filePath, const SoundFilter* filter = nullptr,
        const std::shared_ptr<sf::Transformable>& parent = nullptr);

    BIND_METHOD()
    static void setSoundParent(
        const std::shared_ptr<sf::Sound>& sound,
        const std::shared_ptr<sf::Transformable>& parent);

    BIND_METHOD(defaults = {nil, nil, 64.0})
    static std::shared_ptr<sf::Sound> playVoice(
        const std::string& filePath, const SoundFilter* filter = nullptr,
        const std::shared_ptr<sf::Transformable>& refActor = nullptr,
        float minDistance = 64.0f);

    BIND_METHOD(defaults = {64.0})
    static void setVoiceRefActor(
        const std::shared_ptr<sf::Sound>& voice,
        const std::shared_ptr<sf::Transformable>& refActor,
        float minDistance = 64.0f);

    BIND_METHOD(defaults = {nil})
    static std::shared_ptr<sf::Music> playMusic(
        const std::string& musicType, const std::string& filePath,
        const MusicFilter* filter = nullptr);

    BIND_METHOD()
    static void stopSound();

    BIND_METHOD()
    static void stopVoice();

    BIND_METHOD()
    static void stopMusic(const std::string& musicType);

    BIND_METHOD()
    static void applySoundVolumes();

    BIND_METHOD()
    static void applyVoiceVolumes();

    BIND_METHOD()
    static void applyMusicVolumes();

    BIND_METHOD()
    static void updateAllSoundPositions();

    BIND_METHOD()
    static void updateAllVoicePositions();

    BIND_METHOD()
    static void setSoundFilter(const std::shared_ptr<sf::Sound>& sound,
                               const SoundFilter& filter);

    BIND_METHOD()
    static void setVoiceFilter(const std::shared_ptr<sf::Sound>& voice,
                               const SoundFilter& filter);

    BIND_METHOD()
    static void setMusicFilter(const std::shared_ptr<sf::Music>& music,
                               const MusicFilter& filter);

    BIND_METHOD(metadata = false)
    static void setEffect(
        const std::string& audioType,
        std::function<sf::SoundSource::EffectProcessor()> effectFactory);

    BIND_METHOD()
    static std::size_t getMemory();

    BIND_IGNORE()
    static void update();

    BIND_IGNORE()
    static void shutdown() noexcept;
};
