#include <System.hpp>

using ludork::global::system_runtime::PendingSceneOperation;
using ludork::global::system_runtime::PendingTransition;
using ludork::global::system_runtime::SceneOperationType;

#include "System/Platform/NativeDisplay.hpp"
#include "System/Platform/NativeInputMethod.hpp"
#include "System/Diagnostics/PerformanceProfiler.hpp"
#include "System/DisplayRuntime.hpp"
#include "System/ScreenEffectsRuntime.hpp"
#include "System/SystemRuntime.hpp"
#include "System/TransitionRuntime.hpp"

#include <Fog/FogController.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Input/InputService.hpp>
#include <LudorkPlatform.hpp>
#include <Manager/AudioManager.hpp>
#include <Manager/ShaderManager.hpp>
#include <Manager/TextureManager.hpp>
#include <Runtime/AssetStore.hpp>
#include <Manager/TimeManager.hpp>
#include <EngineState.hpp>
#include <System/NativeDisplayHost.hpp>
#include <SystemConfigBase.hpp>
#include <Utils/Inner.hpp>
#include <Utils/Render.hpp>
#include <Weather/WeatherController.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <deque>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <thread>
#include <type_traits>
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
void System::initializeDisplay(const std::string& title,
                               const sf::Vector2u& gameSize,
                               const std::string& iconPath,
                               const std::string& cursorPath) {
    if (gameSize.x == 0 || gameSize.y == 0) {
        throw std::invalid_argument("Game size must be non-zero");
    }
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    if (display.window_ != nullptr) {
        throw std::logic_error("Display has already been initialized");
    }
    const ludork::global::RuntimeLaunchOptions& launchOptions =
        ludork::global::runtimeLaunchOptions();
    display.windowTitle_ = title;
    display.windowIconPath_ = iconPath;
    display.windowCursorPath_ = cursorPath;
    display.windowContextSettings_ = {};
    display.windowContextSettings_.antiAliasingLevel =
        static_cast<unsigned int>(getAntiAliasingLevel());
#if defined(SFML_SYSTEM_IOS)
    display.windowContextSettings_.majorVersion = 3;
    display.windowContextSettings_.minorVersion = 0;
#endif
    std::shared_ptr<sf::RenderWindow> window;
    float surfaceFitScale = 1.0f;

    setGameSize(gameSize);
    setDebugMode(launchOptions.editor);
    inputService().setUseInjectedMouseOnly(false);

    if (isEmbeddedDisplay()) {
#if defined(_WIN32)
        if (!launchOptions.hostWindowHandle.has_value()) {
            throw std::invalid_argument("Embedded window handle is required");
        }
        window = std::make_shared<sf::RenderWindow>(
            reinterpret_cast<sf::WindowHandle>(
                launchOptions.hostWindowHandle.value()),
            display.windowContextSettings_);
        surfaceFitScale = windowFitScale(window->getSize());
        inputService().setUseInjectedMouseOnly(true);
#else
        throw std::runtime_error(
            "Embedded window mode is only supported on Windows");
#endif
    } else if (isMobileDisplay()) {
        window = std::make_shared<sf::RenderWindow>(
            sf::VideoMode::getDesktopMode(), title, sf::Style::Default,
            sf::State::Fullscreen, display.windowContextSettings_);
        surfaceFitScale = windowFitScale(window->getSize());
    } else {
        const float configuredScale = getConfiguredScale();
        display.desktopFullscreen_ = configuredScale == 0.0f;
        const sf::Vector2u windowSize =
            display.desktopFullscreen_ ? sf::VideoMode::getDesktopMode().size
                                       : windowSizeForScale(configuredScale);
        window = std::make_shared<sf::RenderWindow>(
            sf::VideoMode(windowSize), title,
            display.desktopFullscreen_ ? sf::Style::None : sf::Style::Default,
            sf::State::Windowed, display.windowContextSettings_);
        const std::optional<sf::Vector2u> clientSize =
            display.desktopFullscreen_ ? std::nullopt
                                       : ludork::global::getWindowedClientSize(
                                             window->getNativeHandle());
        surfaceFitScale =
            windowFitScale(clientSize.value_or(window->getSize()));
    }

    display.surfaceFitScale_ = surfaceFitScale;
    engineState().setScale(effectiveRenderScale(surfaceFitScale));
#if defined(SFML_SYSTEM_IOS)
    if (window->getSettings().majorVersion < 3) {
        throw std::runtime_error(
            "iOS requires an OpenGL ES 3.0 context, but OpenGL ES " +
            std::to_string(window->getSettings().majorVersion) + "." +
            std::to_string(window->getSettings().minorVersion) +
            " was created");
    }
#endif
    initWindow(window);
    if (shadersAvailable()) {
        framePipeline.transitionShader_ =
            ShaderManager::load("/Game/Assets/Shaders/Global/Transition.frag",
                                sf::Shader::Type::Fragment);
    } else {
        framePipeline.transitionShader_.reset();
        warnOnce("System.transitionShader",
                 "Shaders are unavailable; skipped loading transition shader");
    }
    setInputMethodDisabled(true);
    inputService().initializeNativePolling();
    initCanvas(renderSizeForScale(getScale()));
    display.observedWindowSize_ = display.window_->getSize();
    display.observedWindowClientSize_ =
        display.desktopFullscreen_ ? std::nullopt
                                   : ludork::global::getWindowedClientSize(
                                         display.window_->getNativeHandle());
    updateWindowViewport();
    if (isMobileDisplay() && isDisplayScaleConfigurable()) {
        ludork::global::native_display_host::requestDisplayScale(
            getConfiguredScale(), gameSize);
    }
}

void System::initWindow(const std::shared_ptr<sf::RenderWindow>& window) {
    if (window == nullptr) {
        throw std::invalid_argument("System window cannot be nil");
    }
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    {
        const std::lock_guard<std::mutex> lock(display.windowMutex_);
        display.window_ = window;
    }
    applyWindowPresentationSettings();
}

std::shared_ptr<sf::RenderWindow> System::getWindow() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    const std::lock_guard<std::mutex> lock(display.windowMutex_);
    return display.window_;
}

bool System::isEmbeddedDisplay() {
    return ludork::global::runtimeLaunchOptions().windowMode ==
           ludork::global::RuntimeWindowMode::Embedded;
}

bool System::isMobileDisplay() {
#if defined(LUDORK_MOBILE)
    return true;
#else
    return false;
#endif
}

bool System::isDisplayScaleConfigurable() {
    if (isEmbeddedDisplay()) {
        return false;
    }
    if (!isMobileDisplay()) {
        return true;
    }
    return ludork::global::native_display_host::isDisplayScaleConfigurable();
}

float System::windowFitScale(const sf::Vector2u& size) {
    return ludork::global::system_display_impl::windowFitScale(size,
                                                               getGameSize());
}

float System::effectiveRenderScale(float surfaceFitScale) {
    return ludork::global::system_display_impl::effectiveRenderScale(
        surfaceFitScale, getMaximumRenderScale());
}

