#pragma once
#include <Manager/AudioEffectControl.hpp>

#include <CoreMinimal.hpp>

#include <GlobalRuntimeApi.hpp>
#include <Manager/ManagedAudioSource.hpp>

#include <SFML/Audio.hpp>

struct lua_State;

class SoundFilter;
class MusicFilter;

BIND_CLASS()
class LUDORK_GLOBAL_API AudioManager {
private:
    enum class SoundCategory {
        Unmanaged,
        Sound,
        Voice
    };

public:
    BIND_METHOD()
    static bool getMusicOn();

    BIND_METHOD()
    static void setMusicOn(bool value);

    BIND_METHOD()
    static void saveMusicOn(bool value);

    BIND_METHOD()
    static bool getSoundOn();

    BIND_METHOD()
    static void setSoundOn(bool value);

    BIND_METHOD()
    static void saveSoundOn(bool value);

    BIND_METHOD()
    static bool getVoiceOn();

    BIND_METHOD()
    static void setVoiceOn(bool value);

    BIND_METHOD()
    static void saveVoiceOn(bool value);

    BIND_METHOD()
    static float getMusicVolume();

    BIND_METHOD()
    static void setMusicVolume(float value);

    BIND_METHOD()
    static void saveMusicVolume(float value);

    BIND_METHOD()
    static float getSoundVolume();

    BIND_METHOD()
    static void setSoundVolume(float value);

    BIND_METHOD()
    static void saveSoundVolume(float value);

    BIND_METHOD()
    static float getVoiceVolume();

    BIND_METHOD()
    static void setVoiceVolume(float value);

    BIND_METHOD()
    static void saveVoiceVolume(float value);

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

    BIND_METHOD(metadata = false, allow_nil = "effect",
                parameter_types = {string, Source.Utils.AudioEffects.Attacher})
    static void setEffect(
        const std::string& audioType,
        std::function<void(sf::SoundSource&,
                           std::shared_ptr<AudioEffectControl>, std::uint32_t)>
            effect);

    BIND_METHOD()
    static std::size_t getMemory();

    static void initialize(lua_State* state);

    static void update();

    static void stopAll();

    static void shutdown() noexcept;
};
