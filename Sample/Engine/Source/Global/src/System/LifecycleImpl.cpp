#include "LifecycleImpl.hpp"
#include "SystemImpl.hpp"
#include "DisplayImpl.hpp"
#include "ScreenEffectsImpl.hpp"
#include "SceneStackImpl.hpp"
#include "Platform/NativeInputMethod.hpp"
#include <EngineState.hpp>
#include <Manager/AudioManager.hpp>
#include <Manager/TimeManager.hpp>
#include <SystemConfigBase.hpp>
#include <algorithm>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace ludork::global::system_lifecycle_impl {

void init(const std::shared_ptr<ludork::standard::ConfigParser>& data,
          const std::string& dataFilePath) {
    engineState().setGameRunning(true);
    SystemConfigBase::init(data, dataFilePath);
    SystemConfigBase::setChangeHandler(
        ludork::global::system_lifecycle_impl::onConfigChanged);
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::DisplayImpl& display = systemImpl.display;
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        systemImpl.framePipeline;
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        systemImpl.sceneStack;
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
    ludork::global::system_screen_effects_impl::stopFlash();
    ludork::global::system_screen_effects_impl::stopScreenTone();
    ludork::global::system_screen_effects_impl::stopShake();
    TimeManager::init();
    framePipeline.transitionShader_.reset();
}

bool isDebugMode() {
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        ludork::global::system_impl::impl().lifecycle;
    return lifecycle.debugMode_;
}

void setDebugMode(bool debugMode) {
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        ludork::global::system_impl::impl().lifecycle;
    lifecycle.debugMode_ = debugMode;
}

bool isActive() {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::DisplayImpl& display = systemImpl.display;
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        systemImpl.lifecycle;
    if (lifecycle.shuttingDown_.load() || !engineState().getGameRunning()) {
        return false;
    }
    const std::lock_guard<std::mutex> lock(display.windowMutex_);
    return display.window_ != nullptr && display.window_->isOpen();
}

bool shouldLoop() {
    return ludork::global::system_lifecycle_impl::isActive() &&
           ludork::global::system_scene_stack_impl::getScene() != nullptr;
}

void run() {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::DisplayImpl& display = systemImpl.display;
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        systemImpl.lifecycle;
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
    if (ludork::global::system_scene_stack_impl::getScene() == nullptr) {
        throw std::runtime_error("Game loop cannot start without a scene");
    }
    ludork::global::system_scene_stack_impl::bindSceneOperationThread();
    try {
        while (ludork::global::system_lifecycle_impl::shouldLoop()) {
            ludork::global::system_scene_stack_impl::applyPendingSceneReplace();
            const std::shared_ptr<SceneRuntime> currentScene =
                ludork::global::system_scene_stack_impl::getScene();
            if (currentScene != nullptr) {
                currentScene->systemMain();
            }
        }
    } catch (...) {
        ludork::global::system_scene_stack_impl::unbindSceneOperationThread();
        throw;
    }
    ludork::global::system_scene_stack_impl::unbindSceneOperationThread();
}

void setStandardUpdate(std::function<void()> update) {
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        ludork::global::system_impl::impl().lifecycle;
    lifecycle.standardUpdate_ = std::move(update);
}

void updateRuntime() {
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        ludork::global::system_impl::impl().lifecycle;
    if (!lifecycle.shuttingDown_.load() && lifecycle.standardUpdate_) {
        try {
            lifecycle.standardUpdate_();
        } catch (...) {
            AudioManager::stopAll();
            throw;
        }
    }
}

void initializeRuntimeSession() noexcept {
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        ludork::global::system_impl::impl().lifecycle;
    const std::lock_guard<std::mutex> lifecycleLock(lifecycle.lifecycleMutex_);
    lifecycle.shuttingDown_.store(false);
}

void shutdownRuntime() noexcept {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::DisplayImpl& display = systemImpl.display;
    ludork::global::system_impl::FramePipelineImpl& framePipeline =
        systemImpl.framePipeline;
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        systemImpl.sceneStack;
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        systemImpl.lifecycle;
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

void onConfigChanged(const std::string& key) {
    if (key == "scale") {
        if (ludork::global::system_display_impl::getWindow() != nullptr) {
            ludork::global::system_impl::DisplayImpl& display =
                ludork::global::system_impl::impl().display;
            display.pendingConfiguredScale_ =
                SystemConfigBase::getConfiguredScale();
        }
    } else if (key == "maximumRenderScale") {
        if (ludork::global::system_display_impl::getWindow() != nullptr) {
            ludork::global::system_impl::DisplayImpl& display =
                ludork::global::system_impl::impl().display;
            display.pendingRenderTargetRebuild_ = true;
        }
    } else if (key == "frameRate") {
        const std::shared_ptr<sf::RenderWindow> window =
            ludork::global::system_display_impl::getWindow();
        if (window != nullptr) {
            window->setFramerateLimit(static_cast<unsigned int>(
                std::max(0, SystemConfigBase::getFrameRate())));
        }
    } else if (key == "verticalSync") {
        const std::shared_ptr<sf::RenderWindow> window =
            ludork::global::system_display_impl::getWindow();
        if (window != nullptr) {
            window->setVerticalSyncEnabled(SystemConfigBase::getVerticalSync());
        }
    } else if (key == "musicOn" || key == "musicVolume") {
        AudioManager::applyMusicVolumes();
    } else if (key == "soundOn") {
        if (SystemConfigBase::getSoundOn()) {
            AudioManager::applySoundVolumes();
        } else {
            AudioManager::stopSound();
        }
    } else if (key == "soundVolume") {
        AudioManager::applySoundVolumes();
    } else if (key == "voiceOn") {
        if (SystemConfigBase::getVoiceOn()) {
            AudioManager::applyVoiceVolumes();
        } else {
            AudioManager::stopVoice();
        }
    } else if (key == "voiceVolume") {
        AudioManager::applyVoiceVolumes();
    }
}

}  // namespace ludork::global::system_lifecycle_impl