sf::Vector2u System::windowSizeForScale(float scale) {
    return ludork::global::system_display_impl::scaledSize(getGameSize(),
                                                           scale);
}

sf::Vector2u System::renderSizeForScale(float scale) {
    return windowSizeForScale(scale);
}

void System::applyWindowPresentationSettings() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.window_ == nullptr) {
        return;
    }
    display.window_->setFramerateLimit(
        static_cast<unsigned int>(std::max(0, getFrameRate())));
    display.window_->setVerticalSyncEnabled(getVerticalSync());
    display.window_->clear(isEmbeddedDisplay() ? sf::Color::Transparent
                                               : sf::Color::Black);
    if (!isMobileDisplay() && !display.windowIconPath_.empty()) {
        std::unique_ptr<ludork::runtime::AssetInputStream> iconStream =
            ludork::runtime::assetStore().open(display.windowIconPath_);
        sf::Image icon;
        if (!icon.loadFromStream(*iconStream)) {
            throw std::runtime_error("Failed to load window icon: " +
                                     display.windowIconPath_);
        }
        display.window_->setIcon(icon);
    }
    display.cursor_.reset();
    if (isMobileDisplay() || display.windowCursorPath_.empty()) {
        return;
    }
    if (!ludork::runtime::assetStore().exists(display.windowCursorPath_)) {
        return;
    }
    try {
        std::unique_ptr<ludork::runtime::AssetInputStream> cursorStream =
            ludork::runtime::assetStore().open(display.windowCursorPath_);
        sf::Image cursorImage;
        if (!cursorImage.loadFromStream(*cursorStream)) {
            throw std::runtime_error("Failed to load cursor image");
        }
        display.cursor_ = std::make_unique<sf::Cursor>(
            cursorImage.getPixelsPtr(), cursorImage.getSize(), sf::Vector2u{});
        display.window_->setMouseCursor(*display.cursor_);
    } catch (const std::exception& exception) {
        std::cerr << "Failed to create cursor from "
                  << display.windowCursorPath_ << ": " << exception.what()
                  << '\n';
    }
}

void System::recreateDesktopWindow(bool fullscreen, const sf::Vector2u& size) {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    const std::lock_guard<std::mutex> lock(display.windowMutex_);
    if (display.window_ == nullptr || isEmbeddedDisplay() ||
        isMobileDisplay()) {
        return;
    }
    ludork::global::restoreNativeInputMethod();
    display.window_->create(sf::VideoMode(size), display.windowTitle_,
                            fullscreen ? sf::Style::None : sf::Style::Default,
                            sf::State::Windowed,
                            display.windowContextSettings_);
    display.desktopFullscreen_ = fullscreen;
    if (fullscreen) {
        display.window_->setPosition({0, 0});
    }
    applyWindowPresentationSettings();
    setInputMethodDisabled(display.inputMethodDisabled_);
    inputService().onWindowRecreated(*display.window_);
    inputService().initializeNativePolling();
}

void System::replaceWindowedDesktopWindow(
    const sf::Vector2u& size,
    const ludork::global::WindowedFramePlacement* placement) {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    const std::shared_ptr<sf::RenderWindow> previousWindow = getWindow();
    if (previousWindow == nullptr || isEmbeddedDisplay() || isMobileDisplay()) {
        return;
    }
    ludork::global::restoreNativeInputMethod();
    const std::shared_ptr<sf::RenderWindow> replacement =
        std::make_shared<sf::RenderWindow>(
            sf::VideoMode(size), display.windowTitle_, sf::Style::Default,
            sf::State::Windowed, display.windowContextSettings_);
    {
        const std::lock_guard<std::mutex> lock(display.windowMutex_);
        display.window_ = replacement;
        display.desktopFullscreen_ = false;
        if (placement != nullptr) {
            ludork::global::setWindowedFramePlacement(
                display.window_->getNativeHandle(), *placement);
        }
        applyWindowPresentationSettings();
        setInputMethodDisabled(display.inputMethodDisabled_);
        inputService().onWindowRecreated(*display.window_);
        inputService().initializeNativePolling();
    }
}

void System::updateWindowViewport() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.window_ == nullptr || display.canvas_ == nullptr) {
        return;
    }
    const sf::Vector2u windowSize = display.window_->getSize();
    const sf::Vector2u renderSize = display.canvas_->getSize();
    if (windowSize.x == 0 || windowSize.y == 0 || renderSize.x == 0 ||
        renderSize.y == 0) {
        return;
    }
    const float fit = std::min(
        static_cast<float>(windowSize.x) / static_cast<float>(renderSize.x),
        static_cast<float>(windowSize.y) / static_cast<float>(renderSize.y));
    const sf::Vector2f contentSize{
        static_cast<float>(renderSize.x) * fit,
        static_cast<float>(renderSize.y) * fit,
    };
    const sf::Vector2f windowSizeFloat{static_cast<float>(windowSize.x),
                                       static_cast<float>(windowSize.y)};
    const sf::Vector2f offset = (windowSizeFloat - contentSize) / 2.0f;
    sf::View view(sf::Vector2f(renderSize) / 2.0f, sf::Vector2f(renderSize));
    view.setViewport(sf::FloatRect(
        {offset.x / windowSizeFloat.x, offset.y / windowSizeFloat.y},
        {contentSize.x / windowSizeFloat.x,
         contentSize.y / windowSizeFloat.y}));
    display.window_->setView(view);
    const sf::Vector2i viewportPosition{
        static_cast<int>(std::ceil(offset.x)),
        static_cast<int>(std::ceil(offset.y)),
    };
    const sf::Vector2i viewportSize{
        std::max(0, static_cast<int>(std::floor(contentSize.x))),
        std::max(0, static_cast<int>(std::floor(contentSize.y))),
    };
    inputService().setPointerViewport(
        sf::IntRect(viewportPosition, viewportSize));
}

void System::rebuildDisplayTargets(float surfaceFitScale) {
    const float normalizedSurfaceFitScale = std::max(0.01f, surfaceFitScale);
    const float renderScale = effectiveRenderScale(normalizedSurfaceFitScale);
    const sf::Vector2u size = renderSizeForScale(renderScale);
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    display.surfaceFitScale_ = normalizedSurfaceFitScale;
    if (display.canvas_ != nullptr && display.canvas_->getSize() == size &&
        engineState().getScale() == renderScale) {
        updateWindowViewport();
        return;
    }
    std::optional<sf::Image> transitionImage;
    if (framePipeline.transition_ != nullptr) {
        framePipeline.transition_->display();
        transitionImage = framePipeline.transition_->getTexture().copyToImage();
    }
    engineState().setScale(renderScale);
    initCanvas(size);
    if (transitionImage.has_value() && framePipeline.transition_ != nullptr) {
        const sf::Texture texture(*transitionImage);
        sf::Sprite sprite(texture);
        const sf::Vector2u sourceSize = texture.getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0) {
            sprite.setScale(
                {static_cast<float>(size.x) / static_cast<float>(sourceSize.x),
                 static_cast<float>(size.y) /
                     static_cast<float>(sourceSize.y)});
            framePipeline.transition_->clear(sf::Color::Transparent);
            framePipeline.transition_->draw(sprite, sf::BlendNone);
            framePipeline.transition_->display();
        }
    }
    if (framePipeline.transitionResource_ != nullptr &&
        framePipeline.transitionMaskTexture_ != nullptr) {
        const sf::Vector2u sourceSize =
            framePipeline.transitionResource_->getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0) {
            sf::Sprite maskSprite(*framePipeline.transitionResource_);
            maskSprite.setScale(
                {static_cast<float>(size.x) / static_cast<float>(sourceSize.x),
                 static_cast<float>(size.y) /
                     static_cast<float>(sourceSize.y)});
            framePipeline.transitionMaskTexture_->clear(sf::Color::Transparent);
            framePipeline.transitionMaskTexture_->draw(maskSprite,
                                                       sf::BlendNone);
            framePipeline.transitionMaskTexture_->display();
        }
    }
    updateWindowViewport();
}

