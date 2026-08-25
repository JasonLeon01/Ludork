#include <System.hpp>

#include "Platform/NativeDisplay.hpp"
#include "Platform/NativeInputMethod.hpp"
#include "SystemRuntime.hpp"

#include <Fog/FogController.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Input/InputService.hpp>
#include <Manager/AudioManager.hpp>
#include <Manager/TimeManager.hpp>
#include <Runtime/EngineState.hpp>
#include <System/NativeDisplayHost.hpp>
#include <SystemConfigBase.hpp>
#include <Weather/WeatherController.hpp>

#include <algorithm>
#include <deque>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>

void System::init(const std::shared_ptr<ludork::standard::ConfigParser>& data,
                  const std::string& dataFilePath) {
    engineState().setGameRunning(true);
    SystemConfigBase::init(data, dataFilePath);
    SystemConfigBase::setChangeHandler(onConfigChanged);
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    display.pendingConfiguredScale_.reset();
    display.pendingResizeScale_.reset();
    display.surfaceFitScale_ = 1.0f;
    display.pendingRenderTargetRebuild_ = false;
    display.observedWindowSize_ = {};
    display.observedWindowClientSize_.reset();
    display.desktopFullscreen_ = false;
    display.inputMethodDisabled_ = true;
    display.canvasDefaultViewActive_ = true;
    framePipeline.graphicsCanvases_.clear();
    framePipeline.graphicsShaders_.clear();
    {
        const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
        sceneStack.scenes_.clear();
        sceneStack.retiredScenes_.clear();
    }
    {
        const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
        sceneStack.pendingSceneOperations_.clear();
        sceneStack.sceneOperationThread_ = {};
    }
    {
        const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
        framePipeline.pendingTransition_.reset();
    }
    framePipeline.transitionResource_.reset();
    framePipeline.transitionFrozen_ = false;
    framePipeline.transitionFreezePending_ = false;
    framePipeline.inTransition_ = false;
    framePipeline.transitionTimeCount_ = 0.0f;
    framePipeline.transitionTime_ = 0.0f;
    framePipeline.transitionRevision_ = 0;
    framePipeline.composedTransitionRevision_ = 0;
    framePipeline.transitionCompletionPending_ = false;
    stopFlash();
    stopScreenTone();
    stopShake();
    TimeManager::init();
    framePipeline.transitionShader_.reset();
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
    if (gameSize.x == 0 || gameSize.y == 0 || isEmbeddedDisplay()) {
        return std::nullopt;
    }
    std::optional<sf::Vector2u> maximumSize;
    if (isMobileDisplay()) {
        maximumSize =
            ludork::global::native_display_host::getMaximumWindowedSize();
    } else {
        ludork::global::system_runtime::DisplayRuntime& display =
            ludork::global::system_runtime::runtime().display;
        const std::lock_guard<std::mutex> lock(display.windowMutex_);
        const sf::WindowHandle windowHandle =
            display.window_ != nullptr && display.window_->isOpen()
                ? display.window_->getNativeHandle()
                : sf::WindowHandle{};
        maximumSize =
            ludork::global::getMaximumWindowedClientSize(windowHandle);
    }
    if (!maximumSize.has_value() || maximumSize->x == 0 ||
        maximumSize->y == 0) {
        return std::nullopt;
    }
    return std::min(
        static_cast<float>(maximumSize->x) / static_cast<float>(gameSize.x),
        static_cast<float>(maximumSize->y) / static_cast<float>(gameSize.y));
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
float System::getMaximumRenderScale() {
    return SystemConfigBase::getMaximumRenderScale();
}
void System::setMaximumRenderScale(float value) {
    SystemConfigBase::setMaximumRenderScale(value);
}
void System::saveMaximumRenderScale(float value) {
    SystemConfigBase::saveMaximumRenderScale(value);
}
float System::getLightingRenderScale() {
    return SystemConfigBase::getLightingRenderScale();
}
void System::setLightingRenderScale(float value) {
    SystemConfigBase::setLightingRenderScale(value);
}
void System::saveLightingRenderScale(float value) {
    SystemConfigBase::saveLightingRenderScale(value);
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
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        ludork::global::system_runtime::runtime().lifecycle;
    return lifecycle.debugMode_;
}
void System::setDebugMode(bool debugMode) {
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        ludork::global::system_runtime::runtime().lifecycle;
    lifecycle.debugMode_ = debugMode;
}

sf::Vector2u System::getGameSize() {
    return engineState().getGameSize();
}
void System::setGameSize(const sf::Vector2u& gameSize) {
    engineState().setGameSize(gameSize);
}

bool System::isActive() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    if (lifecycle.shuttingDown_.load() || !engineState().getGameRunning()) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(display.windowMutex_);
    return display.window_ != nullptr && display.window_->isOpen();
}

bool System::shouldLoop() {
    return isActive() && getScene() != nullptr;
}

void System::run() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    if (lifecycle.shuttingDown_.load()) {
        throw std::runtime_error(
            "Game loop cannot start during runtime shutdown");
    }
    if (display.window_ == nullptr) {
        throw std::runtime_error("Game loop cannot start without a window");
    }
    if (!display.window_->isOpen()) {
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
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        ludork::global::system_runtime::runtime().lifecycle;
    lifecycle.standardUpdate_ = std::move(update);
}

