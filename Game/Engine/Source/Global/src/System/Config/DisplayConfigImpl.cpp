#include "DisplayConfigImpl.hpp"
#include "ConfigStoreImpl.hpp"

#include <Display.hpp>
#include <EngineState.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <cmath>
#include <limits>

namespace ludork::global::system_impl {

namespace {
constexpr int DefaultFrameRate = 60;
constexpr int DefaultAntiAliasingLevel = 8;
}  // namespace

float DisplayConfigImpl::scale_ = 0.0f;
int DisplayConfigImpl::frameRate_ = DefaultFrameRate;
int DisplayConfigImpl::antiAliasingLevel_ = DefaultAntiAliasingLevel;
bool DisplayConfigImpl::verticalSync_ = true;
bool DisplayConfigImpl::initialized_ = false;

void DisplayConfigImpl::initialize() {
    scale_ = normalizeScale(static_cast<float>(
        ConfigStoreImpl::data().getFloat("Main", "scale").value_or(scale_)));
    frameRate_ = static_cast<int>(ConfigStoreImpl::data()
                                      .getInt("Main", "frameRate")
                                      .value_or(frameRate_));
    antiAliasingLevel_ =
        normalizeAntiAliasingLevel(ConfigStoreImpl::data()
                                       .getInt("Main", "antiAliasingLevel")
                                       .value_or(antiAliasingLevel_));
    verticalSync_ = ConfigStoreImpl::data()
                        .getBoolean("Main", "verticalSync")
                        .value_or(verticalSync_);
    initialized_ = true;
}

void DisplayConfigImpl::shutdown() noexcept {
    initialized_ = false;
    scale_ = 0.0f;
    frameRate_ = DefaultFrameRate;
    antiAliasingLevel_ = DefaultAntiAliasingLevel;
    verticalSync_ = true;
}

int DisplayConfigImpl::getFrameRate() {
    return frameRate_;
}

void DisplayConfigImpl::setFrameRate(int value) {
    frameRate_ = value;
    saveFrameRate(value);
    if (initialized_) {
        Display::onConfigurationChanged("frameRate");
    }
}

void DisplayConfigImpl::saveFrameRate(int value) {
    ConfigStoreImpl::setIniData("frameRate", value);
}

int DisplayConfigImpl::getAntiAliasingLevel() {
    return antiAliasingLevel_;
}

void DisplayConfigImpl::setAntiAliasingLevel(int value) {
    antiAliasingLevel_ = normalizeAntiAliasingLevel(value);
    saveAntiAliasingLevel(antiAliasingLevel_);
    if (initialized_) {
        Display::onConfigurationChanged("antiAliasingLevel");
    }
}

void DisplayConfigImpl::saveAntiAliasingLevel(int value) {
    ConfigStoreImpl::setIniData("antiAliasingLevel",
                                normalizeAntiAliasingLevel(value));
}

bool DisplayConfigImpl::getVerticalSync() {
    return verticalSync_;
}

void DisplayConfigImpl::setVerticalSync(bool value) {
    verticalSync_ = value;
    saveVerticalSync(value);
    if (initialized_) {
        Display::onConfigurationChanged("verticalSync");
    }
}

void DisplayConfigImpl::saveVerticalSync(bool value) {
    ConfigStoreImpl::setIniData("verticalSync", value);
}

float DisplayConfigImpl::getScale() {
    return engineState().getScale();
}

float DisplayConfigImpl::getConfiguredScale() {
    return scale_;
}

void DisplayConfigImpl::setScale(float value) {
    applyScale(value);
    saveScale(scale_);
}

void DisplayConfigImpl::applyScale(float value) {
    scale_ = normalizeScale(value);
    if (!initialized_) {
        engineState().setScale(scale_ > 0.0f ? scale_ : 1.0f);
    }
    if (initialized_) {
        Display::onConfigurationChanged("scale");
    }
}

void DisplayConfigImpl::saveScale(float value) {
    ConfigStoreImpl::setIniData("scale", normalizeScale(value));
}

float DisplayConfigImpl::normalizeScale(float scale) {
    return std::isfinite(scale) && scale >= 0.0f ? scale : 1.0f;
}

int DisplayConfigImpl::normalizeAntiAliasingLevel(std::int64_t level) {
    if (level < 0 || level > std::numeric_limits<int>::max()) {
        return static_cast<int>(sf::ContextSettings{}.antiAliasingLevel);
    }
    return static_cast<int>(level);
}

}  // namespace ludork::global::system_impl