void System::applyConfiguredScale(float scale) {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.window_ == nullptr || isEmbeddedDisplay()) {
        return;
    }
    if (isMobileDisplay()) {
        if (isDisplayScaleConfigurable()) {
            ludork::global::native_display_host::requestDisplayScale(
                scale, getGameSize());
        }
        return;
    }
    const bool fullscreen = scale == 0.0f;
    sf::Vector2u targetSize = fullscreen ? sf::VideoMode::getDesktopMode().size
                                         : windowSizeForScale(scale);
    std::optional<sf::Vector2u> clientSize;
    if (fullscreen != display.desktopFullscreen_) {
        recreateDesktopWindow(fullscreen, targetSize);
        if (!fullscreen) {
            clientSize = ludork::global::getWindowedClientSize(
                display.window_->getNativeHandle());
        }
    } else if (!fullscreen) {
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
        const std::optional<ludork::global::WindowedFramePlacement> placement =
            ludork::global::getWindowedFramePlacement(
                display.window_->getNativeHandle());
        replaceWindowedDesktopWindow(
            targetSize, placement.has_value() ? &*placement : nullptr);
#else
        display.window_->setSize(targetSize);
#endif
        clientSize = ludork::global::getWindowedClientSize(
            display.window_->getNativeHandle());
    }
    display.observedWindowSize_ = display.window_->getSize();
    display.observedWindowClientSize_ = clientSize;
    display.pendingResizeScale_.reset();
    rebuildDisplayTargets(
        windowFitScale(clientSize.value_or(display.observedWindowSize_)));
}

void System::observeWindowResize() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.window_ == nullptr) {
        return;
    }
    const sf::Vector2u size = display.window_->getSize();
    const auto now = std::chrono::steady_clock::now();
    if (size != display.observedWindowSize_) {
        const std::optional<sf::Vector2u> clientSize =
            display.desktopFullscreen_
                ? std::nullopt
                : ludork::global::getWindowedClientSize(
                      display.window_->getNativeHandle());
        const bool clientSizeChanged =
            !clientSize.has_value() ||
            clientSize != display.observedWindowClientSize_;
        display.observedWindowSize_ = size;
        display.observedWindowClientSize_ = clientSize;
        if (clientSizeChanged) {
            display.pendingResizeScale_ = windowFitScale(
                clientSize.value_or(display.observedWindowSize_));
            display.lastResizeTime_ = now;
        }
        updateWindowViewport();
    }
    if (!display.pendingResizeScale_.has_value() ||
        now - display.lastResizeTime_ < std::chrono::milliseconds(150)) {
        return;
    }
    float scale = *display.pendingResizeScale_;
    display.pendingResizeScale_.reset();
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
    if (!isEmbeddedDisplay() && !display.desktopFullscreen_) {
        const std::optional<sf::Vector2u> clientSize =
            ludork::global::getWindowedClientSize(
                display.window_->getNativeHandle());
        if (clientSize.has_value()) {
            const std::optional<ludork::global::WindowedFramePlacement>
                placement = ludork::global::getWindowedFramePlacement(
                    display.window_->getNativeHandle());
            replaceWindowedDesktopWindow(
                *clientSize, placement.has_value() ? &*placement : nullptr);
            display.observedWindowSize_ = display.window_->getSize();
            const std::optional<sf::Vector2u> replacedClientSize =
                ludork::global::getWindowedClientSize(
                    display.window_->getNativeHandle());
            display.observedWindowClientSize_ = replacedClientSize;
            scale = windowFitScale(replacedClientSize.value_or(*clientSize));
        }
    }
#endif
    rebuildDisplayTargets(scale);
}

void System::applyPendingDisplayChanges() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.pendingConfiguredScale_.has_value()) {
        const float scale = *display.pendingConfiguredScale_;
        display.pendingConfiguredScale_.reset();
        applyConfiguredScale(scale);
    }
    if (display.pendingRenderTargetRebuild_) {
        display.pendingRenderTargetRebuild_ = false;
        rebuildDisplayTargets(display.surfaceFitScale_);
    }
    observeWindowResize();
}

void System::setInputMethodDisabled(bool disabled) {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    display.inputMethodDisabled_ = disabled;
    if (display.window_ == nullptr ||
        ludork::global::runtimeLaunchOptions().windowMode ==
            ludork::global::RuntimeWindowMode::Embedded) {
        return;
    }
    ludork::global::setNativeInputMethodDisabled(
        display.window_->getNativeHandle(), disabled);
}

void System::initCanvas(const sf::Vector2u& size) {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    std::optional<sf::View> preservedView;
    if (display.canvas_ == nullptr) {
        display.canvas_ = std::make_unique<sf::RenderTexture>(size);
    } else {
        const sf::View currentView = display.canvas_->getView();
        if (!display.canvasDefaultViewActive_ ||
            !ludork::global::system_display_impl::viewsEqual(
                currentView, display.canvas_->getDefaultView())) {
            preservedView = currentView;
        }
        if (display.canvas_->getSize() != size &&
            !display.canvas_->resize(size)) {
            throw std::runtime_error("Failed to resize the System canvas");
        }
    }
    display.canvas_->setView(display.canvas_->getDefaultView());
    display.canvas_->clear(sf::Color::Transparent);
    display.canvas_->setView(
        preservedView.value_or(display.canvas_->getDefaultView()));
    if (display.canvasSprite_.has_value()) {
        display.canvasSprite_->setTexture(display.canvas_->getTexture(), true);
    } else {
        display.canvasSprite_.emplace(display.canvas_->getTexture());
    }
    framePipeline.transition_ = std::make_unique<sf::RenderTexture>(size);
    framePipeline.transition_->clear(sf::Color::Transparent);
    framePipeline.transition_->display();
    framePipeline.transitionTempTexture_ =
        std::make_unique<sf::RenderTexture>(size);
    framePipeline.transitionTempTexture_->clear(sf::Color::Transparent);
    framePipeline.transitionOutputTexture_ =
        std::make_unique<sf::RenderTexture>(size);
    framePipeline.transitionOutputTexture_->clear(sf::Color::Transparent);
    framePipeline.transitionOutputTexture_->display();
    framePipeline.transitionMaskTexture_ =
        std::make_unique<sf::RenderTexture>(size);
    framePipeline.transitionMaskTexture_->clear(sf::Color::Transparent);
    framePipeline.transitionMaskTexture_->display();
    framePipeline.transitionSprite_.emplace(
        framePipeline.transitionTempTexture_->getTexture());
    framePipeline.transitionOutputSprite_.emplace(
        framePipeline.transitionOutputTexture_->getTexture());
    framePipeline.toneBuffer_.reset();
    framePipeline.toneBufferSprite_.reset();
    applyGraphicsShadersLength();
}

