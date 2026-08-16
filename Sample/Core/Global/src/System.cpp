#include <System.hpp>

#include "PerformanceProfiler.hpp"
#include "Platform/NativeDisplay.hpp"
#include "Platform/NativeInputMethod.hpp"

#include <Fog/FogController.hpp>
#include <GlobalRuntimeApi.hpp>
#include <Input/InputService.hpp>
#include <LudorkPlatform.hpp>
#include <Manager/AudioManager.hpp>
#include <Manager/AssetPath.hpp>
#include <Manager/TextureManager.hpp>
#include <Manager/ShaderManager.hpp>
#include <Manager/TimeManager.hpp>
#include <Runtime/EngineState.hpp>
#include <SystemConfigBase.hpp>
#include <Utils/Inner.hpp>
#include <Utils/Render.hpp>
#include <Weather/WeatherController.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

bool viewsEqual(const sf::View& left, const sf::View& right) {
    return left.getCenter() == right.getCenter() &&
           left.getSize() == right.getSize() &&
           left.getRotation() == right.getRotation() &&
           left.getViewport() == right.getViewport() &&
           left.getScissor() == right.getScissor();
}

}  // namespace

std::shared_ptr<sf::RenderWindow> System::window_;
std::mutex System::windowMutex_;
std::unique_ptr<sf::Cursor> System::cursor_;
std::string System::windowTitle_;
std::string System::windowIconPath_;
std::string System::windowCursorPath_;
sf::ContextSettings System::windowContextSettings_;
sf::Vector2u System::observedWindowSize_;
std::optional<sf::Vector2u> System::observedWindowClientSize_;
std::optional<float> System::pendingConfiguredScale_;
std::optional<float> System::pendingResizeScale_;
std::chrono::steady_clock::time_point System::lastResizeTime_;
bool System::desktopFullscreen_ = false;
bool System::inputMethodDisabled_ = true;
bool System::canvasDefaultViewActive_ = true;
std::unique_ptr<sf::RenderTexture> System::canvas_;
std::optional<sf::Sprite> System::canvasSprite_;
std::unique_ptr<sf::RenderTexture> System::transition_;
std::unique_ptr<sf::RenderTexture> System::transitionTempTexture_;
std::unique_ptr<sf::RenderTexture> System::transitionOutputTexture_;
std::unique_ptr<sf::RenderTexture> System::transitionMaskTexture_;
std::optional<sf::Sprite> System::transitionSprite_;
std::optional<sf::Sprite> System::transitionOutputSprite_;
std::vector<std::unique_ptr<sf::RenderTexture>> System::graphicsCanvases_;
std::vector<std::shared_ptr<sf::Shader>> System::graphicsShaders_;
std::shared_ptr<sf::Shader> System::transitionShader_;
std::shared_ptr<sf::Texture> System::transitionResource_;
bool System::inTransition_ = false;
float System::transitionTimeCount_ = 0.0f;
float System::transitionTime_ = 0.0f;
std::size_t System::transitionRevision_ = 0;
std::size_t System::composedTransitionRevision_ = 0;
bool System::transitionCompletionPending_ = false;
bool System::transitionFrozen_ = false;
bool System::transitionFreezePending_ = false;
std::optional<System::PendingTransition> System::pendingTransition_;
std::mutex System::transitionMutex_;
std::mutex System::presentMutex_;

std::shared_ptr<sf::Shader> System::flashShader_;
sf::Glsl::Vec4 System::flashColour_{1.0f, 1.0f, 1.0f, 1.0f};
float System::flashDuration_ = 0.0f;
float System::flashTimeCount_ = 0.0f;
bool System::flashActive_ = false;

std::shared_ptr<sf::Shader> System::toneShader_;
sf::Glsl::Vec4 System::toneCurrentColour_{};
sf::Glsl::Vec4 System::toneStartColour_{};
sf::Glsl::Vec4 System::toneTargetColour_{};
float System::toneDuration_ = 0.0f;
float System::toneTimeCount_ = 0.0f;
bool System::toneActive_ = false;
std::unique_ptr<sf::RenderTexture> System::toneBuffer_;
std::optional<sf::Sprite> System::toneBufferSprite_;

float System::shakePower_ = 0.0f;
float System::shakeSpeed_ = 0.0f;
float System::shakeDuration_ = 0.0f;
float System::shakeTimeCount_ = 0.0f;
bool System::shakeActive_ = false;
sf::Vector2f System::shakeOffset_{};
float System::shakeNextUpdate_ = 0.0f;
std::mt19937 System::random_{std::random_device{}()};

std::vector<std::shared_ptr<SceneRuntime>> System::scenes_;
std::deque<std::shared_ptr<SceneRuntime>> System::retiredScenes_;
std::deque<System::PendingSceneOperation> System::pendingSceneOperations_;
std::mutex System::sceneMutex_;
std::mutex System::pendingSceneMutex_;
std::thread::id System::sceneOperationThread_;
std::function<void()> System::standardUpdate_;
std::atomic_bool System::shuttingDown_ = false;
std::mutex System::lifecycleMutex_;

bool System::debugMode_ = false;

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

