#include <Display.hpp>
#include "System/DisplayImpl.hpp"
#include "System/LifecycleImpl.hpp"
#include "System/FramePipelineImpl.hpp"
#include "System/Config/DisplayConfigImpl.hpp"
#include <GlobalRuntimeApi.hpp>

float Display::getScale() {
    return ludork::global::system_impl::DisplayConfigImpl::getScale();
}

float Display::getConfiguredScale() {
    return ludork::global::system_impl::DisplayConfigImpl::getConfiguredScale();
}

std::optional<float> Display::getMaximumWindowedScale(
    const sf::Vector2u& gameSize) {
    return ludork::global::system_impl::displayImpl().getMaximumWindowedScale(
        gameSize);
}

void Display::setScale(float value) {
    ludork::global::system_impl::DisplayConfigImpl::setScale(value);
}

void Display::applyScale(float value) {
    ludork::global::system_impl::DisplayConfigImpl::applyScale(value);
}

void Display::saveScale(float value) {
    ludork::global::system_impl::DisplayConfigImpl::saveScale(value);
}

int Display::getFrameRate() {
    return ludork::global::system_impl::DisplayConfigImpl::getFrameRate();
}

void Display::setFrameRate(int value) {
    ludork::global::system_impl::DisplayConfigImpl::setFrameRate(value);
}

void Display::saveFrameRate(int value) {
    ludork::global::system_impl::DisplayConfigImpl::saveFrameRate(value);
}

int Display::getAntiAliasingLevel() {
    return ludork::global::system_impl::DisplayConfigImpl::
        getAntiAliasingLevel();
}

void Display::setAntiAliasingLevel(int value) {
    ludork::global::system_impl::DisplayConfigImpl::setAntiAliasingLevel(value);
}

void Display::saveAntiAliasingLevel(int value) {
    ludork::global::system_impl::DisplayConfigImpl::saveAntiAliasingLevel(
        value);
}

bool Display::getVerticalSync() {
    return ludork::global::system_impl::DisplayConfigImpl::getVerticalSync();
}

void Display::setVerticalSync(bool value) {
    ludork::global::system_impl::DisplayConfigImpl::setVerticalSync(value);
}

void Display::saveVerticalSync(bool value) {
    ludork::global::system_impl::DisplayConfigImpl::saveVerticalSync(value);
}

sf::Vector2u Display::getGameSize() {
    return ludork::global::system_impl::displayImpl().getGameSize();
}

void Display::setGameSize(const sf::Vector2u& gameSize) {
    ludork::global::system_impl::displayImpl().setGameSize(gameSize);
}

void Display::initializeDisplay(const std::string& title,
                                const sf::Vector2u& gameSize,
                                const std::string& iconPath,
                                const std::string& cursorPath) {
    ludork::global::system_impl::displayImpl().prepareInitialization(
        title, gameSize, iconPath, cursorPath);
    ludork::global::system_impl::lifecycleImpl().setDebugMode(
        ludork::global::runtimeLaunchOptions().editor);
    ludork::global::system_impl::displayImpl().createDisplayWindow();
    ludork::global::system_impl::framePipelineImpl().initializeGraphics();
    ludork::global::system_impl::displayImpl().initializeInput();
    ludork::global::system_impl::framePipelineImpl().initCanvas(
        ludork::global::system_impl::displayImpl().renderSizeForScale(
            Display::getScale()));
    ludork::global::system_impl::displayImpl().finishInitialization(
        ludork::global::system_impl::framePipelineImpl().getCanvasSize());
}

void Display::initWindow(const std::shared_ptr<sf::RenderWindow>& window) {
    ludork::global::system_impl::displayImpl().initWindow(window);
}

std::shared_ptr<sf::RenderWindow> Display::getWindow() {
    return ludork::global::system_impl::displayImpl().getWindow();
}

bool Display::isDisplayScaleConfigurable() {
    return ludork::global::system_impl::displayImpl()
        .isDisplayScaleConfigurable();
}

void Display::setInputMethodDisabled(bool disabled) {
    ludork::global::system_impl::displayImpl().setInputMethodDisabled(disabled);
}

void Display::onConfigurationChanged(const std::string& key) {
    if (key == "scale") {
        if (ludork::global::system_impl::displayImpl().getWindow() != nullptr) {
            ludork::global::system_impl::displayImpl().requestConfiguredScale(
                getConfiguredScale());
        }
    } else if (key == "frameRate") {
        ludork::global::system_impl::displayImpl().applyFrameRate();
    } else if (key == "verticalSync") {
        ludork::global::system_impl::displayImpl().applyVerticalSync();
    }
}
