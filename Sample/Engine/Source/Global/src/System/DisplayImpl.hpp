#pragma once

#include <SFML/Graphics.hpp>

#include <memory>
#include <optional>
#include <string>

namespace ludork::global {
struct WindowedFramePlacement;
}

namespace ludork::global::system_display_impl {

bool viewsEqual(const sf::View& left, const sf::View& right);
float windowFitScale(const sf::Vector2u& surfaceSize,
                     const sf::Vector2u& gameSize);
float effectiveRenderScale(float surfaceFitScale, float maximumRenderScale);
sf::Vector2u scaledSize(const sf::Vector2u& gameSize, float scale);

std::optional<float> getMaximumWindowedScale(const sf::Vector2u& gameSize);
sf::Vector2u getGameSize();
void setGameSize(const sf::Vector2u& gameSize);
void initializeDisplay(const std::string& title, const sf::Vector2u& gameSize,
                       const std::string& iconPath,
                       const std::string& cursorPath);
void initWindow(const std::shared_ptr<sf::RenderWindow>& window);
std::shared_ptr<sf::RenderWindow> getWindow();
bool isEmbeddedDisplay();
bool isMobileDisplay();
bool isDisplayScaleConfigurable();
float windowFitScale(const sf::Vector2u& size);
float effectiveRenderScale(float surfaceFitScale);
sf::Vector2u windowSizeForScale(float scale);
sf::Vector2u renderSizeForScale(float scale);
void applyWindowPresentationSettings();
void recreateDesktopWindow(bool fullscreen, const sf::Vector2u& size);
void replaceWindowedDesktopWindow(
    const sf::Vector2u& size,
    const ludork::global::WindowedFramePlacement* placement);
void updateWindowViewport();
void rebuildDisplayTargets(float surfaceFitScale);
void applyConfiguredScale(float scale);
void observeWindowResize();
void applyPendingDisplayChanges();
void setInputMethodDisabled(bool disabled);
void clearCanvas();
void setWindowMapView(const sf::IntRect& rect);
void setWindowDefaultView();
sf::RenderTexture* getCanvas();

}  // namespace ludork::global::system_display_impl
