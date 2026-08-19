#include <System.hpp>

#include "SystemRuntimeAccess.hpp"
#include "Platform/NativeDisplay.hpp"
#include "Platform/NativeInputMethod.hpp"

#include <Input/InputService.hpp>
#include <LudorkPlatform.hpp>
#include <Manager/AssetPath.hpp>
#include <Manager/ShaderManager.hpp>
#include <Manager/TextureManager.hpp>
#include <Runtime/EngineState.hpp>
#include <SystemConfigBase.hpp>
#include <Utils/Inner.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>

using namespace ludork::global::system_runtime;

namespace {

bool viewsEqual(const sf::View& left, const sf::View& right) {
    return left.getCenter() == right.getCenter() &&
           left.getSize() == right.getSize() &&
           left.getRotation() == right.getRotation() &&
           left.getViewport() == right.getViewport() &&
           left.getScissor() == right.getScissor();
}

}  // namespace

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
    windowContextSettings_.antiAliasingLevel =
        static_cast<unsigned int>(getAntiAliasingLevel());
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
