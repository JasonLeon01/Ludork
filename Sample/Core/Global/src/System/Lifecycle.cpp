#include <System.hpp>

#include "Platform/NativeDisplay.hpp"
#include "Platform/NativeInputMethod.hpp"
#include "SystemRuntimeAccess.hpp"

#include <Fog/FogController.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Input/InputService.hpp>
#include <Manager/AudioManager.hpp>
#include <Manager/TimeManager.hpp>
#include <Runtime/EngineState.hpp>
#include <SystemConfigBase.hpp>
#include <Weather/WeatherController.hpp>

#include <algorithm>
#include <deque>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace ludork::global::system_runtime;

void System::init(const std::shared_ptr<ludork::standard::ConfigParser>& data,
                  const std::string& dataFilePath) {
    engineState().setGameRunning(true);
    SystemConfigBase::init(data, dataFilePath);
    SystemConfigBase::setChangeHandler(onConfigChanged);
    pendingConfiguredScale_.reset();
    pendingResizeScale_.reset();
    observedWindowSize_ = {};
    observedWindowClientSize_.reset();
    desktopFullscreen_ = false;
    inputMethodDisabled_ = true;
    canvasDefaultViewActive_ = true;
    graphicsCanvases_.clear();
    graphicsShaders_.clear();
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        scenes_.clear();
        retiredScenes_.clear();
    }
    {
        const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
        pendingSceneOperations_.clear();
        sceneOperationThread_ = {};
    }
    {
        const std::lock_guard<std::mutex> lock(transitionMutex_);
        pendingTransition_.reset();
    }
    transitionResource_.reset();
    transitionFrozen_ = false;
    transitionFreezePending_ = false;
    inTransition_ = false;
    transitionTimeCount_ = 0.0f;
    transitionTime_ = 0.0f;
    transitionRevision_ = 0;
    composedTransitionRevision_ = 0;
    transitionCompletionPending_ = false;
    stopFlash();
    stopScreenTone();
    stopShake();
    TimeManager::init();
    transitionShader_.reset();
}

std::string System::getScript() {
    return SystemConfigBase::getScript();
}
void System::setScript(const std::string& value) {
    SystemConfigBase::setScript(value);
}
void System::saveScript(const std::string& value) {
    SystemConfigBase::saveScript(value);
}
std::string System::getLanguage() {
    return SystemConfigBase::getLanguage();
}
void System::setLanguage(const std::string& value) {
    SystemConfigBase::setLanguage(value);
}
void System::saveLanguage(const std::string& value) {
    SystemConfigBase::saveLanguage(value);
}
float System::getScale() {
    return SystemConfigBase::getScale();
}
float System::getConfiguredScale() {
    return SystemConfigBase::getConfiguredScale();
}
std::optional<float> System::getMaximumWindowedScale(
    const sf::Vector2u& gameSize) {
    if (gameSize.x == 0 || gameSize.y == 0 || isEmbeddedDisplay() ||
        isMobileDisplay()) {
        return std::nullopt;
    }
    std::optional<sf::Vector2u> clientSize;
    {
        const std::lock_guard<std::mutex> lock(windowMutex_);
        const sf::WindowHandle windowHandle =
            window_ != nullptr && window_->isOpen() ? window_->getNativeHandle()
                                                    : sf::WindowHandle{};
        clientSize = ludork::global::getMaximumWindowedClientSize(windowHandle);
    }
    if (!clientSize.has_value() || clientSize->x == 0 || clientSize->y == 0) {
        return std::nullopt;
    }
    return std::min(
        static_cast<float>(clientSize->x) / static_cast<float>(gameSize.x),
        static_cast<float>(clientSize->y) / static_cast<float>(gameSize.y));
}
void System::setScale(float value) {
    SystemConfigBase::setScale(value);
}
void System::applyScale(float value) {
    SystemConfigBase::applyScale(value);
}
void System::saveScale(float value) {
    SystemConfigBase::saveScale(value);
}
int System::getFrameRate() {
    return SystemConfigBase::getFrameRate();
}
void System::setFrameRate(int value) {
    SystemConfigBase::setFrameRate(value);
}
void System::saveFrameRate(int value) {
    SystemConfigBase::saveFrameRate(value);
}
int System::getAntiAliasingLevel() {
    return SystemConfigBase::getAntiAliasingLevel();
}
void System::setAntiAliasingLevel(int value) {
    SystemConfigBase::setAntiAliasingLevel(value);
}
void System::saveAntiAliasingLevel(int value) {
    SystemConfigBase::saveAntiAliasingLevel(value);
}
bool System::getVerticalSync() {
    return SystemConfigBase::getVerticalSync();
}
void System::setVerticalSync(bool value) {
    SystemConfigBase::setVerticalSync(value);
}
void System::saveVerticalSync(bool value) {
    SystemConfigBase::saveVerticalSync(value);
}
bool System::getMusicOn() {
    return SystemConfigBase::getMusicOn();
}
void System::setMusicOn(bool value) {
    SystemConfigBase::setMusicOn(value);
}
void System::saveMusicOn(bool value) {
    SystemConfigBase::saveMusicOn(value);
}
bool System::getSoundOn() {
    return SystemConfigBase::getSoundOn();
}
void System::setSoundOn(bool value) {
    SystemConfigBase::setSoundOn(value);
}
void System::saveSoundOn(bool value) {
    SystemConfigBase::saveSoundOn(value);
}
bool System::getVoiceOn() {
    return SystemConfigBase::getVoiceOn();
}
void System::setVoiceOn(bool value) {
    SystemConfigBase::setVoiceOn(value);
}
void System::saveVoiceOn(bool value) {
    SystemConfigBase::saveVoiceOn(value);
}
float System::getMusicVolume() {
    return SystemConfigBase::getMusicVolume();
}
void System::setMusicVolume(float value) {
    SystemConfigBase::setMusicVolume(value);
}
void System::saveMusicVolume(float value) {
    SystemConfigBase::saveMusicVolume(value);
}
float System::getSoundVolume() {
    return SystemConfigBase::getSoundVolume();
}
void System::setSoundVolume(float value) {
    SystemConfigBase::setSoundVolume(value);
}
void System::saveSoundVolume(float value) {
    SystemConfigBase::saveSoundVolume(value);
}
float System::getVoiceVolume() {
    return SystemConfigBase::getVoiceVolume();
}
void System::setVoiceVolume(float value) {
    SystemConfigBase::setVoiceVolume(value);
}
void System::saveVoiceVolume(float value) {
    SystemConfigBase::saveVoiceVolume(value);
}