void System::initializeDisplay(const std::string& title,
                               const sf::Vector2u& gameSize,
                               const std::string& iconPath,
                               const std::string& cursorPath) {
    if (gameSize.x == 0 || gameSize.y == 0) {
        throw std::invalid_argument("Game size must be non-zero");
    }
    if (window_ != nullptr) {
        throw std::logic_error("Display has already been initialized");
    }
    const ludork::global::RuntimeLaunchOptions& launchOptions =
        ludork::global::runtimeLaunchOptions();
    windowTitle_ = title;
    windowIconPath_ = iconPath;
    windowCursorPath_ = cursorPath;
    windowContextSettings_ = {};
    windowContextSettings_.antiAliasingLevel = isMobileDisplay() ? 0U : 8U;
#if defined(SFML_SYSTEM_IOS)
    windowContextSettings_.majorVersion = 3;
    windowContextSettings_.minorVersion = 0;
#endif
    std::shared_ptr<sf::RenderWindow> window;

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
            windowContextSettings_);
        engineState().setScale(windowFitScale(window->getSize()));
        inputService().setUseInjectedMouseOnly(true);
#else
        throw std::runtime_error(
            "Embedded window mode is only supported on Windows");
#endif
    } else if (isMobileDisplay()) {
        window = std::make_shared<sf::RenderWindow>(
            sf::VideoMode::getDesktopMode(), title, sf::Style::Default,
            sf::State::Fullscreen, windowContextSettings_);
        engineState().setScale(windowFitScale(window->getSize()));
    } else {
        const float configuredScale = getConfiguredScale();
        desktopFullscreen_ = configuredScale == 0.0f;
        const sf::Vector2u windowSize =
            desktopFullscreen_ ? sf::VideoMode::getDesktopMode().size
                               : renderSizeForScale(configuredScale);
        window = std::make_shared<sf::RenderWindow>(
            sf::VideoMode(windowSize), title,
            desktopFullscreen_ ? sf::Style::None : sf::Style::Default,
            sf::State::Windowed, windowContextSettings_);
        const std::optional<sf::Vector2u> clientSize =
            desktopFullscreen_ ? std::nullopt
                               : ludork::global::getWindowedClientSize(
                                     window->getNativeHandle());
        engineState().setScale(
            windowFitScale(clientSize.value_or(window->getSize())));
    }

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
        transitionShader_ = ShaderManager::load("Global/Transition.frag",
                                                sf::Shader::Type::Fragment);
    } else {
        transitionShader_.reset();
        warnOnce("System.transitionShader",
                 "Shaders are unavailable; skipped loading transition shader");
    }
    setInputMethodDisabled(true);
    inputService().initializeNativePolling();
    initCanvas(renderSizeForScale(getScale()));
    observedWindowSize_ = window_->getSize();
    observedWindowClientSize_ =
        desktopFullscreen_
            ? std::nullopt
            : ludork::global::getWindowedClientSize(window_->getNativeHandle());
    updateWindowViewport();
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

void System::initWindow(const std::shared_ptr<sf::RenderWindow>& window) {
    if (window == nullptr) {
        throw std::invalid_argument("System window cannot be nil");
    }
    {
        const std::lock_guard<std::mutex> lock(windowMutex_);
        window_ = window;
    }
    applyWindowPresentationSettings();
}

