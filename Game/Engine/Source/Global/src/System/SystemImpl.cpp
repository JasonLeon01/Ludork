#include "SystemImpl.hpp"
#include <EngineState.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Manager/AudioManager.hpp>
#include <Manager/TimeManager.hpp>
#include <SystemConfigBase.hpp>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ludork::global::system_impl {

void SystemImpl::addGraphicsShader(const std::shared_ptr<sf::Shader>& shader,
                                   std::optional<ShaderUniforms> uniforms) {
    framePipeline_.addGraphicsShader(shader, std::move(uniforms));
}

void SystemImpl::applyPendingSceneReplace() {
    sceneStack_.applyPendingSceneReplace();
}

void SystemImpl::applyPendingTransition() {
    framePipeline_.applyPendingTransition();
}

void SystemImpl::applyScreenTonePass() {
    framePipeline_.applyScreenTonePass();
}

void SystemImpl::bindSceneOperationThread() {
    sceneStack_.bindSceneOperationThread();
}

void SystemImpl::cancelPendingTransition() {
    framePipeline_.cancelPendingTransition();
}

void SystemImpl::cancelTransitionBackgroundFreeze() {
    framePipeline_.cancelTransitionBackgroundFreeze();
}

void SystemImpl::changeScreenTone(float red, float green, float blue,
                                  float gray, float duration) {
    framePipeline_.changeScreenTone(red, green, blue, gray, duration);
}

void SystemImpl::clearScreenTone(float duration) {
    framePipeline_.clearScreenTone(duration);
}

void SystemImpl::drainRetiredScenes() {
    sceneStack_.drainRetiredScenes();
}

void SystemImpl::draw(const sf::Drawable& drawable, sf::Shader* shader) {
    framePipeline_.draw(drawable, shader);
}

void SystemImpl::exit() {
    sceneStack_.exit();
}

void SystemImpl::flashScreen(std::optional<sf::Color> color, float duration) {
    framePipeline_.flashScreen(color, duration);
}

void SystemImpl::freezeTransitionBackground() {
    framePipeline_.freezeTransitionBackground();
}

sf::RenderTexture* SystemImpl::getCanvas() {
    return framePipeline_.getCanvas();
}

sf::Vector2u SystemImpl::getGameSize() {
    return display_.getGameSize();
}

std::optional<float> SystemImpl::getMaximumWindowedScale(
    const sf::Vector2u& gameSize) {
    return display_.getMaximumWindowedScale(gameSize);
}

std::shared_ptr<SceneRuntime> SystemImpl::getScene() {
    return sceneStack_.getScene();
}

std::vector<std::shared_ptr<SceneRuntime>> SystemImpl::getSceneList() {
    return sceneStack_.getSceneList();
}

std::shared_ptr<sf::RenderWindow> SystemImpl::getWindow() {
    return display_.getWindow();
}

bool SystemImpl::hasPendingSceneOperations() {
    return sceneStack_.hasPendingSceneOperations();
}

void SystemImpl::initCanvas(const sf::Vector2u& size) {
    framePipeline_.initCanvas(size);
}

void SystemImpl::initWindow(const std::shared_ptr<sf::RenderWindow>& window) {
    display_.initWindow(window);
}

void SystemImpl::initializeRuntimeSession() noexcept {
    lifecycle_.initializeRuntimeSession();
}

bool SystemImpl::isDebugMode() {
    return lifecycle_.isDebugMode();
}

bool SystemImpl::isDisplayScaleConfigurable() {
    return display_.isDisplayScaleConfigurable();
}

bool SystemImpl::isFlashing() {
    return framePipeline_.isFlashing();
}

bool SystemImpl::isInTransition() {
    return framePipeline_.isInTransition();
}

bool SystemImpl::isScreenToneActive() {
    return framePipeline_.isScreenToneActive();
}

bool SystemImpl::isScreenToneTransitionComplete() {
    return framePipeline_.isScreenToneTransitionComplete();
}

bool SystemImpl::isShaking() {
    return framePipeline_.isShaking();
}