bool System::isDebugMode() {
    return debugMode_;
}
void System::setDebugMode(bool debugMode) {
    debugMode_ = debugMode;
}

sf::Vector2u System::getGameSize() {
    return engineState().getGameSize();
}
void System::setGameSize(const sf::Vector2u& gameSize) {
    engineState().setGameSize(gameSize);
}

bool System::isActive() {
    if (shuttingDown_.load() || !engineState().getGameRunning()) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(windowMutex_);
    return window_ != nullptr && window_->isOpen();
}

bool System::shouldLoop() {
    return isActive() && getScene() != nullptr;
}

void System::run() {
    if (shuttingDown_.load()) {
        throw std::runtime_error(
            "Game loop cannot start during runtime shutdown");
    }
    if (window_ == nullptr) {
        throw std::runtime_error("Game loop cannot start without a window");
    }
    if (!window_->isOpen()) {
        throw std::runtime_error(
            "Game loop cannot start because the window is closed");
    }
    if (!engineState().getGameRunning()) {
        throw std::runtime_error(
            "Game loop cannot start while the game is stopped");
    }
    if (getScene() == nullptr) {
        throw std::runtime_error("Game loop cannot start without a scene");
    }
    bindSceneOperationThread();
    try {
        while (shouldLoop()) {
            applyPendingSceneReplace();
            const std::shared_ptr<SceneRuntime> currentScene = getScene();
            if (currentScene != nullptr) {
                currentScene->systemMain();
            }
        }
    } catch (...) {
        unbindSceneOperationThread();
        throw;
    }
    unbindSceneOperationThread();
}

void System::setStandardUpdate(std::function<void()> update) {
    standardUpdate_ = std::move(update);
}

void System::updateRuntime() {
    if (!shuttingDown_.load() && standardUpdate_) {
        standardUpdate_();
    }
}

void System::initializeRuntimeSession() noexcept {
    const std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
    shuttingDown_.store(false);
}