std::shared_ptr<sf::RenderWindow> System::getWindow() {
    const std::lock_guard<std::mutex> lock(windowMutex_);
    return window_;
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

float System::windowFitScale(const sf::Vector2u& size) {
    const sf::Vector2u gameSize = getGameSize();
    const float scale =
        std::min(static_cast<float>(size.x) / static_cast<float>(gameSize.x),
                 static_cast<float>(size.y) / static_cast<float>(gameSize.y));
    return std::max(0.01f, scale);
}

sf::Vector2u System::renderSizeForScale(float scale) {
    const sf::Vector2u gameSize = getGameSize();
    const float normalizedScale = std::max(0.01f, scale);
    return {
        static_cast<unsigned int>(std::max(
            1.0f,
            std::floor(static_cast<float>(gameSize.x) * normalizedScale))),
        static_cast<unsigned int>(std::max(
            1.0f,
            std::floor(static_cast<float>(gameSize.y) * normalizedScale))),
    };
}

void System::applyWindowPresentationSettings() {
    if (window_ == nullptr) {
        return;
    }
    window_->setFramerateLimit(
        static_cast<unsigned int>(std::max(0, getFrameRate())));
    window_->setVerticalSyncEnabled(getVerticalSync());
    window_->clear(isEmbeddedDisplay() ? sf::Color::Transparent
                                       : sf::Color::Black);
    if (!isMobileDisplay() && !windowIconPath_.empty()) {
        window_->setIcon(sf::Image(windowIconPath_));
    }
    cursor_.reset();
    if (isMobileDisplay() || windowCursorPath_.empty()) {
        return;
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(windowCursorPath_, error)) {
        return;
    }
    try {
        const sf::Image cursorImage(windowCursorPath_);
        cursor_ = std::make_unique<sf::Cursor>(
            cursorImage.getPixelsPtr(), cursorImage.getSize(), sf::Vector2u{});
        window_->setMouseCursor(*cursor_);
    } catch (const std::exception& exception) {
        std::cerr << "Failed to create cursor from " << windowCursorPath_
                  << ": " << exception.what() << '\n';
    }
}

void System::recreateDesktopWindow(bool fullscreen, const sf::Vector2u& size) {
    const std::lock_guard<std::mutex> lock(windowMutex_);
    if (window_ == nullptr || isEmbeddedDisplay() || isMobileDisplay()) {
        return;
    }
    ludork::global::restoreNativeInputMethod();
    window_->create(sf::VideoMode(size), windowTitle_,
                    fullscreen ? sf::Style::None : sf::Style::Default,
                    sf::State::Windowed, windowContextSettings_);
    desktopFullscreen_ = fullscreen;
    if (fullscreen) {
        window_->setPosition({0, 0});
    }
    applyWindowPresentationSettings();
    setInputMethodDisabled(inputMethodDisabled_);
    inputService().onWindowRecreated(*window_);
    inputService().initializeNativePolling();
}

void System::replaceWindowedDesktopWindow(
    const sf::Vector2u& size,
    const ludork::global::WindowedFramePlacement* placement) {
    const std::shared_ptr<sf::RenderWindow> previousWindow = getWindow();
    if (previousWindow == nullptr || isEmbeddedDisplay() || isMobileDisplay()) {
        return;
    }
    ludork::global::restoreNativeInputMethod();
    const std::shared_ptr<sf::RenderWindow> replacement =
        std::make_shared<sf::RenderWindow>(
            sf::VideoMode(size), windowTitle_, sf::Style::Default,
            sf::State::Windowed, windowContextSettings_);
    {
        const std::lock_guard<std::mutex> lock(windowMutex_);
        window_ = replacement;
        desktopFullscreen_ = false;
        if (placement != nullptr) {
            ludork::global::setWindowedFramePlacement(
                window_->getNativeHandle(), *placement);
        }
        applyWindowPresentationSettings();
        setInputMethodDisabled(inputMethodDisabled_);
        inputService().onWindowRecreated(*window_);
        inputService().initializeNativePolling();
    }
}

void System::updateWindowViewport() {
    if (window_ == nullptr || canvas_ == nullptr) {
        return;
    }
    const sf::Vector2u windowSize = window_->getSize();
    const sf::Vector2u renderSize = canvas_->getSize();
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
    window_->setView(view);
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

void System::rebuildDisplayTargets(float scale) {
    const float normalizedScale = std::max(0.01f, scale);
    const sf::Vector2u size = renderSizeForScale(normalizedScale);
    if (canvas_ != nullptr && canvas_->getSize() == size &&
        engineState().getScale() == normalizedScale) {
        updateWindowViewport();
        return;
    }
    std::optional<sf::Image> transitionImage;
    if (transition_ != nullptr) {
        transition_->display();
        transitionImage = transition_->getTexture().copyToImage();
    }
    engineState().setScale(normalizedScale);
    initCanvas(size);
    if (transitionImage.has_value() && transition_ != nullptr) {
        const sf::Texture texture(*transitionImage);
        sf::Sprite sprite(texture);
        const sf::Vector2u sourceSize = texture.getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0) {
            sprite.setScale(
                {static_cast<float>(size.x) / static_cast<float>(sourceSize.x),
                 static_cast<float>(size.y) /
                     static_cast<float>(sourceSize.y)});
            transition_->clear(sf::Color::Transparent);
            transition_->draw(sprite, sf::BlendNone);
            transition_->display();
        }
    }
    if (transitionResource_ != nullptr && transitionMaskTexture_ != nullptr) {
        const sf::Vector2u sourceSize = transitionResource_->getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0) {
            sf::Sprite maskSprite(*transitionResource_);
            maskSprite.setScale(
                {static_cast<float>(size.x) / static_cast<float>(sourceSize.x),
                 static_cast<float>(size.y) /
                     static_cast<float>(sourceSize.y)});
            transitionMaskTexture_->clear(sf::Color::Transparent);
            transitionMaskTexture_->draw(maskSprite, sf::BlendNone);
            transitionMaskTexture_->display();
        }
    }
    updateWindowViewport();
}

void System::applyConfiguredScale(float scale) {
    if (window_ == nullptr || isEmbeddedDisplay() || isMobileDisplay()) {
        return;
    }
    const bool fullscreen = scale == 0.0f;
    sf::Vector2u targetSize = fullscreen ? sf::VideoMode::getDesktopMode().size
                                         : renderSizeForScale(scale);
    std::optional<sf::Vector2u> clientSize;
    if (fullscreen != desktopFullscreen_) {
        recreateDesktopWindow(fullscreen, targetSize);
        if (!fullscreen) {
            clientSize = ludork::global::getWindowedClientSize(
                window_->getNativeHandle());
        }
    } else if (!fullscreen) {
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
        const std::optional<ludork::global::WindowedFramePlacement> placement =
            ludork::global::getWindowedFramePlacement(
                window_->getNativeHandle());
        replaceWindowedDesktopWindow(
            targetSize, placement.has_value() ? &*placement : nullptr);
#else
        window_->setSize(targetSize);
#endif
        clientSize =
            ludork::global::getWindowedClientSize(window_->getNativeHandle());
    }
    observedWindowSize_ = window_->getSize();
    observedWindowClientSize_ = clientSize;
    pendingResizeScale_.reset();
    rebuildDisplayTargets(
        windowFitScale(clientSize.value_or(observedWindowSize_)));
}

