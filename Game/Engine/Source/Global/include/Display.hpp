#pragma once

#include <CoreMinimal.hpp>

BIND_CLASS()
class Display {
public:
    BIND_METHOD(metadata = false)
    static void initializeDisplay(const std::string& title,
                                  const sf::Vector2u& gameSize,
                                  const std::string& iconPath,
                                  const std::string& cursorPath);

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
    /// \brief Get the largest configurable display scale
    ///
    /// Desktop uses the current window's display work area when available,
    /// otherwise the primary display. A configurable mobile host uses its
    /// host-reported maximum windowed dimensions. Embedded and
    /// non-configurable mobile displays return no value.
    ///
    /// - \param gameSize Non-zero logical game size
    /// - \return Maximum scale, or no value when display size is unavailable
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true)
    static std::optional<float> getMaximumWindowedScale(
        const sf::Vector2u& gameSize);

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
    /// \brief Check whether the current host can apply display scale changes
    ///
    /// Standalone desktop windows support scale changes. Embedded displays and
    /// ordinary mobile hosts do not; a mobile host may register support.
    ///
    /// - \return True when display scale changes can be applied
    ///
    ////////////////////////////////////////////////////////////
    BIND_METHOD(Pure = true)
    static bool isDisplayScaleConfigurable();

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

    BIND_METHOD(Pure = true)
    static sf::Vector2u getGameSize();

    static void setGameSize(const sf::Vector2u& gameSize);

    static void initWindow(const std::shared_ptr<sf::RenderWindow>& window);

    static std::shared_ptr<sf::RenderWindow> getWindow();

    BIND_METHOD()
    static void setInputMethodDisabled(bool disabled);

    static void onConfigurationChanged(const std::string& key);
};