void System::shutdownRuntime() noexcept {
    const std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
    shuttingDown_.store(true);
    std::vector<std::shared_ptr<SceneRuntime>> scenes;
    std::deque<std::shared_ptr<SceneRuntime>> retiredScenes;
    const auto shutdownScene = [](const auto& scene) noexcept {
        if (scene == nullptr) {
            return;
        }
        try {
            scene->systemDestroy();
        } catch (const std::exception& error) {
            std::cerr << "Scene shutdown callback failed: " << error.what()
                      << '\n';
        } catch (...) {
            std::cerr
                << "Scene shutdown callback failed with an unknown error\n";
        }
        scene->systemShutdown();
    };
    {
        const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
        pendingSceneOperations_.clear();
        sceneOperationThread_ = {};
    }
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        scenes.swap(scenes_);
        retiredScenes.swap(retiredScenes_);
    }
    for (const std::shared_ptr<SceneRuntime>& scene : retiredScenes) {
        shutdownScene(scene);
    }
    retiredScenes.clear();
    for (auto iterator = scenes.rbegin(); iterator != scenes.rend();
         ++iterator) {
        shutdownScene(*iterator);
    }
    scenes.clear();
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        scenes.swap(scenes_);
        retiredScenes.swap(retiredScenes_);
    }
    for (const std::shared_ptr<SceneRuntime>& scene : retiredScenes) {
        shutdownScene(scene);
    }
    retiredScenes.clear();
    for (auto iterator = scenes.rbegin(); iterator != scenes.rend();
         ++iterator) {
        shutdownScene(*iterator);
    }
    scenes.clear();
    {
        const std::lock_guard<std::mutex> lock(transitionMutex_);
        pendingTransition_.reset();
    }
    standardUpdate_ = {};
    graphicsShaders_.clear();
    graphicsCanvases_.clear();
    transitionResource_.reset();
    transitionShader_.reset();
    flashShader_.reset();
    toneShader_.reset();
    canvasSprite_.reset();
    transitionSprite_.reset();
    transitionOutputSprite_.reset();
    toneBufferSprite_.reset();
    transition_.reset();
    transitionTempTexture_.reset();
    transitionOutputTexture_.reset();
    transitionMaskTexture_.reset();
    toneBuffer_.reset();
    canvas_.reset();
    ludork::global::restoreNativeInputMethod();
    std::shared_ptr<sf::RenderWindow> previousWindow;
    {
        const std::lock_guard<std::mutex> lock(windowMutex_);
        previousWindow = std::move(window_);
    }
    previousWindow.reset();
    cursor_.reset();
    windowTitle_.clear();
    windowIconPath_.clear();
    windowCursorPath_.clear();
    windowContextSettings_ = {};
    observedWindowSize_ = {};
    observedWindowClientSize_.reset();
    pendingConfiguredScale_.reset();
    pendingResizeScale_.reset();
    lastResizeTime_ = {};
    desktopFullscreen_ = false;
    inputMethodDisabled_ = true;
    canvasDefaultViewActive_ = true;
    inTransition_ = false;
    transitionTimeCount_ = 0.0f;
    transitionTime_ = 0.0f;
    transitionRevision_ = 0;
    composedTransitionRevision_ = 0;
    transitionCompletionPending_ = false;
    transitionFrozen_ = false;
    transitionFreezePending_ = false;
    flashActive_ = false;
    flashColour_ = {1.0f, 1.0f, 1.0f, 1.0f};
    flashDuration_ = 0.0f;
    flashTimeCount_ = 0.0f;
    toneActive_ = false;
    toneCurrentColour_ = {};
    toneStartColour_ = {};
    toneTargetColour_ = {};
    toneDuration_ = 0.0f;
    toneTimeCount_ = 0.0f;
    shakeActive_ = false;
    shakePower_ = 0.0f;
    shakeSpeed_ = 0.0f;
    shakeDuration_ = 0.0f;
    shakeTimeCount_ = 0.0f;
    shakeOffset_ = {};
    shakeNextUpdate_ = 0.0f;
    debugMode_ = false;
    engineState().setGameRunning(false);
}

void System::onConfigChanged(const std::string& key) {
    if (key == "scale") {
        if (getWindow() != nullptr) {
            pendingConfiguredScale_ = getConfiguredScale();
        }
    } else if (key == "frameRate") {
        const std::shared_ptr<sf::RenderWindow> window = getWindow();
        if (window != nullptr) {
            window->setFramerateLimit(
                static_cast<unsigned int>(std::max(0, getFrameRate())));
        }
    } else if (key == "verticalSync") {
        const std::shared_ptr<sf::RenderWindow> window = getWindow();
        if (window != nullptr) {
            window->setVerticalSyncEnabled(getVerticalSync());
        }
    } else if (key == "musicOn" || key == "musicVolume") {
        AudioManager::applyMusicVolumes();
    } else if (key == "soundOn") {
        if (getSoundOn()) {
            AudioManager::applySoundVolumes();
        } else {
            AudioManager::stopSound();
        }
    } else if (key == "soundVolume") {
        AudioManager::applySoundVolumes();
    } else if (key == "voiceOn") {
        if (getVoiceOn()) {
            AudioManager::applyVoiceVolumes();
        } else {
            AudioManager::stopVoice();
        }
    } else if (key == "voiceVolume") {
        AudioManager::applyVoiceVolumes();
    }
}