void System::clearCanvas() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.window_ != nullptr) {
        display.window_->clear(isEmbeddedDisplay() ? sf::Color::Transparent
                                                   : sf::Color::Black);
    }
    if (display.canvas_ != nullptr) {
        display.canvas_->clear(sf::Color::Transparent);
    }
}

void System::setWindowMapView(const sf::IntRect& rect) {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.canvas_ == nullptr) {
        return;
    }
    const sf::Vector2u gameSize = getGameSize();
    const sf::Vector2f gameSizeFloat{static_cast<float>(gameSize.x),
                                     static_cast<float>(gameSize.y)};
    const sf::Vector2f position{static_cast<float>(rect.position.x),
                                static_cast<float>(rect.position.y)};
    const sf::Vector2f size{static_cast<float>(rect.size.x),
                            static_cast<float>(rect.size.y)};
    sf::View view(size / 2.0f, size);
    view.setViewport(sf::FloatRect(position.componentWiseDiv(gameSizeFloat),
                                   size.componentWiseDiv(gameSizeFloat)));
    display.canvas_->setView(view);
    display.canvasDefaultViewActive_ = false;
}

void System::setWindowDefaultView() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.canvas_ != nullptr) {
        display.canvas_->setView(display.canvas_->getDefaultView());
        display.canvasDefaultViewActive_ = true;
    }
}

sf::RenderTexture* System::getCanvas() {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    return display.canvas_.get();
}
void System::setWeather(WeatherType weatherType, float power, int maxCount) {
    WeatherController::setWeather(weatherType, power, maxCount);
}

void System::clearWeather() {
    WeatherController::clearWeather();
}

void System::updateWeather(float deltaTime) {
    WeatherController::update(deltaTime);
}

void System::updateFog(float deltaTime) {
    FogController::update(deltaTime);
}

void System::clearFog() {
    FogController::clearFog();
}

void System::applyFogFromMapData(const RuntimeValue::Map& mapData) {
    FogController::applyFromMapData(mapData);
}

void System::draw(const sf::Drawable& drawable, sf::Shader* shader) {
    ludork::global::system_runtime::DisplayRuntime& display =
        ludork::global::system_runtime::runtime().display;
    if (display.canvas_ == nullptr) {
        return;
    }
    sf::RenderStates states = canvasRenderStates();
    states.shader = shader;
    display.canvas_->draw(drawable, states);
}

void System::applyScreenTonePass() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    if (!shadersAvailable() || !framePipeline.toneActive_ ||
        framePipeline.toneShader_ == nullptr || display.canvas_ == nullptr ||
        isNeutralTone(framePipeline.toneCurrentColour_)) {
        return;
    }
    display.canvas_->display();
    const sf::Vector2u size = display.canvas_->getSize();
    ensureToneBuffer(size);
    framePipeline.toneBufferSprite_->setTexture(display.canvas_->getTexture(),
                                                true);
    framePipeline.toneBufferSprite_->setPosition({0.0f, 0.0f});
    framePipeline.toneBufferSprite_->setScale({1.0f, 1.0f});
    framePipeline.toneShader_->setUniform("screenTex",
                                          display.canvas_->getTexture());
    framePipeline.toneShader_->setUniform(
        "texSize",
        sf::Vector2f{static_cast<float>(size.x), static_cast<float>(size.y)});
    applyScreenToneUniform();
    framePipeline.toneBuffer_->clear(sf::Color::Transparent);
    sf::RenderStates toneStates = canvasRenderStates();
    toneStates.shader = framePipeline.toneShader_.get();
    framePipeline.toneBuffer_->draw(*framePipeline.toneBufferSprite_,
                                    toneStates);
    framePipeline.toneBuffer_->display();
    const sf::View savedView = display.canvas_->getView();
    display.canvas_->clear(sf::Color::Transparent);
    display.canvas_->setView(display.canvas_->getDefaultView());
    framePipeline.toneBufferSprite_->setTexture(
        framePipeline.toneBuffer_->getTexture(), true);
    display.canvas_->draw(*framePipeline.toneBufferSprite_,
                          canvasRenderStates());
    display.canvas_->setView(savedView);
}