void System::observeWindowResize() {
    if (window_ == nullptr) {
        return;
    }
    const sf::Vector2u size = window_->getSize();
    const auto now = std::chrono::steady_clock::now();
    if (size != observedWindowSize_) {
        const std::optional<sf::Vector2u> clientSize =
            desktopFullscreen_ ? std::nullopt
                               : ludork::global::getWindowedClientSize(
                                     window_->getNativeHandle());
        const bool clientSizeChanged =
            !clientSize.has_value() || clientSize != observedWindowClientSize_;
        observedWindowSize_ = size;
        observedWindowClientSize_ = clientSize;
        if (clientSizeChanged) {
            pendingResizeScale_ =
                windowFitScale(clientSize.value_or(observedWindowSize_));
            lastResizeTime_ = now;
        }
        updateWindowViewport();
    }
    if (!pendingResizeScale_.has_value() ||
        now - lastResizeTime_ < std::chrono::milliseconds(150)) {
        return;
    }
    float scale = *pendingResizeScale_;
    pendingResizeScale_.reset();
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
    if (!isEmbeddedDisplay() && !desktopFullscreen_) {
        const std::optional<sf::Vector2u> clientSize =
            ludork::global::getWindowedClientSize(window_->getNativeHandle());
        if (clientSize.has_value()) {
            const std::optional<ludork::global::WindowedFramePlacement>
                placement = ludork::global::getWindowedFramePlacement(
                    window_->getNativeHandle());
            replaceWindowedDesktopWindow(
                *clientSize, placement.has_value() ? &*placement : nullptr);
            observedWindowSize_ = window_->getSize();
            const std::optional<sf::Vector2u> replacedClientSize =
                ludork::global::getWindowedClientSize(
                    window_->getNativeHandle());
            observedWindowClientSize_ = replacedClientSize;
            scale = windowFitScale(replacedClientSize.value_or(*clientSize));
        }
    }
#endif
    rebuildDisplayTargets(scale);
}

void System::applyPendingDisplayChanges() {
    if (pendingConfiguredScale_.has_value()) {
        const float scale = *pendingConfiguredScale_;
        pendingConfiguredScale_.reset();
        applyConfiguredScale(scale);
        return;
    }
    observeWindowResize();
}

void System::setInputMethodDisabled(bool disabled) {
    inputMethodDisabled_ = disabled;
    if (window_ == nullptr ||
        ludork::global::runtimeLaunchOptions().windowMode ==
            ludork::global::RuntimeWindowMode::Embedded) {
        return;
    }
    ludork::global::setNativeInputMethodDisabled(window_->getNativeHandle(),
                                                 disabled);
}

void System::initCanvas(const sf::Vector2u& size) {
    std::optional<sf::View> preservedView;
    if (canvas_ == nullptr) {
        canvas_ = std::make_unique<sf::RenderTexture>(size);
    } else {
        const sf::View currentView = canvas_->getView();
        if (!canvasDefaultViewActive_ ||
            !viewsEqual(currentView, canvas_->getDefaultView())) {
            preservedView = currentView;
        }
        if (canvas_->getSize() != size && !canvas_->resize(size)) {
            throw std::runtime_error("Failed to resize the System canvas");
        }
    }
    canvas_->setView(canvas_->getDefaultView());
    canvas_->clear(sf::Color::Transparent);
    canvas_->setView(preservedView.value_or(canvas_->getDefaultView()));
    if (canvasSprite_.has_value()) {
        canvasSprite_->setTexture(canvas_->getTexture(), true);
    } else {
        canvasSprite_.emplace(canvas_->getTexture());
    }
    transition_ = std::make_unique<sf::RenderTexture>(size);
    transition_->clear(sf::Color::Transparent);
    transition_->display();
    transitionTempTexture_ = std::make_unique<sf::RenderTexture>(size);
    transitionTempTexture_->clear(sf::Color::Transparent);
    transitionOutputTexture_ = std::make_unique<sf::RenderTexture>(size);
    transitionOutputTexture_->clear(sf::Color::Transparent);
    transitionOutputTexture_->display();
    transitionMaskTexture_ = std::make_unique<sf::RenderTexture>(size);
    transitionMaskTexture_->clear(sf::Color::Transparent);
    transitionMaskTexture_->display();
    transitionSprite_.emplace(transitionTempTexture_->getTexture());
    transitionOutputSprite_.emplace(transitionOutputTexture_->getTexture());
    toneBuffer_.reset();
    toneBufferSprite_.reset();
    applyGraphicsShadersLength();
}

void System::clearCanvas() {
    if (window_ != nullptr) {
        window_->clear(isEmbeddedDisplay() ? sf::Color::Transparent
                                           : sf::Color::Black);
    }
    if (canvas_ != nullptr) {
        canvas_->clear(sf::Color::Transparent);
    }
}

void System::setWindowMapView(sf::Vector2f offset) {
    if (canvas_ == nullptr) {
        return;
    }
    const sf::Vector2u gameSize = getGameSize();
    const sf::Vector2f size{static_cast<float>(gameSize.x),
                            static_cast<float>(gameSize.y)};
    const sf::Vector2f center = size / 2.0f - offset;
    canvas_->setView(sf::View(center, size));
    canvasDefaultViewActive_ = false;
}

void System::setWindowDefaultView() {
    if (canvas_ != nullptr) {
        canvas_->setView(canvas_->getDefaultView());
        canvasDefaultViewActive_ = true;
    }
}

sf::RenderTexture* System::getCanvas() {
    return canvas_.get();
}

void System::setWeather(const RuntimeValue& weatherType, float power,
                        int maxCount) {
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
    if (canvas_ == nullptr) {
        return;
    }
    sf::RenderStates states = canvasRenderStates();
    states.shader = shader;
    canvas_->draw(drawable, states);
}

