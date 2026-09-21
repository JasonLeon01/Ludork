#pragma once

namespace ludork::global::system_impl {

class AudioConfigImpl {
public:
    static void initialize();
    static void shutdown() noexcept;
    static bool getMusicOn();
    static void setMusicOn(bool value);
    static void saveMusicOn(bool value);
    static bool getSoundOn();
    static void setSoundOn(bool value);
    static void saveSoundOn(bool value);
    static bool getVoiceOn();
    static void setVoiceOn(bool value);
    static void saveVoiceOn(bool value);
    static float getMusicVolume();
    static void setMusicVolume(float value);
    static void saveMusicVolume(float value);
    static float getSoundVolume();
    static void setSoundVolume(float value);
    static void saveSoundVolume(float value);
    static float getVoiceVolume();
    static void setVoiceVolume(float value);
    static void saveVoiceVolume(float value);

private:
    static float clampVolume(float volume);
    static bool musicOn_;
    static bool soundOn_;
    static bool voiceOn_;
    static float musicVolume_;
    static float soundVolume_;
    static float voiceVolume_;
    static bool initialized_;
};

}  // namespace ludork::global::system_impl