void System::composeFrame(float deltaTime) {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    framePipeline.transitionCompletionPending_ = false;
    if (display.window_ == nullptr || display.canvas_ == nullptr ||
        !display.canvasSprite_.has_value()) {
        return;
    }
    if (framePipeline.inTransition_) {
        framePipeline.transitionTimeCount_ =
            ludork::global::system_transition_impl::advanceElapsed(
                framePipeline.transitionTimeCount_,
                framePipeline.transitionTime_, deltaTime);
    }
    updateFlash(deltaTime);
    updateScreenTone(deltaTime);
    updateShake(deltaTime);
    updateWeather(deltaTime);
    updateFog(deltaTime);
    applyPendingTransition();
    display.canvas_->display();
    sf::RenderTexture* finalCanvas = display.canvas_.get();
    for (std::size_t index = 0; index < framePipeline.graphicsCanvases_.size();
         ++index) {
        sf::RenderTexture& target = *framePipeline.graphicsCanvases_[index];
        sf::RenderTexture& source =
            index == 0 ? *display.canvas_
                       : *framePipeline.graphicsCanvases_[index - 1];
        target.clear(sf::Color::Transparent);
        sf::Sprite sprite(source.getTexture());
        sf::RenderStates states = canvasRenderStates();
        const std::shared_ptr<sf::Shader>& shader =
            framePipeline.graphicsShaders_[index];
        if (shader != nullptr) {
            shader->setUniform("screenTex", source.getTexture());
            const sf::Vector2u textureSize = source.getTexture().getSize();
            shader->setUniform("texSize",
                               sf::Vector2f{static_cast<float>(textureSize.x),
                                            static_cast<float>(textureSize.y)});
            states.shader = shader.get();
        }
        target.draw(sprite, states);
        target.display();
        finalCanvas = &target;
    }
    display.canvasSprite_->setTexture(finalCanvas->getTexture(), true);
    if (framePipeline.shakeActive_) {
        const sf::Vector2u textureSize = finalCanvas->getSize();
        if (textureSize.x > 0 && textureSize.y > 0) {
            const float pad = framePipeline.shakePower_;
            display.canvasSprite_->setScale(
                {(static_cast<float>(textureSize.x) + pad * 2.0f) /
                     static_cast<float>(textureSize.x),
                 (static_cast<float>(textureSize.y) + pad * 2.0f) /
                     static_cast<float>(textureSize.y)});
            display.canvasSprite_->setPosition(
                {-pad + framePipeline.shakeOffset_.x,
                 -pad + framePipeline.shakeOffset_.y});
        }
    }
    if (framePipeline.transitionOutputTexture_ == nullptr ||
        !framePipeline.transitionOutputSprite_.has_value()) {
        display.window_->draw(*display.canvasSprite_, canvasRenderStates());
    } else if (framePipeline.inTransition_ &&
               framePipeline.transitionShader_ != nullptr &&
               framePipeline.transition_ != nullptr &&
               framePipeline.transitionTempTexture_ != nullptr &&
               framePipeline.transitionSprite_.has_value()) {
        framePipeline.transitionTempTexture_->clear(sf::Color::Transparent);
        framePipeline.transitionTempTexture_->draw(*display.canvasSprite_,
                                                   sf::BlendNone);
        framePipeline.transitionTempTexture_->display();
        framePipeline.transitionShader_->setUniform(
            "screenTex", framePipeline.transitionTempTexture_->getTexture());
        framePipeline.transitionShader_->setUniform(
            "backTex", framePipeline.transition_->getTexture());
        framePipeline.transitionShader_->setUniform(
            "transitionResource",
            framePipeline.transitionResource_ != nullptr &&
                    framePipeline.transitionMaskTexture_ != nullptr
                ? framePipeline.transitionMaskTexture_->getTexture()
                : framePipeline.transition_->getTexture());
        framePipeline.transitionShader_->setUniform(
            "useMask", framePipeline.transitionResource_ != nullptr &&
                           framePipeline.transitionMaskTexture_ != nullptr);
        framePipeline.transitionShader_->setUniform(
            "progress", framePipeline.transitionTimeCount_);
        framePipeline.transitionShader_->setUniform(
            "totalTime", framePipeline.transitionTime_);
        sf::RenderStates states(sf::BlendNone);
        states.shader = framePipeline.transitionShader_.get();
        framePipeline.transitionOutputTexture_->clear(sf::Color::Transparent);
        framePipeline.transitionOutputTexture_->draw(
            *framePipeline.transitionSprite_, states);
        framePipeline.transitionOutputTexture_->display();
        display.window_->draw(*framePipeline.transitionOutputSprite_,
                              canvasRenderStates());
    } else {
        framePipeline.transitionOutputTexture_->clear(sf::Color::Transparent);
        framePipeline.transitionOutputTexture_->draw(*display.canvasSprite_,
                                                     sf::BlendNone);
        framePipeline.transitionOutputTexture_->display();
        display.window_->draw(*framePipeline.transitionOutputSprite_,
                              canvasRenderStates());
    }
    if (framePipeline.shakeActive_) {
        display.canvasSprite_->setScale({1.0f, 1.0f});
        display.canvasSprite_->setPosition({0.0f, 0.0f});
    }
    if (framePipeline.transitionFreezePending_) {
        cacheTransitionBackground();
        framePipeline.transitionFreezePending_ = false;
        framePipeline.transitionFrozen_ = true;
    }
    framePipeline.composedTransitionRevision_ =
        framePipeline.transitionRevision_;
    framePipeline.transitionCompletionPending_ =
        framePipeline.inTransition_ &&
        ludork::global::system_transition_impl::isComplete(
            framePipeline.transitionTimeCount_, framePipeline.transitionTime_);
}

void System::present() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.presentMutex_);
    if (display.window_ == nullptr || display.canvas_ == nullptr ||
        !display.canvasSprite_.has_value()) {
        return;
    }
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
    if (!isEmbeddedDisplay() && !display.desktopFullscreen_) {
        const sf::Vector2u windowSize = display.window_->getSize();
        const bool liveResizing = ludork::global::isNativeWindowLiveResizing(
            display.window_->getNativeHandle());
        if (liveResizing || windowSize != display.observedWindowSize_) {
            const std::optional<sf::Vector2u> clientSize =
                ludork::global::getWindowedClientSize(
                    display.window_->getNativeHandle());
            if (liveResizing || !clientSize.has_value() ||
                clientSize != display.observedWindowClientSize_) {
                display.pendingResizeScale_ =
                    windowFitScale(clientSize.value_or(windowSize));
                display.lastResizeTime_ = std::chrono::steady_clock::now();
            }
            return;
        }
        if (display.pendingResizeScale_.has_value()) {
            return;
        }
    }
#endif
    if (PerformanceProfiler::isEnabled()) {
        const auto presentStart = std::chrono::steady_clock::now();
        display.window_->display();
        PerformanceProfiler::addPresentWait(
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - presentStart)
                .count());
    } else {
        display.window_->display();
    }
}

void System::completeFrame() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    if (display.window_ == nullptr || display.canvas_ == nullptr ||
        !display.canvasSprite_.has_value()) {
        framePipeline.transitionCompletionPending_ = false;
        return;
    }
    if (framePipeline.transitionCompletionPending_ &&
        framePipeline.composedTransitionRevision_ ==
            framePipeline.transitionRevision_) {
        framePipeline.inTransition_ = false;
    }
    framePipeline.transitionCompletionPending_ = false;
    applyPendingDisplayChanges();
}

void System::addGraphicsShader(const std::shared_ptr<sf::Shader>& shader,
                               std::optional<ShaderUniforms> uniforms) {
    if (!shadersAvailable()) {
        if (shader != nullptr) {
            warnOnce("System.addGraphicsShader",
                     "Shaders are unavailable; ignored addGraphicsShader");
        }
        return;
    }
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.graphicsShaders_.push_back(shader);
    if (shader != nullptr && uniforms.has_value()) {
        for (const auto& [name, value] : *uniforms) {
            setShaderUniform(*shader, name, value);
        }
    }
    applyGraphicsShadersLength();
}

void System::removeGraphicsShader(const std::shared_ptr<sf::Shader>& shader) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    const auto iterator =
        std::find(framePipeline.graphicsShaders_.begin(),
                  framePipeline.graphicsShaders_.end(), shader);
    if (iterator != framePipeline.graphicsShaders_.end()) {
        framePipeline.graphicsShaders_.erase(iterator);
    }
    applyGraphicsShadersLength();
}

void System::removeAllGraphicsShaders() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.graphicsShaders_.clear();
    applyGraphicsShadersLength();
}

void System::removeGraphicsShaderAt(int index) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (index < 0 || static_cast<std::size_t>(index) >=
                         framePipeline.graphicsShaders_.size()) {
        return;
    }
    framePipeline.graphicsShaders_.erase(
        framePipeline.graphicsShaders_.begin() +
        static_cast<std::ptrdiff_t>(index));
    applyGraphicsShadersLength();
}