bool SystemImpl::isTransitionBackgroundFreezePending() {
    return framePipeline_.isTransitionBackgroundFreezePending();
}

bool SystemImpl::isTransitionBackgroundFrozen() {
    return framePipeline_.isTransitionBackgroundFrozen();
}

bool SystemImpl::isTransitionPending() {
    return framePipeline_.isTransitionPending();
}

void SystemImpl::popScene() {
    sceneStack_.popScene();
}

void SystemImpl::pushScene(const std::shared_ptr<SceneRuntime>& scene) {
    sceneStack_.pushScene(scene);
}

void SystemImpl::removeAllGraphicsShaders() {
    framePipeline_.removeAllGraphicsShaders();
}

void SystemImpl::removeGraphicsShader(
    const std::shared_ptr<sf::Shader>& shader) {
    framePipeline_.removeGraphicsShader(shader);
}

void SystemImpl::removeGraphicsShaderAt(int index) {
    framePipeline_.removeGraphicsShaderAt(index);
}

void SystemImpl::requestTransition(std::optional<std::string> transitionName,
                                   float transitionTime) {
    framePipeline_.requestTransition(std::move(transitionName), transitionTime);
}

std::shared_ptr<SceneRuntime> SystemImpl::requireScene() {
    return sceneStack_.requireScene();
}

void SystemImpl::setDebugMode(bool debugMode) {
    lifecycle_.setDebugMode(debugMode);
}

void SystemImpl::setGameSize(const sf::Vector2u& gameSize) {
    display_.setGameSize(gameSize);
}

void SystemImpl::setInputMethodDisabled(bool disabled) {
    display_.setInputMethodDisabled(disabled);
}

void SystemImpl::setScene(const std::shared_ptr<SceneRuntime>& scene) {
    sceneStack_.setScene(scene);
}

void SystemImpl::setStandardUpdate(std::function<void()> update) {
    lifecycle_.setStandardUpdate(std::move(update));
}

void SystemImpl::setTransition(
    const std::shared_ptr<sf::Texture>& transitionResource,
    float transitionTime) {
    framePipeline_.setTransition(transitionResource, transitionTime);
}

void SystemImpl::setWindowDefaultView() {
    framePipeline_.setWindowDefaultView();
}

void SystemImpl::setWindowMapView(const sf::IntRect& rect) {
    framePipeline_.setWindowMapView(rect);
}

void SystemImpl::startShake(float power, float speed, float duration) {
    framePipeline_.startShake(power, speed, duration);
}

void SystemImpl::stopFlash() {
    framePipeline_.stopFlash();
}

void SystemImpl::stopScreenTone() {
    framePipeline_.stopScreenTone();
}

void SystemImpl::stopShake() {
    framePipeline_.stopShake();
}

void SystemImpl::updateRuntime() {
    lifecycle_.updateRuntime();
}

void SystemImpl::init(
    const std::shared_ptr<ludork::standard::ConfigParser>& data,
    const std::string& dataFilePath) {
    engineState().setGameRunning(true);
    SystemConfigBase::init(data, dataFilePath);
    SystemConfigBase::setChangeHandler([this](const std::string& key) {
        onConfigChanged(key);
    });
    display_.reset();
    framePipeline_.reset();
    sceneStack_.reset();
    TimeManager::init();
}

bool SystemImpl::isActive() {
    return !lifecycle_.isShuttingDown() && engineState().getGameRunning() &&
           display_.isOpen();
}

bool SystemImpl::shouldLoop() {
    return isActive() && sceneStack_.getScene() != nullptr;
}

