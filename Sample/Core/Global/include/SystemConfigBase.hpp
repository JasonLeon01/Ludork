#pragma once

#include <BindAnnotations.hpp>
#include <ConfigParser.hpp>

#include <cstdint>
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

    ////////////////////////////////////////////////////////////
    /// \brief Get the current positive display scale
    ///
    /// - \return The actual scale used by rendering and input mapping
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static float getScale();
    ////////////////////////////////////////////////////////////
    /// \brief Get the configured display scale
    ///
    /// Zero selects borderless fullscreen on desktop. Non-finite and negative
    /// values are normalised to one.
    ///
    /// - \return The configured value, including zero
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static float getConfiguredScale();
    ////////////////////////////////////////////////////////////
    /// \brief Apply and save a display scale
    ///
    /// The display change is applied between complete frames.
    ///
    /// - \param value Scale preference; zero selects desktop fullscreen
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void setScale(float value);
    ////////////////////////////////////////////////////////////
    /// \brief Apply a display scale without saving it
    ///
    /// - \param value Scale preference; zero selects desktop fullscreen
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void applyScale(float value);
    ////////////////////////////////////////////////////////////
    /// \brief Save a display scale without applying it
    ///
    /// - \param value Scale preference; zero selects desktop fullscreen
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void saveScale(float value);

    ////////////////////////////////////////////////////////////
    /// \brief Get the configured maximum render scale
    ///
    /// Zero leaves the internal render scale uncapped. A positive value caps
    /// the actual surface-fit scale without changing the window size.
    ///
    /// - \return The configured maximum scale, including zero
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static float getMaximumRenderScale();
    ////////////////////////////////////////////////////////////
    /// \brief Apply and save a maximum render scale
    ///
    /// - \param value Maximum scale; zero leaves rendering uncapped
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void setMaximumRenderScale(float value);
    ////////////////////////////////////////////////////////////
    /// \brief Save a maximum render scale without applying it
    ///
    /// - \param value Maximum scale; zero leaves rendering uncapped
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void saveMaximumRenderScale(float value);

    ////////////////////////////////////////////////////////////
    /// \brief Get the configured lighting render scale
    ///
    /// The supported values are 0.5, 0.75 and 1.0. Other values are
    /// normalised to one.
    ///
    /// - \return The configured lighting render scale
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static float getLightingRenderScale();
    ////////////////////////////////////////////////////////////
    /// \brief Apply and save a lighting render scale
    ///
    /// - \param value Lighting render scale
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void setLightingRenderScale(float value);
    ////////////////////////////////////////////////////////////
    /// \brief Save a lighting render scale without applying it
    ///
    /// - \param value Lighting render scale
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD()
    static void saveLightingRenderScale(float value);

    BIND_METHOD()
    static int getFrameRate();
    BIND_METHOD()
    static void setFrameRate(int value);
    BIND_METHOD()
    static void saveFrameRate(int value);

    BIND_METHOD()
    static int getAntiAliasingLevel();
    BIND_METHOD()
    static void setAntiAliasingLevel(int value);
    BIND_METHOD()
    static void saveAntiAliasingLevel(int value);

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

    static void setChangeHandler(
        std::function<void(const std::string&)> handler);

    static void shutdown() noexcept;

private:
    static void setIniData(const std::string& key, const std::string& value);
    static void afterConfigChanged(const std::string& key);
    static std::string resolveLanguage(const std::string& language);
    static float normalizeScale(float scale);
    static float normalizeMaximumRenderScale(float scale);
    static float normalizeLightingRenderScale(float scale);
    static int normalizeAntiAliasingLevel(std::int64_t level);
    static float clampVolume(float volume);

    static std::shared_ptr<ludork::standard::ConfigParser> data_;
    static std::filesystem::path dataFilePath_;
    static std::string script_;
    static std::string language_;
    static float scale_;
    static float maximumRenderScale_;
    static float lightingRenderScale_;
    static int frameRate_;
    static int antiAliasingLevel_;
    static bool verticalSync_;
    static bool musicOn_;
    static bool soundOn_;
    static bool voiceOn_;
    static float musicVolume_;
    static float soundVolume_;
    static float voiceVolume_;
    static std::function<void(const std::string&)> changeHandler_;
};