void System::flashScreen(std::optional<sf::Color> color, float duration) {
    if (duration <= 0.0f) {
        stopFlash();
        return;
    }
    if (!shadersAvailable()) {
        warnOnce("System.flashScreen",
                 "Shaders are unavailable; skipped screen flash effect");
        return;
    }
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (framePipeline.flashShader_ == nullptr) {
        try {
            framePipeline.flashShader_ =
                ShaderManager::load("/Game/Assets/Shaders/Global/Flash.frag");
        } catch (const std::exception&) {
            framePipeline.flashShader_.reset();
            std::cerr << "FLASH_SHADER_LOAD_FAILED\n";
            return;
        }
    }
    if (framePipeline.flashShader_ == nullptr) {
        return;
    }
    framePipeline.flashColour_ =
        sf::Glsl::Vec4(color.value_or(sf::Color::White));
    framePipeline.flashDuration_ = duration;
    framePipeline.flashTimeCount_ = 0.0f;
    if (!framePipeline.flashActive_) {
        addGraphicsShader(framePipeline.flashShader_);
        framePipeline.flashActive_ = true;
    }
    framePipeline.flashShader_->setUniform("flashColor",
                                           framePipeline.flashColour_);
    framePipeline.flashShader_->setUniform("intensity", 1.0f);
}

void System::stopFlash() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (framePipeline.flashActive_ && framePipeline.flashShader_ != nullptr) {
        removeGraphicsShader(framePipeline.flashShader_);
    }
    framePipeline.flashActive_ = false;
    framePipeline.flashTimeCount_ = 0.0f;
    framePipeline.flashDuration_ = 0.0f;
}

bool System::isFlashing() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return framePipeline.flashActive_;
}

void System::changeScreenTone(float red, float green, float blue, float gray,
                              float duration) {
    if (!shadersAvailable()) {
        warnOnce("System.changeScreenTone",
                 "Shaders are unavailable; skipped screen tone effect");
        return;
    }
    if (!ensureToneShader()) {
        return;
    }
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    const sf::Glsl::Vec4 target = makeToneColour(red, green, blue, gray);
    framePipeline.toneStartColour_ = framePipeline.toneCurrentColour_;
    framePipeline.toneTargetColour_ = target;
    framePipeline.toneDuration_ = std::max(0.0f, duration);
    framePipeline.toneTimeCount_ = 0.0f;
    framePipeline.toneActive_ = true;
    if (framePipeline.toneDuration_ <= 0.0f) {
        framePipeline.toneCurrentColour_ = target;
        applyScreenToneUniform();
        if (isNeutralTone(target)) {
            stopScreenTone();
        }
    } else {
        applyScreenToneUniform();
    }
}

void System::clearScreenTone(float duration) {
    changeScreenTone(0.0f, 0.0f, 0.0f, 0.0f, duration);
}

void System::stopScreenTone() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.toneCurrentColour_ = {};
    framePipeline.toneStartColour_ = {};
    framePipeline.toneTargetColour_ = {};
    framePipeline.toneDuration_ = 0.0f;
    framePipeline.toneTimeCount_ = 0.0f;
    framePipeline.toneActive_ = false;
}

bool System::isScreenToneActive() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return framePipeline.toneActive_;
}

bool System::isScreenToneTransitionComplete() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return !framePipeline.toneActive_ || framePipeline.toneDuration_ <= 0.0f;
}

void System::startShake(float power, float speed, float duration) {
    if (duration <= 0.0f) {
        stopShake();
        return;
    }
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.shakePower_ = power;
    framePipeline.shakeSpeed_ = speed;
    framePipeline.shakeDuration_ = duration;
    framePipeline.shakeTimeCount_ = 0.0f;
    framePipeline.shakeActive_ = true;
    framePipeline.shakeNextUpdate_ = 0.0f;
    framePipeline.shakeOffset_ = {};
}

void System::stopShake() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.shakeActive_ = false;
    framePipeline.shakeTimeCount_ = 0.0f;
    framePipeline.shakeDuration_ = 0.0f;
    framePipeline.shakeOffset_ = {};
}

bool System::isShaking() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return framePipeline.shakeActive_;
}

void System::cacheTransitionBackground() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (framePipeline.transition_ == nullptr ||
        !framePipeline.transitionOutputSprite_.has_value()) {
        return;
    }
    framePipeline.transition_->clear(sf::Color::Transparent);
    framePipeline.transition_->draw(*framePipeline.transitionOutputSprite_,
                                    sf::BlendNone);
    framePipeline.transition_->display();
}

void System::setTransition(
    const std::shared_ptr<sf::Texture>& transitionResource,
    float transitionTime) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.presentMutex_);
    ++framePipeline.transitionRevision_;
    framePipeline.transitionResource_ = transitionResource;
    if (framePipeline.transitionFreezePending_) {
        cacheTransitionBackground();
        framePipeline.transitionFreezePending_ = false;
        framePipeline.transitionFrozen_ = true;
    }
    if (framePipeline.transitionResource_ != nullptr &&
        framePipeline.transitionMaskTexture_ != nullptr) {
        const sf::Vector2u sourceSize =
            framePipeline.transitionResource_->getSize();
        const sf::Vector2u targetSize =
            framePipeline.transitionMaskTexture_->getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0 && targetSize.x > 0 &&
            targetSize.y > 0) {
            sf::Sprite maskSprite(*framePipeline.transitionResource_);
            maskSprite.setScale({static_cast<float>(targetSize.x) /
                                     static_cast<float>(sourceSize.x),
                                 static_cast<float>(targetSize.y) /
                                     static_cast<float>(sourceSize.y)});
            framePipeline.transitionMaskTexture_->clear(sf::Color::Transparent);
            framePipeline.transitionMaskTexture_->draw(maskSprite,
                                                       sf::BlendNone);
            framePipeline.transitionMaskTexture_->display();
        }
    }
    if (!framePipeline.transitionFrozen_) {
        cacheTransitionBackground();
    } else {
        framePipeline.transitionFrozen_ = false;
    }
    if (framePipeline.transitionShader_ == nullptr) {
        framePipeline.inTransition_ = false;
        return;
    }
    framePipeline.inTransition_ = true;
    framePipeline.transitionTimeCount_ = 0.0f;
    framePipeline.transitionTime_ = std::max(0.0f, transitionTime);
    if (framePipeline.transitionTempTexture_ != nullptr) {
        framePipeline.transitionTempTexture_->clear(sf::Color::Transparent);
    }
}

void System::freezeTransitionBackground() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.transitionFreezePending_ = true;
}

bool System::isTransitionBackgroundFrozen() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return framePipeline.transitionFrozen_;
}

bool System::isTransitionBackgroundFreezePending() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return framePipeline.transitionFreezePending_;
}

void System::cancelTransitionBackgroundFreeze() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    framePipeline.transitionFreezePending_ = false;
    framePipeline.transitionFrozen_ = false;
}

void System::requestTransition(std::optional<std::string> transitionName,
                               float transitionTime) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
    framePipeline.pendingTransition_ =
        PendingTransition{std::move(transitionName), transitionTime};
}

void System::cancelPendingTransition() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
    framePipeline.pendingTransition_.reset();
}

bool System::isTransitionPending() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
    return framePipeline.pendingTransition_.has_value();
}

