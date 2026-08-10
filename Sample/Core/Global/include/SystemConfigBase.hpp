#pragma once

#include <BindAnnotations.hpp>
#include <ConfigParser.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

BIND_CLASS()
class SystemConfigBase {
public:
    BIND_METHOD(parameter_types = {ConfigParser, string})
    static void init(
        const std::shared_ptr<ludork::standard::ConfigParser>& data,
        const std::string& dataFilePath);

    BIND_METHOD()
    static std::string getScript();
    BIND_METHOD()
    static void setScript(const std::string& value);
    BIND_METHOD()
    static void saveScript(const std::string& value);

    BIND_METHOD()
    static std::string getLanguage();
    BIND_METHOD()
    static void setLanguage(const std::string& value);
    BIND_METHOD()
    static void saveLanguage(const std::string& value);

    BIND_METHOD()
    static float getScale();
    BIND_METHOD()
    static float getConfiguredScale();
    BIND_METHOD()
    static void setScale(float value);
    BIND_METHOD()
    static void applyScale(float value);
    BIND_METHOD()
    static void saveScale(float value);

    BIND_METHOD()
    static int getFrameRate();
    BIND_METHOD()
    static void setFrameRate(int value);
    BIND_METHOD()
    static void saveFrameRate(int value);

    BIND_METHOD()
    static bool getVerticalSync();
    BIND_METHOD()
    static void setVerticalSync(bool value);
    BIND_METHOD()
    static void saveVerticalSync(bool value);

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

    BIND_IGNORE()
    static void setChangeHandler(
        std::function<void(const std::string&)> handler);

    BIND_IGNORE()
    static void shutdown() noexcept;

private:
    static void setIniData(const std::string& key, const std::string& value);
    static void afterConfigChanged(const std::string& key);
    static std::string resolveLanguage(const std::string& language);
    static float clampVolume(float volume);

    static std::shared_ptr<ludork::standard::ConfigParser> data_;
    static std::filesystem::path dataFilePath_;
    static std::string script_;
    static std::string language_;
    static float scale_;
    static int frameRate_;
    static bool verticalSync_;
    static bool musicOn_;
    static bool soundOn_;
    static bool voiceOn_;
    static float musicVolume_;
    static float soundVolume_;
    static float voiceVolume_;
    static std::function<void(const std::string&)> changeHandler_;
};
