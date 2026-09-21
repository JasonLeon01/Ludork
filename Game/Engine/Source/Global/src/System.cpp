#include <System.hpp>
#include "System/LifecycleImpl.hpp"
#include "System/DisplayImpl.hpp"
#include "System/SceneStackImpl.hpp"
#include "System/FramePipelineImpl.hpp"
#include "System/Config/ConfigStoreImpl.hpp"
#include "System/Config/SystemConfigImpl.hpp"
#include "System/Config/DisplayConfigImpl.hpp"
#include "System/Config/GraphicsConfigImpl.hpp"
#include "System/Config/AudioConfigImpl.hpp"
#include <EngineState.hpp>
#include <Manager/TimeManager.hpp>
#include <stdexcept>
#include <utility>

void System::init(const std::shared_ptr<ludork::standard::ConfigParser>& data,
                  const std::string& dataFilePath) {
    engineState().setGameRunning(true);
    ludork::global::system_impl::ConfigStoreImpl::init(data, dataFilePath);
    ludork::global::system_impl::SystemConfigImpl::initialize();
    ludork::global::system_impl::DisplayConfigImpl::initialize();
    ludork::global::system_impl::GraphicsConfigImpl::initialize();
    ludork::global::system_impl::AudioConfigImpl::initialize();
    ludork::global::system_impl::GraphicsConfigImpl::initializeRenderScale(
        ludork::global::system_impl::DisplayConfigImpl::getConfiguredScale());
    ludork::global::system_impl::displayImpl().reset();
    ludork::global::system_impl::framePipelineImpl().reset();
    ludork::global::system_impl::sceneStackImpl().reset();
    TimeManager::init();
}

std::string System::getScript() {
    return ludork::global::system_impl::SystemConfigImpl::getScript();
}

void System::setScript(const std::string& value) {
    ludork::global::system_impl::SystemConfigImpl::setScript(value);
}

void System::saveScript(const std::string& value) {
    ludork::global::system_impl::SystemConfigImpl::saveScript(value);
}

std::string System::getLanguage() {
    return ludork::global::system_impl::SystemConfigImpl::getLanguage();
}

void System::setLanguage(const std::string& value) {
    ludork::global::system_impl::SystemConfigImpl::setLanguage(value);
}

void System::saveLanguage(const std::string& value) {
    ludork::global::system_impl::SystemConfigImpl::saveLanguage(value);
}

bool System::isDebugMode() {
    return ludork::global::system_impl::lifecycleImpl().isDebugMode();
}

void System::setDebugMode(bool debugMode) {
    ludork::global::system_impl::lifecycleImpl().setDebugMode(debugMode);
}

bool System::isActive() {
    return !ludork::global::system_impl::lifecycleImpl().isShuttingDown() &&
           engineState().getGameRunning() &&
           ludork::global::system_impl::displayImpl().isOpen();
}

bool System::shouldLoop() {
    return isActive() &&
           ludork::global::system_impl::sceneStackImpl().getScene() != nullptr;
}

void System::run() {
    std::shared_ptr<sf::RenderWindow> window =
        ludork::global::system_impl::displayImpl().getWindow();
    if (ludork::global::system_impl::lifecycleImpl().isShuttingDown()) {
        throw std::runtime_error(
            "Game loop cannot start during runtime shutdown");
    }
    if (window == nullptr) {
        throw std::runtime_error("Game loop cannot start without a window");
    }
    if (!window->isOpen()) {
        throw std::runtime_error(
            "Game loop cannot start because the window is closed");
    }
    if (!engineState().getGameRunning()) {
        throw std::runtime_error(
            "Game loop cannot start while the game is stopped");
    }
    if (ludork::global::system_impl::sceneStackImpl().getScene() == nullptr) {
        throw std::runtime_error("Game loop cannot start without a scene");
    }
    window.reset();
    ludork::global::system_impl::sceneStackImpl().bindSceneOperationThread();
    try {
        while (shouldLoop()) {
            ludork::global::system_impl::sceneStackImpl()
                .applyPendingSceneReplace();
            const std::shared_ptr<SceneRuntime> currentScene =
                ludork::global::system_impl::sceneStackImpl().getScene();
            if (currentScene != nullptr) {
                currentScene->systemMain();
            }
        }
    } catch (...) {
        ludork::global::system_impl::sceneStackImpl()
            .unbindSceneOperationThread();
        throw;
    }
    ludork::global::system_impl::sceneStackImpl().unbindSceneOperationThread();
}

void System::setStandardUpdate(std::function<void()> update) {
    ludork::global::system_impl::lifecycleImpl().setStandardUpdate(
        std::move(update));
}

void System::updateRuntime() {
    ludork::global::system_impl::lifecycleImpl().updateRuntime();
}

void System::initializeRuntimeSession() noexcept {
    ludork::global::system_impl::lifecycleImpl().initializeRuntimeSession();
}

void System::shutdownRuntime() noexcept {
    ludork::global::system_impl::lifecycleImpl().shutdownRuntime([]() noexcept {
        ludork::global::system_impl::sceneStackImpl().shutdown();
        ludork::global::system_impl::framePipelineImpl()
            .transition()
            .cancelPendingTransition();
        ludork::global::system_impl::lifecycleImpl().setStandardUpdate({});
        ludork::global::system_impl::framePipelineImpl().shutdown();
        ludork::global::system_impl::displayImpl().shutdown();
    });
}

void System::exit() {
    ludork::global::system_impl::sceneStackImpl().exit();
}

void System::shutdownConfiguration() noexcept {
    ludork::global::system_impl::SystemConfigImpl::shutdown();
    ludork::global::system_impl::DisplayConfigImpl::shutdown();
    ludork::global::system_impl::GraphicsConfigImpl::shutdown();
    ludork::global::system_impl::AudioConfigImpl::shutdown();
    ludork::global::system_impl::ConfigStoreImpl::shutdown();
}