bool System::isInTransition() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    return framePipeline.inTransition_;
}

void System::applyPendingTransition() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    std::optional<PendingTransition> pending;
    {
        const std::lock_guard<std::mutex> lock(framePipeline.transitionMutex_);
        pending.swap(framePipeline.pendingTransition_);
    }
    if (!pending.has_value()) {
        return;
    }
    std::shared_ptr<sf::Texture> resource;
    if (pending->name.has_value() && !pending->name->empty()) {
        resource = TextureManager::load(*pending->name);
    }
    setTransition(resource, pending->time);
}
void System::updateFlash(float deltaTime) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (!framePipeline.flashActive_ || framePipeline.flashShader_ == nullptr) {
        return;
    }
    framePipeline.flashTimeCount_ =
        std::min(framePipeline.flashTimeCount_ + deltaTime,
                 framePipeline.flashDuration_);
    const float intensity =
        framePipeline.flashDuration_ > 0.0f
            ? std::max(0.0f, 1.0f - framePipeline.flashTimeCount_ /
                                        framePipeline.flashDuration_)
            : 0.0f;
    framePipeline.flashShader_->setUniform("flashColor",
                                           framePipeline.flashColour_);
    framePipeline.flashShader_->setUniform("intensity", intensity);
    if (framePipeline.flashTimeCount_ >= framePipeline.flashDuration_) {
        removeGraphicsShader(framePipeline.flashShader_);
        framePipeline.flashActive_ = false;
    }
}

void System::updateScreenTone(float deltaTime) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (!framePipeline.toneActive_ || framePipeline.toneShader_ == nullptr) {
        return;
    }
    if (framePipeline.toneDuration_ > 0.0f) {
        framePipeline.toneTimeCount_ =
            std::min(framePipeline.toneTimeCount_ + deltaTime,
                     framePipeline.toneDuration_);
        const float ratio = std::min(
            1.0f, framePipeline.toneTimeCount_ / framePipeline.toneDuration_);
        framePipeline.toneCurrentColour_ =
            ludork::global::system_screen_effects_impl::interpolateTone(
                framePipeline.toneStartColour_, framePipeline.toneTargetColour_,
                ratio);
    }
    applyScreenToneUniform();
    if (framePipeline.toneDuration_ > 0.0f &&
        framePipeline.toneTimeCount_ >= framePipeline.toneDuration_) {
        framePipeline.toneDuration_ = 0.0f;
        if (isNeutralTone(framePipeline.toneCurrentColour_)) {
            stopScreenTone();
        }
    }
}

void System::updateShake(float deltaTime) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (!framePipeline.shakeActive_) {
        return;
    }
    framePipeline.shakeTimeCount_ =
        std::min(framePipeline.shakeTimeCount_ + deltaTime,
                 framePipeline.shakeDuration_);
    if (framePipeline.shakeTimeCount_ >= framePipeline.shakeDuration_) {
        stopShake();
        return;
    }
    const float remainingPower =
        framePipeline.shakePower_ *
        (1.0f - framePipeline.shakeTimeCount_ / framePipeline.shakeDuration_);
    framePipeline.shakeNextUpdate_ -= deltaTime;
    if (framePipeline.shakeNextUpdate_ <= 0.0f) {
        framePipeline.shakeNextUpdate_ =
            framePipeline.shakeSpeed_ > 0.0f
                ? 1.0f / framePipeline.shakeSpeed_
                : std::numeric_limits<float>::max();
        std::uniform_real_distribution<float> offset(-remainingPower,
                                                     remainingPower);
        framePipeline.shakeOffset_ = {offset(framePipeline.random_),
                                      offset(framePipeline.random_)};
    }
}

bool System::ensureToneShader() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (framePipeline.toneShader_ != nullptr) {
        return true;
    }
    try {
        framePipeline.toneShader_ =
            ShaderManager::load("/Game/Assets/Shaders/Global/Tone.frag");
    } catch (const std::exception&) {
        framePipeline.toneShader_.reset();
        std::cerr << "TONE_SHADER_LOAD_FAILED\n";
        return false;
    }
    return framePipeline.toneShader_ != nullptr;
}

void System::applyScreenToneUniform() {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (framePipeline.toneShader_ != nullptr) {
        framePipeline.toneShader_->setUniform("toneColor",
                                              framePipeline.toneCurrentColour_);
    }
}

void System::ensureToneBuffer(const sf::Vector2u& size) {
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        ludork::global::system_runtime::runtime().framePipeline;
    if (framePipeline.toneBuffer_ == nullptr ||
        framePipeline.toneBuffer_->getSize() != size) {
        framePipeline.toneBuffer_ = std::make_unique<sf::RenderTexture>(size);
        framePipeline.toneBufferSprite_.emplace(
            framePipeline.toneBuffer_->getTexture());
    } else if (!framePipeline.toneBufferSprite_.has_value()) {
        framePipeline.toneBufferSprite_.emplace(
            framePipeline.toneBuffer_->getTexture());
    }
}

sf::Glsl::Vec4 System::makeToneColour(float red, float green, float blue,
                                      float gray) {
    return ludork::global::system_screen_effects_impl::makeToneColour(
        red, green, blue, gray);
}

bool System::isNeutralTone(const sf::Glsl::Vec4& toneColour) {
    return ludork::global::system_screen_effects_impl::isNeutralTone(
        toneColour);
}

void System::applyGraphicsShadersLength() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::DisplayRuntime& display =
        systemRuntime.display;
    ludork::global::system_runtime::FramePipelineRuntime& framePipeline =
        systemRuntime.framePipeline;
    while (framePipeline.graphicsCanvases_.size() >
           framePipeline.graphicsShaders_.size()) {
        framePipeline.graphicsCanvases_.pop_back();
    }
    if (display.canvas_ == nullptr) {
        return;
    }
    const sf::Vector2u size = display.canvas_->getSize();
    while (framePipeline.graphicsCanvases_.size() <
           framePipeline.graphicsShaders_.size()) {
        framePipeline.graphicsCanvases_.push_back(
            std::make_unique<sf::RenderTexture>(size));
    }
    for (std::unique_ptr<sf::RenderTexture>& graphicsCanvas :
         framePipeline.graphicsCanvases_) {
        if (graphicsCanvas->getSize() != size) {
            graphicsCanvas = std::make_unique<sf::RenderTexture>(size);
        }
    }
}

void System::setShaderUniform(sf::Shader& shader, const std::string& name,
                              const ShaderUniformValue& value) {
    std::visit(
        [&shader, &name](const auto& current) {
            using Value = std::decay_t<decltype(current)>;
            if constexpr (std::is_same_v<Value, sf::Color>) {
                shader.setUniform(name, sf::Glsl::Vec4(current));
            } else if constexpr (std::is_same_v<Value,
                                                std::shared_ptr<sf::Texture>>) {
                if (current != nullptr) {
                    shader.setUniform(name, *current);
                }
            } else if constexpr (std::is_same_v<Value, std::vector<float>> ||
                                 std::is_same_v<Value,
                                                std::vector<sf::Vector2f>> ||
                                 std::is_same_v<Value,
                                                std::vector<sf::Vector3f>> ||
                                 std::is_same_v<Value,
                                                std::vector<sf::Glsl::Vec4>>) {
                if (!current.empty()) {
                    shader.setUniformArray(name, current.data(),
                                           current.size());
                }
            } else {
                shader.setUniform(name, current);
            }
        },
        value);
}

