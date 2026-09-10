#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window/Cursor.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace ludork::global {
struct WindowedFramePlacement;
}

namespace ludork::global::system_impl {

class DisplayImpl {
public:
    float windowFitScale(const sf::Vector2u& surfaceSize,
                         const sf::Vector2u& gameSize);
    float effectiveRenderScale(float surfaceFitScale, float maximumRenderScale);
    std::optional<float> getMaximumWindowedScale(const sf::Vector2u& gameSize);
    sf::Vector2u getGameSize();
    void setGameSize(const sf::Vector2u& gameSize);
    void finishInitialization(const sf::Vector2u& renderSize);
    void prepareInitialization(const std::string& title,
                               const sf::Vector2u& gameSize,
                               const std::string& iconPath,
                               const std::string& cursorPath);
    void createDisplayWindow();
    void initializeInput();
    void initWindow(const std::shared_ptr<sf::RenderWindow>& window);
    std::shared_ptr<sf::RenderWindow> getWindow();
    bool isEmbeddedDisplay();
    bool isMobileDisplay();
    bool isDisplayScaleConfigurable();
    float windowFitScale(const sf::Vector2u& size);
    float effectiveRenderScale(float surfaceFitScale);
    sf::Vector2u windowSizeForScale(float scale);
    sf::Vector2u renderSizeForScale(float scale);
    void updateWindowViewport(const sf::Vector2u& renderSize);
    std::optional<float> applyConfiguredScale(float scale);
    std::optional<float> observeWindowResize(const sf::Vector2u& renderSize);
    void setInputMethodDisabled(bool disabled);
    void present();
    void clearWindow();
    bool isOpen() const;
    void requestConfiguredScale(float scale);
    std::optional<float> takeConfiguredScale();
    void requestRenderTargetRebuild();
    bool takeRenderTargetRebuild();
    float getSurfaceFitScale() const;
    void setSurfaceFitScale(float scale);
    void applyFrameRate();
    void applyVerticalSync();
    void reset();
    void shutdown() noexcept;

private:
    sf::Vector2u scaledSize(const sf::Vector2u& gameSize, float scale);
    void applyWindowPresentationSettings();
    void recreateDesktopWindow(bool fullscreen, const sf::Vector2u& size);
    void replaceWindowedDesktopWindow(
        const sf::Vector2u& size,
        const ludork::global::WindowedFramePlacement* placement);

    std::shared_ptr<sf::RenderWindow> window_;
    mutable std::mutex windowMutex_;
    std::mutex pendingSettingsMutex_;
    std::unique_ptr<sf::Cursor> cursor_;
    std::string windowTitle_;
    std::string windowIconPath_;
    std::string windowCursorPath_;
    sf::ContextSettings windowContextSettings_;
    sf::Vector2u observedWindowSize_;
    std::optional<sf::Vector2u> observedWindowClientSize_;
    float surfaceFitScale_ = 1.0f;
    std::optional<float> pendingConfiguredScale_;
    std::optional<float> pendingResizeScale_;
    bool pendingRenderTargetRebuild_ = false;
    std::chrono::steady_clock::time_point lastResizeTime_;
    bool desktopFullscreen_ = false;
    bool inputMethodDisabled_ = true;
};

}  // namespace ludork::global::system_impl