void System::applyScreenTonePass() {
    if (!shadersAvailable() || !toneActive_ || toneShader_ == nullptr ||
        canvas_ == nullptr || isNeutralTone(toneCurrentColour_)) {
        return;
    }
    canvas_->display();
    const sf::Vector2u size = canvas_->getSize();
    ensureToneBuffer(size);
    toneBufferSprite_->setTexture(canvas_->getTexture(), true);
    toneBufferSprite_->setPosition({0.0f, 0.0f});
    toneBufferSprite_->setScale({1.0f, 1.0f});
    toneShader_->setUniform("screenTex", canvas_->getTexture());
    toneShader_->setUniform(
        "texSize",
        sf::Vector2f{static_cast<float>(size.x), static_cast<float>(size.y)});
    applyScreenToneUniform();
    toneBuffer_->clear(sf::Color::Transparent);
    sf::RenderStates toneStates = canvasRenderStates();
    toneStates.shader = toneShader_.get();
    toneBuffer_->draw(*toneBufferSprite_, toneStates);
    toneBuffer_->display();
    const sf::View savedView = canvas_->getView();
    canvas_->clear(sf::Color::Transparent);
    canvas_->setView(canvas_->getDefaultView());
    toneBufferSprite_->setTexture(toneBuffer_->getTexture(), true);
    canvas_->draw(*toneBufferSprite_, canvasRenderStates());
    canvas_->setView(savedView);
}

void System::composeFrame(float deltaTime) {
    transitionCompletionPending_ = false;
    if (window_ == nullptr || canvas_ == nullptr ||
        !canvasSprite_.has_value()) {
        return;
    }
    if (inTransition_) {
        transitionTimeCount_ =
            std::min(transitionTimeCount_ + deltaTime, transitionTime_);
    }
    updateFlash(deltaTime);
    updateScreenTone(deltaTime);
    updateShake(deltaTime);
    updateWeather(deltaTime);
    updateFog(deltaTime);
    applyPendingTransition();
    canvas_->display();
    sf::RenderTexture* finalCanvas = canvas_.get();
    for (std::size_t index = 0; index < graphicsCanvases_.size(); ++index) {
        sf::RenderTexture& target = *graphicsCanvases_[index];
        sf::RenderTexture& source =
            index == 0 ? *canvas_ : *graphicsCanvases_[index - 1];
        target.clear(sf::Color::Transparent);
        sf::Sprite sprite(source.getTexture());
        sf::RenderStates states = canvasRenderStates();
        const std::shared_ptr<sf::Shader>& shader = graphicsShaders_[index];
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
    canvasSprite_->setTexture(finalCanvas->getTexture(), true);
    if (shakeActive_) {
        const sf::Vector2u textureSize = finalCanvas->getSize();
        if (textureSize.x > 0 && textureSize.y > 0) {
            const float pad = shakePower_;
            canvasSprite_->setScale(
                {(static_cast<float>(textureSize.x) + pad * 2.0f) /
                     static_cast<float>(textureSize.x),
                 (static_cast<float>(textureSize.y) + pad * 2.0f) /
                     static_cast<float>(textureSize.y)});
            canvasSprite_->setPosition(
                {-pad + shakeOffset_.x, -pad + shakeOffset_.y});
        }
    }
    if (transitionOutputTexture_ == nullptr ||
        !transitionOutputSprite_.has_value()) {
        window_->draw(*canvasSprite_, canvasRenderStates());
    } else if (inTransition_ && transitionShader_ != nullptr &&
               transition_ != nullptr && transitionTempTexture_ != nullptr &&
               transitionSprite_.has_value()) {
        transitionTempTexture_->clear(sf::Color::Transparent);
        transitionTempTexture_->draw(*canvasSprite_, sf::BlendNone);
        transitionTempTexture_->display();
        transitionShader_->setUniform("screenTex",
                                      transitionTempTexture_->getTexture());
        transitionShader_->setUniform("backTex", transition_->getTexture());
        transitionShader_->setUniform(
            "transitionResource",
            transitionResource_ != nullptr && transitionMaskTexture_ != nullptr
                ? transitionMaskTexture_->getTexture()
                : transition_->getTexture());
        transitionShader_->setUniform("useMask",
                                      transitionResource_ != nullptr &&
                                          transitionMaskTexture_ != nullptr);
        transitionShader_->setUniform("progress", transitionTimeCount_);
        transitionShader_->setUniform("totalTime", transitionTime_);
        sf::RenderStates states(sf::BlendNone);
        states.shader = transitionShader_.get();
        transitionOutputTexture_->clear(sf::Color::Transparent);
        transitionOutputTexture_->draw(*transitionSprite_, states);
        transitionOutputTexture_->display();
        window_->draw(*transitionOutputSprite_, canvasRenderStates());
    } else {
        transitionOutputTexture_->clear(sf::Color::Transparent);
        transitionOutputTexture_->draw(*canvasSprite_, sf::BlendNone);
        transitionOutputTexture_->display();
        window_->draw(*transitionOutputSprite_, canvasRenderStates());
    }
    if (shakeActive_) {
        canvasSprite_->setScale({1.0f, 1.0f});
        canvasSprite_->setPosition({0.0f, 0.0f});
    }
    if (transitionFreezePending_) {
        cacheTransitionBackground();
        transitionFreezePending_ = false;
        transitionFrozen_ = true;
    }
    composedTransitionRevision_ = transitionRevision_;
    transitionCompletionPending_ =
        inTransition_ && transitionTimeCount_ >= transitionTime_;
}

void System::present() {
    const std::lock_guard<std::mutex> lock(presentMutex_);
    if (window_ == nullptr || canvas_ == nullptr ||
        !canvasSprite_.has_value()) {
        return;
    }
#if defined(__APPLE__) && !defined(LUDORK_MOBILE)
    if (!isEmbeddedDisplay() && !desktopFullscreen_) {
        const sf::Vector2u windowSize = window_->getSize();
        const bool liveResizing = ludork::global::isNativeWindowLiveResizing(
            window_->getNativeHandle());
        if (liveResizing || windowSize != observedWindowSize_) {
            const std::optional<sf::Vector2u> clientSize =
                ludork::global::getWindowedClientSize(
                    window_->getNativeHandle());
            if (liveResizing || !clientSize.has_value() ||
                clientSize != observedWindowClientSize_) {
                pendingResizeScale_ =
                    windowFitScale(clientSize.value_or(windowSize));
                lastResizeTime_ = std::chrono::steady_clock::now();
            }
            return;
        }
        if (pendingResizeScale_.has_value()) {
            return;
        }
    }
#endif
    if (PerformanceProfiler::isEnabled()) {
        const auto presentStart = std::chrono::steady_clock::now();
        window_->display();
        PerformanceProfiler::addPresentWait(
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - presentStart)
                .count());
    } else {
        window_->display();
    }
}