bool System::shadersAvailable() {
    return sf::Shader::isAvailable();
}
std::shared_ptr<SceneRuntime> System::getScene() {
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        ludork::global::system_runtime::runtime().sceneStack;
    const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
    return sceneStack.scenes_.empty() ? nullptr : sceneStack.scenes_.back();
}

std::shared_ptr<SceneRuntime> System::requireScene() {
    const std::shared_ptr<SceneRuntime> scene = getScene();
    if (scene == nullptr) {
        throw std::runtime_error("No active scene");
    }
    return scene;
}

std::vector<std::shared_ptr<SceneRuntime>> System::getSceneList() {
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        ludork::global::system_runtime::runtime().sceneStack;
    const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
    return sceneStack.scenes_;
}

void System::bindSceneOperationThread() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
    if (lifecycle.shuttingDown_.load()) {
        return;
    }
    sceneStack.sceneOperationThread_ = std::this_thread::get_id();
}

void System::unbindSceneOperationThread() {
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        ludork::global::system_runtime::runtime().sceneStack;
    const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
    if (sceneStack.sceneOperationThread_ == std::this_thread::get_id()) {
        sceneStack.sceneOperationThread_ = {};
    }
}

bool System::hasPendingSceneOperations() {
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        ludork::global::system_runtime::runtime().sceneStack;
    const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
    return !sceneStack.pendingSceneOperations_.empty();
}

void System::applyPendingSceneReplace() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    std::deque<PendingSceneOperation> operations;
    {
        const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
        if (lifecycle.shuttingDown_.load()) {
            return;
        }
        if (sceneStack.sceneOperationThread_ != std::thread::id{} &&
            sceneStack.sceneOperationThread_ != std::this_thread::get_id()) {
            return;
        }
        operations.swap(sceneStack.pendingSceneOperations_);
    }
    while (!operations.empty()) {
        PendingSceneOperation operation = std::move(operations.front());
        operations.pop_front();
        applySceneOperation(std::move(operation));
    }
}

void System::setScene(const std::shared_ptr<SceneRuntime>& scene) {
    if (scene == nullptr) {
        throw std::invalid_argument("Scene cannot be null");
    }
    requestSceneOperation(SceneOperationType::Replace, scene);
}

void System::pushScene(const std::shared_ptr<SceneRuntime>& scene) {
    if (scene == nullptr) {
        throw std::invalid_argument("Scene cannot be null");
    }
    requestSceneOperation(SceneOperationType::Push, scene);
}

void System::popScene() {
    requestSceneOperation(SceneOperationType::Pop);
}

void System::exit() {
    requestSceneOperation(SceneOperationType::Exit);
}
void System::requestSceneOperation(SceneOperationType type,
                                   std::shared_ptr<SceneRuntime> scene) {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    bool applyImmediately = false;
    {
        const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
        if (lifecycle.shuttingDown_.load()) {
            return;
        }
        applyImmediately =
            sceneStack.sceneOperationThread_ == std::thread::id{};
        if (!applyImmediately) {
            sceneStack.pendingSceneOperations_.push_back(
                {type, std::move(scene)});
        }
    }
    if (applyImmediately) {
        applySceneOperation({type, std::move(scene)});
    }
}

void System::applySceneOperation(PendingSceneOperation operation) {
    switch (operation.type) {
        case SceneOperationType::Replace:
            applySetScene(operation.scene);
            break;
        case SceneOperationType::Push:
            applyPushScene(operation.scene);
            break;
        case SceneOperationType::Pop:
            applyPopScene();
            break;
        case SceneOperationType::Exit:
            applyExit();
            break;
    }
}

void System::applySetScene(const std::shared_ptr<SceneRuntime>& scene) {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    if (lifecycle.shuttingDown_.load()) {
        return;
    }
    freezeTransitionBackground();
    {
        const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
        if (lifecycle.shuttingDown_.load()) {
            return;
        }
        if (sceneStack.scenes_.empty()) {
            sceneStack.scenes_.push_back(scene);
        } else {
            sceneStack.retiredScenes_.push_back(
                std::move(sceneStack.scenes_.back()));
            sceneStack.scenes_.back() = scene;
        }
    }
    drainRetiredScenes();
}

void System::applyPushScene(const std::shared_ptr<SceneRuntime>& scene) {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
    if (lifecycle.shuttingDown_.load()) {
        return;
    }
    sceneStack.scenes_.push_back(scene);
}

void System::applyPopScene() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    {
        const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
        if (lifecycle.shuttingDown_.load()) {
            return;
        }
        if (sceneStack.scenes_.empty()) {
            throw std::logic_error("Cannot pop an empty scene stack");
        }
        sceneStack.retiredScenes_.push_back(
            std::move(sceneStack.scenes_.back()));
        sceneStack.scenes_.pop_back();
    }
    drainRetiredScenes();
}

void System::applyExit() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    std::vector<std::shared_ptr<SceneRuntime>> scenes;
    {
        const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
        if (lifecycle.shuttingDown_.load()) {
            return;
        }
        scenes.swap(sceneStack.scenes_);
        for (auto iterator = scenes.rbegin(); iterator != scenes.rend();
             ++iterator) {
            sceneStack.retiredScenes_.push_back(std::move(*iterator));
        }
    }
    drainRetiredScenes();
}

void System::drainRetiredScenes() {
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        ludork::global::system_runtime::runtime().sceneStack;
    std::exception_ptr failure;
    while (true) {
        std::shared_ptr<SceneRuntime> scene;
        {
            const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
            if (sceneStack.retiredScenes_.empty() ||
                (sceneStack.retiredScenes_.front() != nullptr &&
                 sceneStack.retiredScenes_.front()->systemIsRunning())) {
                break;
            }
            scene = std::move(sceneStack.retiredScenes_.front());
            sceneStack.retiredScenes_.pop_front();
        }
        if (scene == nullptr) {
            continue;
        }
        try {
            scene->systemDestroy();
        } catch (...) {
            if (failure == nullptr) {
                failure = std::current_exception();
            }
        }
    }
    if (failure != nullptr) {
        std::rethrow_exception(failure);
    }
}

bool System::isPerformanceProfilerEnabled() {
    return PerformanceProfiler::isEnabled();
}

void System::recordWorldStreamingPerformance(
    int queueDepth, int reading, int prepared, int active, int dormant,
    std::int64_t cacheBytes, double publishMilliseconds, int visibleTileChunks,
    int activeActors) {
    PerformanceProfiler::recordWorldStreaming({
        queueDepth,
        reading,
        prepared,
        active,
        dormant,
        cacheBytes,
        publishMilliseconds,
        visibleTileChunks,
        activeActors,
    });
}