void SystemImpl::run() {
    std::shared_ptr<sf::RenderWindow> window = display_.getWindow();
    if (lifecycle_.isShuttingDown()) {
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
    if (sceneStack_.getScene() == nullptr) {
        throw std::runtime_error("Game loop cannot start without a scene");
    }
    window.reset();
    sceneStack_.bindSceneOperationThread();
    try {
        while (shouldLoop()) {
            sceneStack_.applyPendingSceneReplace();
            const std::shared_ptr<SceneRuntime> currentScene =
                sceneStack_.getScene();
            if (currentScene != nullptr) {
                currentScene->systemMain();
            }
        }
    } catch (...) {
        sceneStack_.unbindSceneOperationThread();
        throw;
    }
    sceneStack_.unbindSceneOperationThread();
}

void SystemImpl::shutdownRuntime() noexcept {
    lifecycle_.shutdownRuntime([this]() noexcept {
        sceneStack_.shutdown();
        framePipeline_.cancelPendingTransition();
        lifecycle_.setStandardUpdate({});
        framePipeline_.shutdown();
        display_.shutdown();
    });
}

void SystemImpl::initializeDisplay(const std::string& title,
                                   const sf::Vector2u& gameSize,
                                   const std::string& iconPath,
                                   const std::string& cursorPath) {
    display_.prepareInitialization(title, gameSize, iconPath, cursorPath);
    lifecycle_.setDebugMode(ludork::global::runtimeLaunchOptions().editor);
    display_.createDisplayWindow();
    framePipeline_.initializeGraphics();
    display_.initializeInput();
    framePipeline_.initCanvas(
        display_.renderSizeForScale(SystemConfigBase::getScale()));
    display_.finishInitialization(framePipeline_.getCanvasSize());
}

void SystemImpl::clearCanvas() {
    display_.clearWindow();
    framePipeline_.clearCanvas();
}

void SystemImpl::composeFrame(float deltaTime) {
    const std::shared_ptr<sf::RenderWindow> window = display_.getWindow();
    framePipeline_.composeFrame(deltaTime, window.get());
}

void SystemImpl::present() {
    framePipeline_.present(display_);
}

void SystemImpl::completeFrame() {
    if (framePipeline_.completeFrame(display_.getWindow() != nullptr)) {
        applyPendingDisplayChanges();
    }
}

void SystemImpl::rebuildDisplayTargets(float surfaceFitScale) {
    const float normalizedSurfaceFitScale = std::max(0.01f, surfaceFitScale);
    const float renderScale =
        display_.effectiveRenderScale(normalizedSurfaceFitScale);
    const sf::Vector2u size = display_.renderSizeForScale(renderScale);
    display_.setSurfaceFitScale(normalizedSurfaceFitScale);
    if (framePipeline_.getCanvasSize() == size &&
        engineState().getScale() == renderScale) {
        display_.updateWindowViewport(size);
        return;
    }
    framePipeline_.rebuildTargets(size, renderScale);
    display_.updateWindowViewport(framePipeline_.getCanvasSize());
}

void SystemImpl::applyPendingDisplayChanges() {
    if (const std::optional<float> configuredScale =
            display_.takeConfiguredScale();
        configuredScale.has_value()) {
        if (const std::optional<float> surfaceScale =
                display_.applyConfiguredScale(*configuredScale);
            surfaceScale.has_value()) {
            rebuildDisplayTargets(*surfaceScale);
        }
    }
    if (display_.takeRenderTargetRebuild()) {
        rebuildDisplayTargets(display_.getSurfaceFitScale());
    }
    if (const std::optional<float> resizeScale =
            display_.observeWindowResize(framePipeline_.getCanvasSize());
        resizeScale.has_value()) {
        rebuildDisplayTargets(*resizeScale);
    }
}

void SystemImpl::onConfigChanged(const std::string& key) {
    if (key == "scale") {
        if (display_.getWindow() != nullptr) {
            display_.requestConfiguredScale(
                SystemConfigBase::getConfiguredScale());
        }
    } else if (key == "maximumRenderScale") {
        if (display_.getWindow() != nullptr) {
            display_.requestRenderTargetRebuild();
        }
    } else if (key == "frameRate") {
        display_.applyFrameRate();
    } else if (key == "verticalSync") {
        display_.applyVerticalSync();
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

SystemImpl& impl() {
    static SystemImpl instance;
    return instance;
}

}  // namespace ludork::global::system_impl