void System::completeFrame() {
    if (window_ == nullptr || canvas_ == nullptr ||
        !canvasSprite_.has_value()) {
        transitionCompletionPending_ = false;
        return;
    }
    if (transitionCompletionPending_ &&
        composedTransitionRevision_ == transitionRevision_) {
        inTransition_ = false;
    }
    transitionCompletionPending_ = false;
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
    graphicsShaders_.push_back(shader);
    if (shader != nullptr && uniforms.has_value()) {
        for (const auto& [name, value] : *uniforms) {
            setShaderUniform(*shader, name, value);
        }
    }
    applyGraphicsShadersLength();
}

void System::removeGraphicsShader(const std::shared_ptr<sf::Shader>& shader) {
    const auto iterator =
        std::find(graphicsShaders_.begin(), graphicsShaders_.end(), shader);
    if (iterator != graphicsShaders_.end()) {
        graphicsShaders_.erase(iterator);
    }
    applyGraphicsShadersLength();
}

void System::removeAllGraphicsShaders() {
    graphicsShaders_.clear();
    applyGraphicsShadersLength();
}

void System::removeGraphicsShaderAt(int index) {
    if (index < 0 ||
        static_cast<std::size_t>(index) >= graphicsShaders_.size()) {
        return;
    }
    graphicsShaders_.erase(graphicsShaders_.begin() +
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
    if (flashShader_ == nullptr) {
        try {
            flashShader_ = ShaderManager::load("Global/Flash.frag");
        } catch (const std::exception&) {
            flashShader_.reset();
            std::cerr << "FLASH_SHADER_LOAD_FAILED\n";
            return;
        }
    }
    if (flashShader_ == nullptr) {
        return;
    }
    flashColour_ = sf::Glsl::Vec4(color.value_or(sf::Color::White));
    flashDuration_ = duration;
    flashTimeCount_ = 0.0f;
    if (!flashActive_) {
        addGraphicsShader(flashShader_);
        flashActive_ = true;
    }
    flashShader_->setUniform("flashColor", flashColour_);
    flashShader_->setUniform("intensity", 1.0f);
}

void System::stopFlash() {
    if (flashActive_ && flashShader_ != nullptr) {
        removeGraphicsShader(flashShader_);
    }
    flashActive_ = false;
    flashTimeCount_ = 0.0f;
    flashDuration_ = 0.0f;
}

bool System::isFlashing() {
    return flashActive_;
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
    const sf::Glsl::Vec4 target = makeToneColour(red, green, blue, gray);
    toneStartColour_ = toneCurrentColour_;
    toneTargetColour_ = target;
    toneDuration_ = std::max(0.0f, duration);
    toneTimeCount_ = 0.0f;
    toneActive_ = true;
    if (toneDuration_ <= 0.0f) {
        toneCurrentColour_ = target;
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
    toneCurrentColour_ = {};
    toneStartColour_ = {};
    toneTargetColour_ = {};
    toneDuration_ = 0.0f;
    toneTimeCount_ = 0.0f;
    toneActive_ = false;
}

bool System::isScreenToneActive() {
    return toneActive_;
}

bool System::isScreenToneTransitionComplete() {
    return !toneActive_ || toneDuration_ <= 0.0f;
}

void System::startShake(float power, float speed, float duration) {
    if (duration <= 0.0f) {
        stopShake();
        return;
    }
    shakePower_ = power;
    shakeSpeed_ = speed;
    shakeDuration_ = duration;
    shakeTimeCount_ = 0.0f;
    shakeActive_ = true;
    shakeNextUpdate_ = 0.0f;
    shakeOffset_ = {};
}

void System::stopShake() {
    shakeActive_ = false;
    shakeTimeCount_ = 0.0f;
    shakeDuration_ = 0.0f;
    shakeOffset_ = {};
}

bool System::isShaking() {
    return shakeActive_;
}

void System::cacheTransitionBackground() {
    if (transition_ == nullptr || !transitionOutputSprite_.has_value()) {
        return;
    }
    transition_->clear(sf::Color::Transparent);
    transition_->draw(*transitionOutputSprite_, sf::BlendNone);
    transition_->display();
}

void System::setTransition(
    const std::shared_ptr<sf::Texture>& transitionResource,
    float transitionTime) {
    const std::lock_guard<std::mutex> lock(presentMutex_);
    ++transitionRevision_;
    transitionResource_ = transitionResource;
    if (transitionFreezePending_) {
        cacheTransitionBackground();
        transitionFreezePending_ = false;
        transitionFrozen_ = true;
    }
    if (transitionResource_ != nullptr && transitionMaskTexture_ != nullptr) {
        const sf::Vector2u sourceSize = transitionResource_->getSize();
        const sf::Vector2u targetSize = transitionMaskTexture_->getSize();
        if (sourceSize.x > 0 && sourceSize.y > 0 && targetSize.x > 0 &&
            targetSize.y > 0) {
            sf::Sprite maskSprite(*transitionResource_);
            maskSprite.setScale({static_cast<float>(targetSize.x) /
                                     static_cast<float>(sourceSize.x),
                                 static_cast<float>(targetSize.y) /
                                     static_cast<float>(sourceSize.y)});
            transitionMaskTexture_->clear(sf::Color::Transparent);
            transitionMaskTexture_->draw(maskSprite, sf::BlendNone);
            transitionMaskTexture_->display();
        }
    }
    if (!transitionFrozen_) {
        cacheTransitionBackground();
    } else {
        transitionFrozen_ = false;
    }
    if (transitionShader_ == nullptr) {
        inTransition_ = false;
        return;
    }
    inTransition_ = true;
    transitionTimeCount_ = 0.0f;
    transitionTime_ = std::max(0.0f, transitionTime);
    if (transitionTempTexture_ != nullptr) {
        transitionTempTexture_->clear(sf::Color::Transparent);
    }
}

void System::freezeTransitionBackground() {
    transitionFreezePending_ = true;
}

bool System::isTransitionBackgroundFrozen() {
    return transitionFrozen_;
}

bool System::isTransitionBackgroundFreezePending() {
    return transitionFreezePending_;
}

void System::cancelTransitionBackgroundFreeze() {
    transitionFreezePending_ = false;
    transitionFrozen_ = false;
}

void System::requestTransition(std::optional<std::string> transitionName,
                               float transitionTime) {
    const std::lock_guard<std::mutex> lock(transitionMutex_);
    pendingTransition_ =
        PendingTransition{std::move(transitionName), transitionTime};
}

void System::cancelPendingTransition() {
    const std::lock_guard<std::mutex> lock(transitionMutex_);
    pendingTransition_.reset();
}

bool System::isTransitionPending() {
    const std::lock_guard<std::mutex> lock(transitionMutex_);
    return pendingTransition_.has_value();
}

bool System::isInTransition() {
    return inTransition_;
}

void System::applyPendingTransition() {
    std::optional<PendingTransition> pending;
    {
        const std::lock_guard<std::mutex> lock(transitionMutex_);
        pending.swap(pendingTransition_);
    }
    if (!pending.has_value()) {
        return;
    }
    std::shared_ptr<sf::Texture> resource;
    if (pending->name.has_value() && !pending->name->empty()) {
        resource =
            TextureManager::load(ludork::global::manager::textureAssetFile(
                "Transitions", *pending->name));
    }
    setTransition(resource, pending->time);
}

std::shared_ptr<SceneRuntime> System::getScene() {
    const std::lock_guard<std::mutex> lock(sceneMutex_);
    return scenes_.empty() ? nullptr : scenes_.back();
}

std::shared_ptr<SceneRuntime> System::requireScene() {
    const std::shared_ptr<SceneRuntime> scene = getScene();
    if (scene == nullptr) {
        throw std::runtime_error("No active scene");
    }
    return scene;
}

std::vector<std::shared_ptr<SceneRuntime>> System::getSceneList() {
    const std::lock_guard<std::mutex> lock(sceneMutex_);
    return scenes_;
}

void System::bindSceneOperationThread() {
    const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
    if (shuttingDown_.load()) {
        return;
    }
    sceneOperationThread_ = std::this_thread::get_id();
}

void System::unbindSceneOperationThread() {
    const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
    if (sceneOperationThread_ == std::this_thread::get_id()) {
        sceneOperationThread_ = {};
    }
}

bool System::hasPendingSceneOperations() {
    const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
    return !pendingSceneOperations_.empty();
}

void System::applyPendingSceneReplace() {
    std::deque<PendingSceneOperation> operations;
    {
        const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
        if (shuttingDown_.load()) {
            return;
        }
        if (sceneOperationThread_ != std::thread::id{} &&
            sceneOperationThread_ != std::this_thread::get_id()) {
            return;
        }
        operations.swap(pendingSceneOperations_);
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

void System::requestSceneOperation(SceneOperationType type,
                                   std::shared_ptr<SceneRuntime> scene) {
    bool applyImmediately = false;
    {
        const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
        if (shuttingDown_.load()) {
            return;
        }
        applyImmediately = sceneOperationThread_ == std::thread::id{};
        if (!applyImmediately) {
            pendingSceneOperations_.push_back({type, std::move(scene)});
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
    if (shuttingDown_.load()) {
        return;
    }
    freezeTransitionBackground();
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        if (shuttingDown_.load()) {
            return;
        }
        if (scenes_.empty()) {
            scenes_.push_back(scene);
        } else {
            retiredScenes_.push_back(std::move(scenes_.back()));
            scenes_.back() = scene;
        }
    }
    drainRetiredScenes();
}

void System::applyPushScene(const std::shared_ptr<SceneRuntime>& scene) {
    const std::lock_guard<std::mutex> lock(sceneMutex_);
    if (shuttingDown_.load()) {
        return;
    }
    scenes_.push_back(scene);
}

void System::applyPopScene() {
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        if (shuttingDown_.load()) {
            return;
        }
        if (scenes_.empty()) {
            throw std::logic_error("Cannot pop an empty scene stack");
        }
        retiredScenes_.push_back(std::move(scenes_.back()));
        scenes_.pop_back();
    }
    drainRetiredScenes();
}

void System::applyExit() {
    std::vector<std::shared_ptr<SceneRuntime>> scenes;
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        if (shuttingDown_.load()) {
            return;
        }
        scenes.swap(scenes_);
        for (auto iterator = scenes.rbegin(); iterator != scenes.rend();
             ++iterator) {
            retiredScenes_.push_back(std::move(*iterator));
        }
    }
    drainRetiredScenes();
}

void System::drainRetiredScenes() {
    std::exception_ptr failure;
    while (true) {
        std::shared_ptr<SceneRuntime> scene;
        {
            const std::lock_guard<std::mutex> lock(sceneMutex_);
            if (retiredScenes_.empty() ||
                (retiredScenes_.front() != nullptr &&
                 retiredScenes_.front()->systemIsRunning())) {
                break;
            }
            scene = std::move(retiredScenes_.front());
            retiredScenes_.pop_front();
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

void System::updateFlash(float deltaTime) {
    if (!flashActive_ || flashShader_ == nullptr) {
        return;
    }
    flashTimeCount_ = std::min(flashTimeCount_ + deltaTime, flashDuration_);
    const float intensity =
        flashDuration_ > 0.0f
            ? std::max(0.0f, 1.0f - flashTimeCount_ / flashDuration_)
            : 0.0f;
    flashShader_->setUniform("flashColor", flashColour_);
    flashShader_->setUniform("intensity", intensity);
    if (flashTimeCount_ >= flashDuration_) {
        removeGraphicsShader(flashShader_);
        flashActive_ = false;
    }
}

void System::updateScreenTone(float deltaTime) {
    if (!toneActive_ || toneShader_ == nullptr) {
        return;
    }
    if (toneDuration_ > 0.0f) {
        toneTimeCount_ = std::min(toneTimeCount_ + deltaTime, toneDuration_);
        const float ratio = std::min(1.0f, toneTimeCount_ / toneDuration_);
        toneCurrentColour_ = {
            toneStartColour_.x +
                (toneTargetColour_.x - toneStartColour_.x) * ratio,
            toneStartColour_.y +
                (toneTargetColour_.y - toneStartColour_.y) * ratio,
            toneStartColour_.z +
                (toneTargetColour_.z - toneStartColour_.z) * ratio,
            toneStartColour_.w +
                (toneTargetColour_.w - toneStartColour_.w) * ratio};
    }
    applyScreenToneUniform();
    if (toneDuration_ > 0.0f && toneTimeCount_ >= toneDuration_) {
        toneDuration_ = 0.0f;
        if (isNeutralTone(toneCurrentColour_)) {
            stopScreenTone();
        }
    }
}

void System::updateShake(float deltaTime) {
    if (!shakeActive_) {
        return;
    }
    shakeTimeCount_ = std::min(shakeTimeCount_ + deltaTime, shakeDuration_);
    if (shakeTimeCount_ >= shakeDuration_) {
        stopShake();
        return;
    }
    const float remainingPower =
        shakePower_ * (1.0f - shakeTimeCount_ / shakeDuration_);
    shakeNextUpdate_ -= deltaTime;
    if (shakeNextUpdate_ <= 0.0f) {
        shakeNextUpdate_ = shakeSpeed_ > 0.0f
                               ? 1.0f / shakeSpeed_
                               : std::numeric_limits<float>::max();
        std::uniform_real_distribution<float> offset(-remainingPower,
                                                     remainingPower);
        shakeOffset_ = {offset(random_), offset(random_)};
    }
}

bool System::ensureToneShader() {
    if (toneShader_ != nullptr) {
        return true;
    }
    try {
        toneShader_ = ShaderManager::load("Global/Tone.frag");
    } catch (const std::exception&) {
        toneShader_.reset();
        std::cerr << "TONE_SHADER_LOAD_FAILED\n";
        return false;
    }
    return toneShader_ != nullptr;
}

void System::applyScreenToneUniform() {
    if (toneShader_ != nullptr) {
        toneShader_->setUniform("toneColor", toneCurrentColour_);
    }
}

void System::ensureToneBuffer(const sf::Vector2u& size) {
    if (toneBuffer_ == nullptr || toneBuffer_->getSize() != size) {
        toneBuffer_ = std::make_unique<sf::RenderTexture>(size);
        toneBufferSprite_.emplace(toneBuffer_->getTexture());
    } else if (!toneBufferSprite_.has_value()) {
        toneBufferSprite_.emplace(toneBuffer_->getTexture());
    }
}

sf::Glsl::Vec4 System::makeToneColour(float red, float green, float blue,
                                      float gray) {
    return {std::clamp(red, -255.0f, 255.0f) / 255.0f,
            std::clamp(green, -255.0f, 255.0f) / 255.0f,
            std::clamp(blue, -255.0f, 255.0f) / 255.0f,
            std::clamp(gray, 0.0f, 255.0f) / 255.0f};
}

bool System::isNeutralTone(const sf::Glsl::Vec4& toneColour) {
    return std::abs(toneColour.x) <= 0.0001f &&
           std::abs(toneColour.y) <= 0.0001f &&
           std::abs(toneColour.z) <= 0.0001f &&
           std::abs(toneColour.w) <= 0.0001f;
}

void System::applyGraphicsShadersLength() {
    while (graphicsCanvases_.size() > graphicsShaders_.size()) {
        graphicsCanvases_.pop_back();
    }
    if (canvas_ == nullptr) {
        return;
    }
    const sf::Vector2u size = canvas_->getSize();
    while (graphicsCanvases_.size() < graphicsShaders_.size()) {
        graphicsCanvases_.push_back(std::make_unique<sf::RenderTexture>(size));
    }
    for (std::unique_ptr<sf::RenderTexture>& graphicsCanvas :
         graphicsCanvases_) {
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