void System::updateRuntime() {
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        ludork::global::system_runtime::runtime().lifecycle;
    if (!lifecycle.shuttingDown_.load() && lifecycle.standardUpdate_) {
        try {
            lifecycle.standardUpdate_();
        } catch (...) {
            AudioManager::stopAll();
            throw;
        }
    }
}

void System::initializeRuntimeSession() noexcept {
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        ludork::global::system_runtime::runtime().lifecycle;
    const std::lock_guard<std::mutex> lifecycleLock(lifecycle.lifecycleMutex_);
    lifecycle.shuttingDown_.store(false);
}

void System::shutdownRuntime() noexcept {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    const std::lock_guard<std::mutex> lifecycleLock(lifecycle.lifecycleMutex_);
    lifecycle.shuttingDown_.store(true);
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
        const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
        sceneStack.pendingSceneOperations_.clear();
        sceneStack.sceneOperationThread_ = {};
    }
    {
        const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
        scenes.swap(sceneStack.scenes_);
        retiredScenes.swap(sceneStack.retiredScenes_);
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
        const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
        scenes.swap(sceneStack.scenes_);
        retiredScenes.swap(sceneStack.retiredScenes_);
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
        const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
        framePipeline.pendingTransition_.reset();
    }
    lifecycle.standardUpdate_ = {};
    framePipeline.graphicsShaders_.clear();
    framePipeline.graphicsCanvases_.clear();
    framePipeline.transitionResource_.reset();
    framePipeline.transitionShader_.reset();
    framePipeline.flashShader_.reset();
    framePipeline.toneShader_.reset();
    display.canvasSprite_.reset();
    framePipeline.transitionSprite_.reset();
    framePipeline.transitionOutputSprite_.reset();
    framePipeline.toneBufferSprite_.reset();
    framePipeline.transition_.reset();
    framePipeline.transitionTempTexture_.reset();
    framePipeline.transitionOutputTexture_.reset();
    framePipeline.transitionMaskTexture_.reset();
    framePipeline.toneBuffer_.reset();
    display.canvas_.reset();
    ludork::global::restoreNativeInputMethod();
    std::shared_ptr<sf::RenderWindow> previousWindow;
    {
        const std::lock_guard<std::mutex> lock(display.windowMutex_);
        previousWindow = std::move(display.window_);
    }
    previousWindow.reset();
    display.cursor_.reset();
    display.windowTitle_.clear();
    display.windowIconPath_.clear();
    display.windowCursorPath_.clear();
    display.windowContextSettings_ = {};
    display.observedWindowSize_ = {};
    display.observedWindowClientSize_.reset();
    display.pendingConfiguredScale_.reset();
    display.pendingResizeScale_.reset();
    display.surfaceFitScale_ = 1.0f;
    display.pendingRenderTargetRebuild_ = false;
    display.lastResizeTime_ = {};
    display.desktopFullscreen_ = false;
    display.inputMethodDisabled_ = true;
    display.canvasDefaultViewActive_ = true;
    framePipeline.inTransition_ = false;
    framePipeline.transitionTimeCount_ = 0.0f;
    framePipeline.transitionTime_ = 0.0f;
    framePipeline.transitionRevision_ = 0;
    framePipeline.composedTransitionRevision_ = 0;
    framePipeline.transitionCompletionPending_ = false;
    framePipeline.transitionFrozen_ = false;
    framePipeline.transitionFreezePending_ = false;
    framePipeline.flashActive_ = false;
    framePipeline.flashColour_ = {1.0f, 1.0f, 1.0f, 1.0f};
    framePipeline.flashDuration_ = 0.0f;
    framePipeline.flashTimeCount_ = 0.0f;
    framePipeline.toneActive_ = false;
    framePipeline.toneCurrentColour_ = {};
    framePipeline.toneStartColour_ = {};
    framePipeline.toneTargetColour_ = {};
    framePipeline.toneDuration_ = 0.0f;
    framePipeline.toneTimeCount_ = 0.0f;
    framePipeline.shakeActive_ = false;
    framePipeline.shakePower_ = 0.0f;
    framePipeline.shakeSpeed_ = 0.0f;
    framePipeline.shakeDuration_ = 0.0f;
    framePipeline.shakeTimeCount_ = 0.0f;
    framePipeline.shakeOffset_ = {};
    framePipeline.shakeNextUpdate_ = 0.0f;
    lifecycle.debugMode_ = false;
    engineState().setGameRunning(false);
}

void System::onConfigChanged(const std::string& key) {
    if (key == "scale") {
        if (getWindow() != nullptr) {
            ludork::global::system_runtime::DisplayRuntime& display =
                ludork::global::system_runtime::runtime().display;
            display.pendingConfiguredScale_ = getConfiguredScale();
        }
    } else if (key == "maximumRenderScale") {
        if (getWindow() != nullptr) {
            ludork::global::system_runtime::DisplayRuntime& display =
                ludork::global::system_runtime::runtime().display;
            display.pendingRenderTargetRebuild_ = true;
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
